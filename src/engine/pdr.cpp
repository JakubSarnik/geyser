#include "pdr.hpp"

namespace geyser::pdr
{

result pdr::run( const transition_system& system )
{
    _system = &system;
    const auto bound = _opts->bound.value_or( std::numeric_limits< int >::max() );

    initialize();

    return unknown{};
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

void pdr::refresh_solver()
{
    trace( "Refreshing the solver" );

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