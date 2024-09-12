#include "car.hpp"
#include "logger.hpp"
#include <queue>
#include <ranges>
#include <string>

namespace geyser::car
{

result car::run( const transition_system& system )
{
    _system = &system;
    initialize();

    return check();
}

void car::initialize()
{
    push_frame();
    push_coframe();

    _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    _activated_trans = _system->trans().activate( _transition_activator.var() );
    _activated_error = _system->error().activate( _error_activator.var() );

    // In the backward mode, the initial formula is not in general a cube.
    if ( _forward )
        _init_negated = formula_as_cube( _system->init() ).negate();
    else
        _init_negated = negate_cnf( _system->init() );
}

void car::refresh_solver()
{
    logger::log_line_loud( "Refreshing the solver after {} queries", _queries );

    assert( _system );

    _solver.reset();
    _queries = 0;

    _solver.assert_formula( _activated_init );
    _solver.assert_formula( _activated_trans );
    _solver.assert_formula( _activated_error );

    for ( const auto& [ cubes, act ] : std::views::zip( _trace_blocked_cubes, _trace_activators ) )
        for ( const auto& cube : cubes )
            _solver.assert_formula( cube.negate().activate( act.var() ) );
}

result car::check()
{
    while ( true )
    {
        // Let's first look at the states we already stored in the cotrace.
        if ( const auto cex = check_existing_cotrace(); cex.has_value() )
            return *cex;

        // Now try to extend the first coframe by new error states.
        if ( const auto cex = check_new_error_states(); cex.has_value() )
            return *cex;

        push_frame();

        if ( propagate() )
            return ok{};

        if ( is_inductive() )
            return ok{};

        log_trace_content();
        log_cotrace_content();
    }
}

std::optional< counterexample > car::check_existing_cotrace()
{
    // Iterate through the cotrace in reverse, i.e. from states that are
    // furthest from the error states (at least as far as we know at the
    // moment). This is a heuristic which is used by SimpleCAR in the default
    // mode. Note that we don't check here whether cubes in the cotrace still
    // have a non-empty intersection with the current frame, since that will
    // be checked by is_already_blocked in solve_obligation.

    for ( int j = codepth(); j >= 0; --j )
        for ( const auto handle : _cotrace_found_cubes[ j ] )
            if ( const auto cex = solve_obligation( proof_obligation{ handle, depth(), j } ); cex.has_value() )
                return cex;

    return {};
}

std::optional< counterexample > car::check_new_error_states()
{
    auto handle = get_error_state();

    while ( handle.has_value() )
    {
        if ( const auto cex = solve_obligation( proof_obligation{ *handle, depth(), 0 } ); cex.has_value() )
            return cex;

        handle = get_error_state();
    }

    return {};
}

std::optional< bad_cube_handle > car::get_error_state()
{
    assert( depth() < _trace_activators.size() );

    if ( with_solver()
         .assume( _trace_activators[ depth() ] )
         .assume( _error_activator )
         .is_sat() )
    {
        // TODO: Extend the state as in predecessor generalization here?

        const auto handle = _cotrace.make( cube{ _solver.get_model( _system->state_vars() ) },
                                           cube{ _solver.get_model( _system->input_vars() ) });

        add_reaching_at( handle, 0 );
        return handle;
    }

    return {};
}

std::optional< counterexample > car::solve_obligation( const proof_obligation& starting_po )
{
    assert( 0 <= starting_po.level() && starting_po.level() <= depth() );
    assert( 0 <= starting_po.colevel() && starting_po.colevel() <= codepth() );

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
            const auto ix = po.colevel() + 1;

            logger::log_line_debug( "B[{}]: {}", ix, cube_to_string( _cotrace.get( pred ).state_vars() ) );
            add_reaching_at( pred, ix );

            min_queue.emplace( pred, po.level() - 1, ix );
            min_queue.push( po );
        }
        else
        {
            auto c = generalize_blocked( po );

            logger::log_line_debug( "F[{}]: {}", po.level(), cube_to_string( c ) );
            add_blocked_at( c, po.level() );

            // TODO: Is this good in CAR?
            // if ( po.level() < depth() )
            //     min_queue.emplace( po.handle(), po.level() + 1, po.colevel() );
        }
    }

    return {};
}

