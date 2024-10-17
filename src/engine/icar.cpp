#include "icar.hpp"
#include "logger.hpp"
#include <queue>
#include <ranges>
#include <string>

namespace geyser::car
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

    _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    _activated_trans = _system->trans().activate( _transition_activator.var() );
    _activated_error = _system->error().activate( _error_activator.var() );

    _init_negated = formula_as_cube( _system->init() ).negate();
}

void icar::refresh_solver()
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

    for ( const auto& [ handle, act ] : _cotrace_found_cubes )
        add_blocked_to_solver( handle, act );
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

    _solver.assert_formula( cnf );
}

result car::check()
{
    while ( true )
    {
        // TODO
    }
}

} // namespace geyser::car