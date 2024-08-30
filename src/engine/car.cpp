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

        log_trace_content();
        log_cotrace_content();
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

            log_line_debug( "B[{}]: {}", ix, cube_to_string( _cotrace.get( pred ).state_vars() ) );
            add_reaching_at( pred, ix );

            min_queue.emplace( pred, po.level() - 1, ix );
            min_queue.push( po );
        }
        else
        {
            auto c = generalize_blocked( po );

            log_line_debug( "F[{}]: {}", po.level(), cube_to_string( c ) );
            add_blocked_at( c, po.level() );

            // TODO: Is this good in CAR?
            // if ( po.level() < depth() )
            //     min_queue.emplace( po.handle(), po.level() + 1 );
        }
    }

    return {};
}

counterexample car::build_counterexample( bad_cube_handle initial )
{
    log_line_loud( "Found a counterexample at k = {}", depth() );

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
        std::ranges::reverse( inputs );

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
            .assume_mapped( s, [ & ]( literal l ){ return prime_literal( l ); } )
            .is_sat();
}

bad_cube_handle car::get_predecessor( const proof_obligation& po )
{
    auto ins = _solver.get_model( _system->input_vars() );
    auto p = _solver.get_model( _system->state_vars() );

    if ( _forward )
    {
        const auto& s = _cotrace.get( po.handle() ).state_vars().literals();

        [[maybe_unused]]
        const auto sat = with_solver()
                .constrain_not_mapped( s, [ & ]( literal l ){ return prime_literal( l ); } )
                .assume( _transition_activator )
                .assume( ins )
                .assume( p )
                .is_sat();

        assert( !sat );

        return _cotrace.make( cube{ _solver.get_core( p ) }, cube{ std::move( ins ) }, po.handle() );
    }
    else
    {
        // We cannot generalize in the backward mode.
        return _cotrace.make( cube{ std::move( p ) }, cube{ std::move( ins ) }, po.handle() );
    }
}

cube car::generalize_blocked( const proof_obligation& po )
{
    // TODO
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
    assert( is_state_cube( c ) );

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
    // TODO
}

bool car::is_inductive()
{
    // TODO
}

literal car::prime_literal( literal lit ) const
{
    const auto [ type, pos ] = _system->get_var_info( lit.var() );
    assert( type == var_type::state );

    return lit.substitute( _system->next_state_vars().nth( pos ) );
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

bool car::is_state_cube( const cube& cube ) const
{
    return is_state_cube( cube.literals() );
}

void car::log_trace_content() const
{
    auto line = std::format( "F[{}]:", depth() );

    for ( int i = 1; i <= depth(); ++i )
        line += std::format( " {}", _trace_blocked_cubes[ i ].size() );

    log_line_loud( "{}", line );
}

void car::log_cotrace_content() const
{
    auto line = std::format( "B[{}]:", depth() );

    for ( int i = 1; i <= codepth(); ++i )
        line += std::format( " {}", _cotrace_found_cubes[ i ].size() );

    log_line_loud( "{}", line );
}

transition_system backward_car::reverse_system( const transition_system& system )
{
    // TODO
}

} // namespace geyser::car