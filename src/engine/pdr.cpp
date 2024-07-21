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

    _transition_activator = literal{ _store->make( "ActT" ) };
    _error_activator = literal{ _store->make( "ActE" ) };

    _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    _activated_trans = _system->trans().activate( _transition_activator.var() );
    _activated_error = _system->error().activate( _error_activator.var() );
}

result pdr::check( int bound )
{
    while ( k() < bound )
    {
        if ( const auto cex = block(); cex.has_value() )
            return *cex;

        push_frame();

        if ( propagate() )
            return ok{};

        _ctis.flush();
    }

    return unknown{};
}

std::optional< counterexample > pdr::block()
{
    assert( k() < _trace_blocked_cubes.size() );

    while ( with_solver()
            .assume( { _error_activator, _trace_activators[ k() ] } )
            .is_sat() )
    {
        auto state_vars = get_model( _system->state_vars() );
        auto input_vars = get_model( _system->input_vars() );
        const auto cti = _ctis.make( std::move( state_vars ), std::move( input_vars ) );

        const auto cex = solve_obligation( proof_obligation{ cti, k() } );

        if ( cex.has_value() )
            return cex;
    }

    return {};
}

std::optional< counterexample > pdr::solve_obligation( proof_obligation cti_po )
{
    assert( 0 <= cti_po.level() && cti_po.level() <= k() );

    auto min_queue = std::priority_queue< proof_obligation,
        std::vector< proof_obligation >, std::greater<> >{};

    min_queue.push( cti_po );

    while ( !min_queue.empty() )
    {
        auto po = min_queue.top();
        min_queue.pop();

        if ( po.level() == 0 )
            return build_counterexample( po.handle() );

        if ( is_already_blocked( po ) )
            continue;

        // TODO: Assert !intersects_initial( po.handle().state_variables() )

        assert( po.level() >= 1 );

        const auto& state_vars = _ctis.get( po.handle() ).state_vars();

        if ( with_solver()
                .constrain( state_vars.negate() )
                .assume( _transition_activator )
                .assume( prime_cube( state_vars ).literals() )
                .assume( activators_from( po.level() - 1 ) )
                .is_sat() )
        {
            const auto pred_handle = generalize_predecessor( po.handle() );

            min_queue.emplace( pred_handle, po.level() - 1 );
            min_queue.push( po );
        }
        else
        {
            const auto gen = generalize_inductive( po );
            add_blocked_at( _ctis.get( gen ).state_vars(), po.level() );
        }
    }

    return {};
}

counterexample pdr::build_counterexample( cti_handle initial )
{
    trace( "Found a counterexample with k = {}", k() );

    // CTI entries don't necessarily contain all the variables. If a variable
    // doesn't appear in any literal, its value is not important, so we might
    // as well just make it false.
    auto get_vars = []( variable_range range, const ordered_cube& val )
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
    inputs.reserve( k() );

    while ( entry.has_value() )
    {
        inputs.emplace_back( get_vars( _system->input_vars(), entry->input_vars() ) );
        entry = entry->successor().transform( [ & ]( cti_handle h ){ return _ctis.get( h ); } );
    }

    return counterexample{ std::move( initial_state ), std::move( inputs ) };
}

bool pdr::is_already_blocked( proof_obligation po )
{
    assert( 1 <= po.level() && po.level() <= k() );

    for ( const auto& frame : frames_from( po.level() ) )
        for ( const auto& cube : frame )
            if ( cube.subsumes( _ctis.get( po.handle() ).state_vars() ) )
                return true;

    const auto sat = with_solver()
            .assume( _ctis.get( po.handle() ).state_vars().literals() )
            .assume( activators_from( po.level() ) )
            .is_sat();

    return !sat;
}

cti_handle pdr::generalize_predecessor( cti_handle cti )
{
    auto ins = get_model( _system->input_vars() );
    auto p = get_model( _system->state_vars() );
    auto s = _ctis.get( cti ).state_vars();

    while ( true )
    {
        [[maybe_unused]]
        auto sat = with_solver()
                .constrain( prime_cube( s ).negate() )
                .assume( _transition_activator )
                .assume( ins.literals() )
                .assume( p.literals() )
                .is_sat();

        assert ( !sat );

        s = get_unsat_core( p );

        if ( s == p )
            return _ctis.make( std::move( s ), std::move( ins ), cti );

        p = s;
    }
}

cti_handle pdr::generalize_inductive( proof_obligation po )
{
    // TODO: po.level() or po.level() - 1???
    // TODO: Implement a proper generalization...

    return po.handle();
}

void pdr::add_blocked_at( const ordered_cube& c, int level, int start_from /* = 1*/ )
{
    assert( 1 <= level && level <= k() );
    assert( 1 <= start_from && start_from <= level );

    for ( int i = start_from; i <= level; ++i )
    {
        auto& cubes = _trace_blocked_cubes[ i ];

        for ( int j = 0; j < cubes.size(); )
        {
            if ( c.subsumes( cubes[ j ] ) )
            {
                std::swap( cubes[ j ], cubes[ cubes.size() - 1 ] );
                cubes.pop_back();
            }
            else
                ++j;
        }
    }
}

bool pdr::propagate()
{
    assert( _trace_blocked_cubes[ k() ].empty() );

    for ( int i = 1; i < k(); ++i )
    {
        // We maintain the invariant that cubes doesn't contain a pair of
        // cubes c, d such that c subsumes d (this is ensured by
        // add_blocked_at). As such, the call to add_blocked_at removes
        // from the i-th frame precisely the single cube that is subsumed
        // by the cube being added to the (i + 1)-th frame -- the very same
        // cube.
        //
        // The removal is done by swapping the last cube in the set with
        // the removed cube and popping the back. As such, whenever we call
        // add_blocked_at, we don't move forward in cubes, since the same
        // position will now be occupied by a clause that was at the end
        // (or we will already be at the end and stop).

        const auto& cubes = _trace_blocked_cubes[ i ];

        for ( int j = 0; j < cubes.size(); )
        {
            const auto& c = _trace_blocked_cubes[ i ][ j ];

            if ( !( with_solver()
                    .assume( _transition_activator )
                    .assume( prime_cube( c ).literals())
                    .assume( activators_from( i ))
                    .is_sat()))
                add_blocked_at( c, i + 1, i );
            else
                ++j;
        }

        if ( cubes.empty() )
            return true;
    }

    return false;
}

void pdr::refresh_solver()
{
    trace( "Refreshing the solver (k = {})", k() );

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

ordered_cube pdr::prime_cube( const ordered_cube& cube ) const
{
    auto lits = cube.literals();

    for ( auto& lit : lits )
    {
        const auto [ type, pos ] = _system->get_var_info( lit.var() );

        switch ( type )
        {
            case var_type::state:
                lit = lit.substitute( _system->next_state_vars().nth( pos ) );
            default:
                trace( "An unexpected variable ({}) has occurred during priming in PDR",
                       std::to_underlying( type ) );
                std::terminate(); // Unreachable
        }
    }

    return ordered_cube{ lits, sorted_tag };
}

} // namespace geyser::pdr