#include "icar.hpp"
#include "logger.hpp"
#include <queue>
#include <ranges>
#include <string>

namespace geyser::icar
{

result icar::run( const transition_system& system )
{
    _system = &system;
    initialize();

    return check();
}

void icar::initialize()
{
    push_frame();

    _init_negated = formula_as_cube( _system->init() ).negate();

    const auto activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    const auto activated_error = _system->error().activate( _error_activator.var() );

    // In iCAR, the basic solver is used even for the error formula, since
    // we cannot activate it permanently either way.
    _basic_solver.assert_formula( activated_init );
    _basic_solver.assert_formula( activated_error );

    _trans_solver.assert_formula( activated_init );
    _trans_solver.assert_formula( _system->trans() );
}

void icar::add_blocked_to_solver( bad_cube_handle h, literal act )
{
    const auto& c = _cotrace.get( h ).state_vars();

    auto big = std::vector< literal >{};

    big.reserve( c.literals().size() + 1 );

    for ( const auto lit : c.literals() )
        big.push_back( !lit );

    big.push_back( act );

    auto cnf = cnf_formula::clause( big );

    for ( const auto lit : c.literals() )
        cnf.add_clause( !act, lit );

    _basic_solver.assert_formula( cnf );
}

result icar::check()
{
    while ( true )
    {
        if ( const auto s = get_error_state(); s.has_value() )
        {
            if ( const auto cex = solve_obligation( proof_obligation{ *s, depth() } ); cex.has_value() )
                return *cex;
        }
        else
        {
            push_frame();

            if ( propagate() )
                return ok{};

            if ( is_inductive() )
                return ok{};

            log_trace_content();
            log_cotrace_content();
        }
    }
}

std::optional< bad_cube_handle > icar::get_error_state()
{
    assert( depth() < _trace_activators.size() );

    auto constraint = std::vector< literal >{};
    constraint.reserve( _cotrace_found_cubes.size() + 1 );

    constraint.push_back( _error_activator );

    for ( const auto& [ _, act ] : _cotrace_found_cubes )
        constraint.push_back( act );

    if ( _basic_solver.query()
         .assume( _trace_activators[ depth() ] )
         .constrain_clause( constraint )
         .is_sat() )
    {
        // Find the bad cube that was used. The first activator is special as
        // it denotes a new error state. A new error state (i.e. a state
        // satisfying E) is not added to the cotrace, as that is handled by
        // the special error activator only.

        const auto handle = [ & ]
        {
            for ( std::size_t i = 1; i < constraint.size(); ++i )
                if ( _basic_solver.is_true_in_model( constraint[ i ].var() ) )
                    return _cotrace_found_cubes[ i - 1 ].first;

            return _cotrace.make( cube{ _basic_solver.get_model( _system->state_vars() ) },
                                  cube{ _basic_solver.get_model( _system->input_vars() ) } );
        }();

        return handle;
    }

    return {};
}

std::optional< counterexample > icar::solve_obligation( const proof_obligation& starting_po )
{
    assert( 0 <= starting_po.level() && starting_po.level() <= depth() );

    auto min_queue = std::priority_queue< proof_obligation,
            std::vector< proof_obligation >, std::greater<> >{};

    min_queue.push( starting_po );

    while ( !min_queue.empty() )
    {
        const auto po = min_queue.top();
        min_queue.pop();

        if ( po.level() == 0 )
            return build_counterexample( po.handle() );

        if ( is_already_blocked( po ) )
            continue;

        if ( has_predecessor( _cotrace.get( po.handle() ).state_vars().literals(), po.level() ) )
        {
            const auto pred = get_predecessor( po );

            logger::log_line_debug( "B: {}", cube_to_string( _cotrace.get( pred ).state_vars() ) );
            add_reaching( pred );

            min_queue.emplace( pred, po.level() - 1 );
            min_queue.push( po );
        }
        else
        {
            auto c = generalize_blocked( po );

            logger::log_line_debug( "F[{}]: {}", po.level(), cube_to_string( c ) );
            add_blocked_at( c, po.level() );

            // We don't repush here.
        }
    }

    return {};
}

counterexample icar::build_counterexample( bad_cube_handle initial )
{
    logger::log_line_loud( "Found a counterexample at k = {}", depth() );

    auto get_vars = []( variable_range range, const cube& val )
    {
        auto row = valuation{};
        row.reserve( range.size() );

        for ( const auto var : range )
            row.push_back( val.find( var ).value_or( literal{ var, true } ) );

        return row;
    };

    auto entry = std::optional{ _cotrace.get( initial ) };

    auto initial_state = get_vars( _system->state_vars(), entry->state_vars() );

    auto inputs = std::vector< valuation >{};
    inputs.reserve( depth() );

    while ( entry.has_value() )
    {
        inputs.emplace_back( get_vars( _system->input_vars(), entry->input_vars() ) );
        entry = entry->successor().transform( [ & ]( bad_cube_handle h ){ return _cotrace.get( h ); } );
    }

    return counterexample{ std::move( initial_state ), std::move( inputs ) };
}

bool icar::is_already_blocked( const proof_obligation& po )
{
    assert( 1 <= po.level() && po.level() <= depth() );

    const auto& s = _cotrace.get( po.handle() ).state_vars();

    for ( const auto& c : _trace_blocked_cubes[ po.level() ] )
        if ( c.subsumes( s ) )
            return true;

    return !_basic_solver.query()
            .assume( _trace_activators[ po.level() ] )
            .assume( s.literals() )
            .is_sat();
}

bool icar::has_predecessor( std::span< const literal > s, int i )
{
    assert( i >= 1 );

    return _trans_solver.query()
            .assume( _trace_activators[ i - 1 ] )
            .assume_mapped( s, [ & ]( literal l ){ return _system->prime( l ); } )
            .is_sat();
}

bad_cube_handle icar::get_predecessor( const proof_obligation& po )
{
    const auto& s = _cotrace.get( po.handle() ).state_vars().literals();
    auto ins = _trans_solver.get_model( _system->input_vars() );
    auto p = _trans_solver.get_model( _system->state_vars() );

    [[maybe_unused]]
    const auto sat = _trans_solver.query()
            .constrain_not_mapped( s, [ & ]( literal l ){ return _system->prime( l ); } )
            .assume( ins )
            .assume( p )
            .is_sat();

    assert( !sat );

    return _cotrace.make( cube{ _trans_solver.get_core( p ) }, cube{ std::move( ins ) }, po.handle() );
}

cube icar::generalize_blocked( const proof_obligation& po )
{
    auto core = std::vector< literal >{};

    for ( const auto lit : _cotrace.get( po.handle() ).state_vars().literals() )
    {
        const auto primed = _system->prime( lit );

        if ( _trans_solver.is_in_core( primed ) )
            core.push_back( primed );
    }

    const auto thunk = [ & ]( std::span< const literal > assumptions )
    {
        assert( is_next_state_cube( assumptions ) );

        return _trans_solver.query()
                .assume( _trace_activators[ po.level() - 1 ] )
                .assume( assumptions )
                .is_sat();
    };

    core = get_minimal_core( _trans_solver, core, thunk );

    for ( auto& lit : core )
        lit = _system->unprime( lit );

    return cube{ std::move( core ) };
}

std::vector< literal > icar::get_minimal_core( solver& solver, std::span< const literal > seed,
                                               std::invocable< std::span< const literal > > auto requery )
{
    auto core = std::vector< literal >( seed.begin(), seed.end() );
    const auto lits = core;

    for ( const auto lit : lits )
    {
        core.erase( std::remove( core.begin(), core.end(), lit ), core.end() );

        if ( requery( core ) )
            core.push_back( lit );
        else
            core = solver.get_core( core );
    }

    return core;
}

void icar::add_reaching( bad_cube_handle h )
{
    if ( !_opts.enable_cotrace() )
        return;

    const auto& res = _cotrace_found_cubes.emplace_back( h, literal{ _store->make() } );
    add_blocked_to_solver( res.first, res.second );
}

void icar::add_blocked_at( const cube& c, int level )
{
    assert( 1 <= level && level <= depth() );
    assert( is_state_cube( c.literals() ) );

    auto& frame = _trace_blocked_cubes[ level ];

    for ( std::size_t i = 0; i < frame.size(); )
    {
        if ( c.subsumes( frame[ i ] ) )
        {
            frame[ i ] = frame.back();
            frame.pop_back();
        }
        else
            ++i;
    }

    assert( level < _trace_activators.size() );

    frame.emplace_back( c );

    const auto activated = c.negate().activate( _trace_activators[ level ].var() );

    _basic_solver.assert_formula( activated );
    _trans_solver.assert_formula( activated );
}

bool icar::propagate()
{
    assert( _trace_blocked_cubes[ depth() ].empty() );

    for ( int i = 1; i < depth(); ++i )
    {
        auto pushed_all = true;

        for ( const auto &c : _trace_blocked_cubes[ i ] )
        {
            if ( has_predecessor( c.literals(), i + 1 ) )
            {
                pushed_all = false;
            }
            else
            {
                // has_predecessor queries with c primed
                auto core = std::vector< literal >{};

                for ( const auto lit : c.literals() )
                    if ( _trans_solver.is_in_core( _system->prime( lit ) ) )
                        core.push_back( lit );

                add_blocked_at( cube{ core }, i + 1 );
            }
        }

        if ( pushed_all )
            return true;
    }

    return false;
}

bool icar::is_inductive()
{
    assert( 1 <= depth() );

    const auto clausify_frame_negation = [ & ]( const cube_set& cubes )
    {
        const auto x = literal{ _store->make() };

        auto cnf = cnf_formula{};

        cnf.add_clause( x );

        auto ys = std::vector< literal >{};

        for ( const auto& c : cubes )
        {
            const auto y = ys.emplace_back( _store->make() );

            for ( const auto lit : c.literals() )
                cnf.add_clause( !y, lit );

            auto clause = std::vector< literal >{};
            clause.reserve( c.literals().size() + 1 );

            for ( const auto lit : c.literals() )
                clause.push_back( !lit );

            clause.push_back( y );

            cnf.add_clause( clause );
        }

        auto clause = std::vector< literal >{};
        clause.reserve( ys.size() + 1 );

        clause.push_back( !x );

        for ( const auto y : ys )
            clause.push_back( y );

        cnf.add_clause( clause );

        for ( const auto y : ys )
            cnf.add_clause( !y, x );

        return cnf;
    };

    auto checker = solver{};

    checker.assert_formula( _init_negated );

    for ( int i = 1; i <= depth(); ++i )
    {
        const auto act = literal{ _store->make() };

        for ( const auto& c : _trace_blocked_cubes[ i ] )
            checker.assert_formula( c.negate().activate( act.var() ) );

        if ( !checker.query().assume( act ).is_sat() )
            return true;

        if ( i < depth() )
        {
            checker.assert_formula( cnf_formula::clause( std::vector{ !act } ) );
            checker.assert_formula( clausify_frame_negation( _trace_blocked_cubes[ i ] ) );
        }
    }

    return false;
}

bool icar::is_state_cube( std::span< const literal > literals ) const
{
    const auto is_state_var = [ & ]( variable var )
    {
        const auto [ type, _ ] = _system->get_var_info( var );
        return type == var_type::state;
    };

    return std::ranges::all_of( literals, [ & ]( literal lit ){ return is_state_var( lit.var() ); } );
}

bool icar::is_next_state_cube( std::span< const literal > literals ) const
{
    const auto is_next_state_var = [ & ]( variable var )
    {
        const auto [ type, _ ] = _system->get_var_info( var );
        return type == var_type::next_state;
    };

    return std::ranges::all_of( literals, [ & ]( literal lit ){ return is_next_state_var( lit.var() ); } );
}


void icar::log_trace_content() const
{
    auto line = std::format( "{} F:", depth() );

    for ( int i = 1; i <= depth(); ++i )
        line += std::format( " {}", _trace_blocked_cubes[ i ].size() );

    logger::log_line_loud( "{}", line );
}

void icar::log_cotrace_content() const
{
    logger::log_line_loud( "{} B: {}", depth(), _cotrace_found_cubes.size() );
}

} // namespace geyser::icar