counterexample car::build_counterexample( bad_cube_handle initial )
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
    auto previous = std::optional< bad_cube >{};

    auto inputs = std::vector< valuation >{};
    inputs.reserve( depth() );

    while ( entry.has_value() )
    {
        inputs.emplace_back( get_vars( _system->input_vars(), entry->input_vars() ) );
        previous = entry;
        entry = entry->successor().transform( [ & ]( bad_cube_handle h ){ return _cotrace.get( h ); } );
    }

    if ( _forward )
    {
        auto first = get_vars( _system->state_vars(), _cotrace.get( initial ).state_vars() );

        return counterexample{ std::move( first ), std::move( inputs ) };
    }
    else
    {
        // Handle initial actually points to the terminal state of the
        // counterexample and the real initial state is the penultimate entry.

        assert( previous.has_value() );
        auto first = get_vars( _system->state_vars(), previous->state_vars() ); // NOLINT

        // The last element of the inputs is not needed, since the initial
        // formula depends only on the state variables, not inputs. Unlike it,
        // the error formula does depend on input variable valuation, so we
        // must add the inputs of the terminal state after reversing instead.

        inputs.pop_back();
        std::ranges::reverse( inputs );
        inputs.emplace_back( get_vars( _system->input_vars(), _cotrace.get( initial ).input_vars() ) );

        return counterexample{ std::move( first ), std::move( inputs ) };
    }
}

bool car::is_already_blocked( const proof_obligation& po )
{
    assert( 1 <= po.level() && po.level() <= depth() );

    const auto& s = _cotrace.get( po.handle() ).state_vars();

    for ( const auto& c : _trace_blocked_cubes[ po.level() ] )
        if ( c.subsumes( s ) )
            return true;

    return !with_solver()
            .assume( _trace_activators[ po.level() ] )
            .assume( s.literals() )
            .is_sat();
}

// Given a state s in R_i, check whether it has a predecessor in R_{i - 1},
// i.e. whether the formula R_{i - 1} /\ T /\ s' is satisfiable.
bool car::has_predecessor( std::span< const literal > s, int i )
{
    assert( i >= 1 );

    return with_solver()
            .assume( _trace_activators[ i - 1 ] )
            .assume( _transition_activator )
            .assume_mapped( s, [ & ]( literal l ){ return _system->prime( l ); } )
            .is_sat();
}

bad_cube_handle car::get_predecessor( const proof_obligation& po )
{
    auto ins = _solver.get_model( _system->input_vars() );
    auto p = _solver.get_model( _system->state_vars() );

    if ( _forward )
    {
        const auto& s = _cotrace.get( po.handle() ).state_vars().literals();

        const auto query = [ & ]( std::span< const literal > assumptions )
        {
            assert( is_state_cube( assumptions ) );

            return with_solver()
                    .constrain_not_mapped( s, [ & ]( literal l ){ return _system->prime( l ); } )
                    .assume( _transition_activator )
                    .assume( ins )
                    .assume( assumptions )
                    .is_sat();
        };

        [[maybe_unused]]
        const auto sat = query( p );
        assert( !sat );

        auto core = _solver.get_core( p );

        return _cotrace.make( cube{ get_minimal_core( std::move( core ), query ) },
                              cube{ std::move( ins ) }, po.handle() );
    }
    else
    {
        // We cannot generalize in the backward mode.
        return _cotrace.make( cube{ std::move( p ) }, cube{ std::move( ins ) }, po.handle() );
    }
}

cube car::generalize_blocked( const proof_obligation& po )
{
    auto core = _solver.get_core( _system->next_state_vars() );

    const auto thunk = [ & ]( std::span< const literal > assumptions )
    {
        assert( is_next_state_cube( assumptions ) );

        return with_solver()
                .assume( _trace_activators[ po.level() - 1 ] )
                .assume( _transition_activator )
                .assume( assumptions )
                .is_sat();
    };

    auto minimal = get_minimal_core( core, thunk );

    for ( auto& lit : minimal )
        lit = _system->unprime( lit );

    return cube{ minimal };
}

std::vector< literal > car::get_minimal_core( std::span< const literal > seed,
                                              std::invocable< std::span< const literal > > auto requery )
{
    auto core = std::vector< literal >( seed.begin(), seed.end() );
    const auto lits = core;

    // TODO: Make this more efficient?

    for ( const auto lit : lits )
    {
        core.erase( std::remove( core.begin(), core.end(), lit ), core.end() );

        if ( requery( core ) )
            core.push_back( lit );
        else
            core = _solver.get_core( core );
    }

    return core;
}

void car::add_reaching_at( bad_cube_handle h, int level )
{
    assert( 0 <= level );

    while ( codepth() < level )
        push_coframe();

    const auto& c = _cotrace.get( h ).state_vars();
    auto& coframe = _cotrace_found_cubes[ level ];

    for ( std::size_t i = 0; i < coframe.size(); )
    {
        if ( c.subsumes( _cotrace.get( coframe[ i ] ).state_vars() ) )
        {
            coframe[ i ] = coframe.back();
            coframe.pop_back();
        }
        else
            ++i;
    }

    coframe.emplace_back( h );
}

void car::add_blocked_at( const cube& c, int level )
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
    _solver.assert_formula( c.negate().activate( _trace_activators[ level ].var() ) );
}

