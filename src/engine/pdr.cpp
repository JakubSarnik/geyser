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

        // TODO
    }
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

bool pdr::propagate()
{
    // TODO
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

} // namespace geyser::pdr