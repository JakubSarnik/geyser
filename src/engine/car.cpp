#include "car.hpp"
#include <queue>
#include <ranges>
#include <string>

namespace geyser::car
{

result car::run( const transition_system& system )
{
    _system = &system;
    const auto bound = _opts->bound.value_or( std::numeric_limits< int >::max() );

    initialize();

    return check( bound );
}

void car::initialize()
{
    push_frame();
    push_coframe();

    _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
    _activated_trans = _system->trans().activate( _transition_activator.var() );
    _activated_error = _system->error().activate( _error_activator.var() );
}

void car::refresh_solver()
{
    log_line_loud( "Refreshing the solver after {} queries", _queries );

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

result car::check( int bound )
{
    while ( depth() < bound )
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

    }

    return unknown{ std::format( "counterexample not found after {} frames", bound ) };
}

std::optional< counterexample > car::check_existing_cotrace()
{
    // Iterate through the cotrace in reverse, i.e. from states that are
    // furthest from the error states (at least as far as we know at the
    // moment). This is a heuristic which is used by SimpleCAR in the default
    // mode.

    for ( int j = codepth(); j >= 0; --j )
    {
        for ( const auto handle : _cotrace_found_cubes[ j ] )
        {
            // Does this entry still have non-empty intersection with the
            // current frame?

            const auto& s = _cotrace.get( handle ).state_vars();

            assert( depth() < _trace_activators.size() );

            if ( with_solver()
                    .assume( _trace_activators[ depth() ] )
                    .assume( s.literals() )
                    .is_sat() )
            {
                if ( const auto cex = solve_obligation( proof_obligation{ handle, depth(), j } ); cex.has_value() )
                    return cex;
            }
        }
    }

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

std::optional< bad_state_handle > car::get_error_state()
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
    // TODO
}

void car::add_reaching_at( bad_state_handle h, int level )
{
    assert( 0 <= j );

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

bool car::propagate()
{
    // TODO
}

bool car::is_inductive()
{
    // TODO
}

transition_system backward_car::reverse_system( const transition_system& system )
{
    // TODO
}

} // namespace geyser::car