bool car::propagate()
{
    assert( _trace_blocked_cubes[ depth() ].empty() );

    for ( int i = 1; i < depth(); ++i )
    {
        auto pushed_all = true;

        for ( const auto &c : _trace_blocked_cubes[ i ] )
        {
            // TODO: Get the unsat core in the false branch, as in the paper?

            if ( has_predecessor( c.literals(), i + 1 ) )
                pushed_all = false;
            else
                add_blocked_at( c, i + 1 );
        }

        if ( pushed_all )
            return true;
    }

    return false;
}

bool car::is_inductive()
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

bool car::is_state_cube( std::span< const literal > literals ) const
{
    const auto is_state_var = [ & ]( variable var )
    {
        const auto [ type, _ ] = _system->get_var_info( var );
        return type == var_type::state;
    };

    return std::ranges::all_of( literals, [ & ]( literal lit ){ return is_state_var( lit.var() ); } );
}

bool car::is_next_state_cube( std::span< const literal > literals ) const
{
    const auto is_next_state_var = [ & ]( variable var )
    {
        const auto [ type, _ ] = _system->get_var_info( var );
        return type == var_type::next_state;
    };

    return std::ranges::all_of( literals, [ & ]( literal lit ){ return is_next_state_var( lit.var() ); } );
}

void car::log_trace_content() const
{
    auto line = std::format( "{} F:", depth() );

    for ( int i = 1; i <= depth(); ++i )
        line += std::format( " {}", _trace_blocked_cubes[ i ].size() );

    logger::log_line_loud( "{}", line );
}

void car::log_cotrace_content() const
{
    auto line = std::format( "{} B:", depth() );

    for ( int i = 1; i <= codepth(); ++i )
        line += std::format( " {}", _cotrace_found_cubes[ i ].size() );

    logger::log_line_loud( "{}", line );
}

cnf_formula car::negate_cnf( const cnf_formula& f )
{
    // Given a CNF formula f such as
    //   (a \/ -b \/ c) /\ (-c \/ d) /\ (a \/ b \/ c),
    // we can do a trivial Tseiting encoding of it by adding a new variable x
    // for the whole formula and y1, ..., yn for the individual clauses. First,
    // add constraints for each clause, e.g.
    //   y1 -> (a \/ -b \/ c) = -y1 \/ a \/ -b \/ c
    //   a -> y1, -b -> y1, c -> y1 = -a \/ y1, -b \/ y1, c \/ y1
    // Then add constraints specifying x <-> cy /\ ... /\ cy, e.g.
    //   x -> y1, x -> y2, x -> y3 = -x \/ y1, -x \/ y2, -x \/ y3
    //   y1 /\ y2 /\ y3 -> x = -y1 \/ -y2 \/ -y3 \/ x
    // Finally add -x, saying that the whole formula is false.
    // (Yes, this is almost the same as clausify_frame_negation above, but the
    // negations are the other way around, sigh...)
    // TODO: Somehow deduplicate.

    auto negation = cnf_formula{};
    const auto x = literal{ _store->make() };

    negation.add_clause( !x );

    // If only we had C++23 ranges::to.

    auto split = f.literals()
               | std::views::split( literal::separator )
               | std::views::transform( []( const auto& subrange )
                 {
                   return std::vector< literal >( subrange.begin(), subrange.end() );
                 } );

    const auto clauses = std::vector( split.begin(), split.end() );

    auto ys = std::vector< literal >{};

    for ( const auto& c : clauses )
    {
        const auto y = ys.emplace_back( _store->make() );

        for ( const auto lit : c )
            negation.add_clause( !lit, y );

        auto clause = std::vector< literal >{};

        clause.reserve( c.size() + 1 );
        clause.push_back( !y );

        for ( const auto lit : c )
            clause.push_back( lit );

        negation.add_clause( clause );
    }

    for ( const auto y : ys )
        negation.add_clause( !x, y );

    auto clause = std::vector< literal >{};
    clause.reserve( ys.size() + 1 );

    for ( const auto y : ys )
        clause.push_back( !y );

    clause.push_back( x );

    negation.add_clause( clause );

    return negation;
}

transition_system backward_car::reverse_system( const transition_system& system )
{
    const auto reversed_trans = system.trans().map( [ & ]( literal lit )
    {
        const auto [ type, pos ] = system.get_var_info( lit.var() );

        if ( type == var_type::state )
            return system.prime( lit );
        if ( type == var_type::next_state )
            return system.unprime( lit );

        return lit;
    } );

    return transition_system{ system.input_vars(), system.state_vars(), system.next_state_vars(),
                              system.aux_vars(), system.error(), reversed_trans, system.init() };
}

} // namespace geyser::car