#include "pdr.hpp"
#include "logger.hpp"
#include <queue>
#include <ranges>
#include <string>

namespace geyser::pdr
{

result pdr::run( const transition_system& system )
{
    _system = &system;
    initialize();

    return check();
}

void pdr::initialize()
{
    push_frame();

    _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    _activated_trans = _system->trans().activate( _transition_activator.var() );
    _activated_error = _system->error().activate( _error_activator.var() );

    // We assume that initial states are given as a single cube. This is the
    // case when no invariant constraints are present in the Aiger input, which
    // we assume for the sake of simplicity of implementation.

    _init_cube = formula_as_cube( _system->init() );
}

result pdr::check()
{
    while ( true )
    {
        if ( const auto cti = get_error_cti(); cti.has_value() )
        {
            if ( const auto cex = solve_obligation( proof_obligation{ *cti, depth() } ); cex.has_value() )
                return *cex;
        }
        else
        {
            push_frame();

            if ( propagate() )
                return ok{};
        }

        _ctis.flush();
    }
}

// Returns a cti handle to a state which reaches error under given
// input variable values.
std::optional< cti_handle > pdr::get_error_cti()
{
    if ( with_solver()
         .assume( activators_from( depth() ) )
         .assume( _error_activator )
         .is_sat() )
        return _ctis.make( cube{ _solver.get_model( _system->state_vars() ) },
                           cube{ _solver.get_model( _system->input_vars() ) } );

    return {};
}

std::optional< counterexample > pdr::solve_obligation( const proof_obligation& starting_po )
{
    assert( 0 <= starting_po.level() && starting_po.level() <= depth() );

    auto min_queue = std::priority_queue< proof_obligation,
        std::vector< proof_obligation >, std::greater<> >{};

    min_queue.push( starting_po );

    while ( !min_queue.empty() )
    {
        auto po = min_queue.top();
        min_queue.pop();

        if ( po.level() == 0 )
            return build_counterexample( po.handle() );

        if ( is_already_blocked( po ) )
            continue;

        assert( !intersects_initial_states( _ctis.get( po.handle() ).state_vars().literals() ) );

        if ( is_relative_inductive( _ctis.get( po.handle() ).state_vars().literals(), po.level() ) )
        {
            const auto [ c, i ] = generalize_inductive( po );

            logger::log_line_debug( "{}: {}", i, cube_to_string( c ) );
            add_blocked_at( c, i );

            if ( po.level() < depth() )
                min_queue.emplace( po.handle(), po.level() + 1 );
        }
        else
        {
            min_queue.emplace( get_predecessor( po ), po.level() - 1 );
            min_queue.push( po );
        }
    }

    return {};
}

// Fetch a generalized predecessor of a proof obligation po from a model of
// the last relative inductive check.
cti_handle pdr::get_predecessor( const proof_obligation& po )
{
    const auto& s = _ctis.get( po.handle() ).state_vars().literals();
    auto ins = _solver.get_model( _system->input_vars() );
    auto p = _solver.get_model( _system->state_vars() );

    [[maybe_unused]]
    const auto sat = with_solver()
            .constrain_not_mapped( s, [ & ]( literal l ){ return _system->prime( l ); } )
            .assume( _transition_activator )
            .assume( ins )
            .assume( p )
            .is_sat();

    assert( !sat );

    return _ctis.make( cube{ _solver.get_core( p ) }, cube{ std::move( ins ) }, po.handle() );
}

std::pair< std::vector< literal >, int > pdr::generalize_from_core( std::span< const literal > s, int level )
{
    int j = depth();

    for ( int i = level - 1; i <= depth(); ++i )
    {
        if ( _solver.is_in_core( _trace_activators[ i ] ) )
        {
            j = i;
            break;
        }
    }

    const auto all_lits = std::vector< literal >( s.begin(), s.end() );
    auto res_lits = all_lits;

    for ( const auto lit : all_lits )
    {
        if ( _solver.is_in_core( _system->prime( lit ) ) )
            continue;

        res_lits.erase( std::remove( res_lits.begin(), res_lits.end(), lit ), res_lits.end() );

        if ( intersects_initial_states( res_lits ) )
            res_lits.push_back( lit );
    }

    return { std::move( res_lits ), j + 1 };
}

// Proof obligation po was blocked, i.e. it has no predecessors at the previous
// level. Its cube is therefore inductive relative to the previous level. Try
// to shrink it and possibly move it further along the trace.
std::pair< cube, int > pdr::generalize_inductive( const proof_obligation& po )
{
    auto [ res_lits, res_level ] = generalize_from_core( _ctis.get( po.handle() ).state_vars().literals(), po.level() );
    const auto all_lits = res_lits;

    for ( const auto lit : all_lits )
    {
        res_lits.erase( std::remove( res_lits.begin(), res_lits.end(), lit ), res_lits.end() );

        if ( intersects_initial_states( res_lits ) || !is_relative_inductive( res_lits, res_level ) )
            res_lits.emplace_back( lit );
        else
            std::tie( res_lits, res_level ) = generalize_from_core( res_lits, res_level );
    }

    while ( res_level <= depth() )
    {
        if ( is_relative_inductive( res_lits, res_level + 1 ) )
            std::tie( res_lits, res_level ) = generalize_from_core( res_lits, res_level + 1 );
        else
            break;
    }

    return { cube{ std::move( res_lits ) }, res_level };
}

counterexample pdr::build_counterexample( cti_handle initial )
{
    logger::log_line_loud( "Found a counterexample at k = {}", depth() );

    // CTI entries don't necessarily contain all the variables. If a variable
    // doesn't appear in any literal, its value is not important, so we might
    // as well just make it false.
    auto get_vars = []( variable_range range, const cube& val )
    {
        auto row = valuation{};
        row.reserve( range.size() );

        for ( const auto var : range )
            row.push_back( val.find( var ).value_or( literal{ var, true } ) );

        return row;
    };

    auto entry = std::optional{ _ctis.get( initial ) };

    auto initial_state = get_vars( _system->state_vars(), entry->state_vars() );

    auto inputs = std::vector< valuation >{};
    inputs.reserve( depth() );

    while ( entry.has_value() )
    {
        inputs.emplace_back( get_vars( _system->input_vars(), entry->input_vars() ) );
        entry = entry->successor().transform( [ & ]( cti_handle h ){ return _ctis.get( h ); } );
    }

    return counterexample{ std::move( initial_state ), std::move( inputs ) };
}

bool pdr::is_already_blocked( const proof_obligation& po )
{
    assert( 1 <= po.level() );

    if ( po.level() > depth() )
        return false;

    const auto& s = _ctis.get( po.handle() ).state_vars();

    for ( const auto& frame : frames_from( po.level() ) )
        for ( const auto& cube : frame )
            if ( cube.subsumes( s ) )
                return true;

    return !with_solver()
            .assume( s.literals() )
            .assume( activators_from( po.level() ) )
            .is_sat();
}

bool pdr::intersects_initial_states( std::span< const literal > c )
{
    for ( const auto lit : c )
        if ( _init_cube.contains( !lit ) )
            return false;

    return true;
}

// Check whether a cube s in R_i is inductive relative to R_{i - 1}, i.e.
// whether the formula R_{i - 1} /\ -s /\ T /\ s' is unsatisfiable.
bool pdr::is_relative_inductive( std::span< const literal > s, int i )
{
    assert( i >= 1 );

    return !with_solver()
            .constrain_not( s )
            .assume( activators_from( i - 1 ) )
            .assume( _transition_activator )
            .assume_mapped( s, [ & ]( literal l ){ return _system->prime( l ); } )
            .is_sat();
}

void pdr::add_blocked_at( const cube& c, int level, int start_from /* = 1*/ )
{
    assert( 1 <= level );
    assert( 1 <= start_from && start_from <= level );
    assert( is_state_cube( c.literals() ) );

    const auto k = std::min( level, depth() );

    for ( int d = start_from; d <= k; ++d )
    {
        auto& cubes = _trace_blocked_cubes[ d ];

        for ( std::size_t i = 0; i < cubes.size(); )
        {
            if ( c.subsumes( cubes[ i ] ) )
            {
                cubes[ i ] = cubes.back();
                cubes.pop_back();
            }
            else
                ++i;
        }
    }

    assert( k < _trace_blocked_cubes.size() );
    assert( k < _trace_activators.size() );

    _trace_blocked_cubes[ k ].emplace_back( c );
    _solver.assert_formula( c.negate().activate( _trace_activators[ k ].var() ) );
}

// Returns true if the system has been proven safe by finding an invariant.
bool pdr::propagate()
{
    assert( _trace_blocked_cubes[ depth() ].empty() );

    for ( int i = 1; i < depth(); ++i )
    {
        // The copy is done since the _trace_blocked_cubes[ i ] will be changed
        // during the forthcoming iteration.
        const auto cubes = _trace_blocked_cubes[ i ];

        for ( const auto& c : cubes )
        {
            if ( is_relative_inductive( c.literals(), i + 1 ) )
            {
                const auto [ lits, level ] = generalize_from_core( c.literals(), i + 1 );
                add_blocked_at( cube{ lits }, level, i );
            }
        }

        if ( _trace_blocked_cubes[ i ].empty() )
            return true;
    }

    log_trace_content();

    return false;
}

void pdr::refresh_solver()
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

// Returns true if cube contains only state variables. Used for assertions
// only.
bool pdr::is_state_cube( std::span< const literal > literals ) const
{
    const auto is_state_var = [ & ]( variable var )
    {
        const auto [ type, _ ] = _system->get_var_info( var );
        return type == var_type::state;
    };

    return std::ranges::all_of( literals, [ & ]( literal lit ){ return is_state_var( lit.var() ); } );
}

void pdr::log_trace_content() const
{
    auto line = std::format( "{}:", depth() );

    for ( int i = 1; i <= depth(); ++i )
        line += std::format( " {}", _trace_blocked_cubes[ i ].size() );

    logger::log_line_loud( "{}", line );
}

} // namespace geyser::pdr