#include "pdr.hpp"

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

    _activated_init = _system->init().activate( _trace[ 0 ].activator.var() );
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

        flush_ctis();
    }

    return unknown{};
}

std::optional< counterexample > pdr::block()
{
    assert( k() < _trace.size() );

    while ( with_solver()
            .assume( { _error_activator, _trace[ k() ].activator } )
            .is_sat() )
    {
        auto state_vars = get_model( _system->state_vars() );
        auto input_vars = get_model( _system->input_vars() );
        const auto cti = make_cti( std::move( state_vars ), std::move( input_vars ) );

        const auto cex = solve_obligation( proof_obligation{ cti, k() } );

        if ( cex.has_value() )
            return cex;
    }

    return {};
}

std::optional< counterexample > pdr::solve_obligation( proof_obligation po )
{
    // TODO
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

    for ( const auto& frame : _trace )
        for ( const auto& cube : frame.blocked_cubes )
            assert_formula( cube.negate().activate( frame.activator.var() ) );
}

} // namespace geyser::pdr