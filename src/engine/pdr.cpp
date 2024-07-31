#include "pdr.hpp"
#include <queue>
#include <ranges>

namespace geyser::pdr
{

result pdr::run( const transition_system& system )
{
    _system = &system;
    const auto bound = _opts->bound.value_or( std::numeric_limits< int >::max() );

    initialize();

    return check( bound );
}

void pdr::initialize()
{
    push_frame();

    _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    _activated_trans = _system->trans().activate( _transition_activator.var() );
    _activated_error = _system->error().activate( _error_activator.var() );
}

result pdr::check( int bound )
{
    while ( depth() < bound )
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

    return unknown{ std::format( "counterexample not found after {} frames", bound ) };
}

// Returns a cti handle to a state which reaches error under given
// input variable values.
std::optional< cti_handle > pdr::get_error_cti()
{
    if ( with_solver()
         .assume( activators_from( depth() ) )
         .assume( _error_activator )
         .is_sat() )
        return _ctis.make( get_model( _system->state_vars() ), get_model( _system->input_vars() ) );

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

            trace( "{}: {}", i, c.format() );
            add_blocked_at( c, i );

            if ( po.level() <= depth() )
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
    // TODO: Try making this into the loop again.

    const auto& s = _ctis.get( po.handle() ).state_vars();
    auto ins = get_model( _system->input_vars() );
    auto p = get_model( _system->state_vars() );

    [[maybe_unused]]
    const auto sat = with_solver()
            .constrain_not_mapped( s, [ & ]( literal l ){ return prime_literal( l ); } )
            .assume( _transition_activator )
            .assume( ins.literals() )
            .assume( p.literals() )
            .is_sat();

    assert( !sat );

    auto res = std::vector< literal >{};

    for ( const auto lit : p.literals() )
        if ( _solver->failed( lit.value() ) )
            res.emplace_back( lit );

    return _ctis.make( cube{ std::move( res ) }, std::move( ins ), po.handle() );
}

// Proof obligation po was blocked, i.e. it has no predecessors at the previous
// level. Its cube is therefore inductive relative to the previous level. Try
// to shrink it and possibly move it further along the trace.
std::pair< cube, int > pdr::generalize_inductive( const proof_obligation& po )
{
    assert( _solver->state() == CaDiCaL::UNSATISFIED );

    int j = depth();

    for ( int i = po.level() - 1; i <= depth(); ++i )
    {
        if ( _solver->failed( _trace_activators[ i ].value() ) )
        {
            j = i;
            break;
        }
    }

    int res_level = std::min( j + 1, depth() );

    auto all_lits = _ctis.get( po.handle() ).state_vars().literals();
    auto res_lits = all_lits;

    for ( const auto lit : all_lits )
    {
        if ( !_solver->failed( lit.value() ) )
            continue;

        // TODO: Optimize erasing by a single swap?
//        res_lits.erase( std::remove( res_lits.begin(), res_lits.end(), lit ), res_lits.end() );
//
//        if ( intersects_initial_states( res_lits ) )
//            res_lits.push_back( lit );

        auto shorter = res_lits;
        shorter.erase( std::remove( shorter.begin(), shorter.end(), lit ), shorter.end() );

        if ( !intersects_initial_states( shorter ) )
            res_lits = shorter;
    }

    all_lits = res_lits;

    for ( const auto lit : all_lits )
    {
        // TODO: Optimize erasing by a single swap?
        res_lits.erase( std::remove( res_lits.begin(), res_lits.end(), lit ), res_lits.end() );

        if ( intersects_initial_states( res_lits ) || !is_relative_inductive( res_lits, res_level ) )
            res_lits.emplace_back( lit );
    }

    const auto res_cube = cube{ res_lits };

    while ( res_level <= depth() )
    {
        if ( is_relative_inductive( res_cube.literals(), res_level + 1 ) )
            ++res_level;
        else
            break;
    }

    return std::make_pair( res_cube, res_level );
}

counterexample pdr::build_counterexample( cti_handle initial )
{
    trace( "Found a counterexample at k = {}", depth() );

    // CTI entries don't necessarily contain all the variables. If a variable
    // doesn't appear in any literal, its value is not important, so we might
    // as well just make it false.
    auto get_vars = []( variable_range range, const cube& val )
    {
        auto row = valuation{};
        row.reserve( range.size() );

        for ( int vi = 0; vi < range.size(); ++vi )
        {
            const auto var = range.nth( vi );
            row.push_back( val.find( var ).value_or( literal{ var, true } ) );
        }

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
    // Note that this would not work if we had initial formula more
    // complex than a cube (i.e. with invariance constraints)!

    const auto& init_lits = _system->init().literals();

    for ( const auto lit : c )
        if ( std::find( init_lits.begin(), init_lits.end(), !lit ) != init_lits.end() )
            return false;

    return true;

//    return with_solver()
//           .assume( _trace_activators[ 0 ] )
//           .assume( c.literals() )
//           .is_sat();
}

// Check whether, given po = < s, i >, s is inductive relative to
// R_{i - 1}, i.e. whether the formula R_{i - 1} /\ -s /\ T /\ s' is
// unsatisfiable.
// bool pdr::is_relative_inductive( const proof_obligation& po )
bool pdr::is_relative_inductive( std::span< const literal > s, int i )
{
    assert( i >= 1 );

    return !with_solver()
            .constrain_not( s )
            .assume( activators_from( i - 1 ) )
            .assume( _transition_activator )
            .assume_mapped( s, [ & ]( literal l ){ return prime_literal( l ); } )
            .is_sat();
}

void pdr::add_blocked_at( const cube& c, int level, int start_from /* = 1*/ )
{
    assert( 1 <= level );
    assert( 1 <= start_from && start_from <= level );
    assert( is_state_cube( c ) );

    const auto k = std::min( level, depth() );

    for ( int d = 1; d <= k; ++d )
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
    assert_formula( c.negate().activate( _trace_activators[ k ].var() ) );
}

// Returns true if the system has been proven safe by finding an invariant.
bool pdr::propagate()
{
    trace( "Propagating (k = {})", depth() );

    assert( _trace_blocked_cubes[ depth() ].empty() );

    for ( int i = 1; i < depth(); ++i )
    {
        // The copy is done since the _trace_blocked_cubes[ i ] will be changed
        // during the forthcoming iteration.
        const auto cubes = _trace_blocked_cubes[ i ];

        for ( const auto& c : cubes )
            if ( is_relative_inductive( c.literals(), i + 1 ) )
                add_blocked_at( c, i + 1, i );

        if ( cubes.empty() )
            return true;
    }

    for ( int i = 1; i <= depth(); ++i )
        trace( "  F[ {} ]: {} cubes", i, _trace_blocked_cubes[ i ].size() );

    return false;
}

void pdr::refresh_solver()
{
    trace( "Refreshing the solver after {} queries", _queries );

    assert( _system );

    _solver = std::make_unique< CaDiCaL::Solver >();
    _queries = 0;

    assert_formula( _activated_init );
    assert_formula( _activated_trans );
    assert_formula( _activated_error );

    for ( const auto& [ cubes, act ] : std::views::zip( _trace_blocked_cubes, _trace_activators ) )
        for ( const auto& cube : cubes )
            assert_formula( cube.negate().activate( act.var() ) );
}

literal pdr::prime_literal( literal lit ) const
{
    const auto [ type, pos ] = _system->get_var_info( lit.var() );
    assert( type == var_type::state );

    return lit.substitute( _system->next_state_vars().nth( pos ) );
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

bool pdr::is_state_cube( const cube& cube ) const
{
    return is_state_cube( cube.literals() );
}

} // namespace geyser::pdr