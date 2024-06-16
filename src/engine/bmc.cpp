#include "bmc.hpp"
#include <limits>
#include <format>

namespace geyser
{

// Repeatedly check the satisfiability of
//   Init(X_0) /\ Error(X_0)
//   Init(X_0) /\ Trans(X_0, Y_0, X_1) /\ Error(X_1)
//   Init(X_0) /\ Trans(X_0, Y_0, X_1) /\ Trans(X_1, Y_1, X_2) /\ Error(X_2)
//   ...
// until Error(X_{bound}).
result bmc::run( const transition_system& system )
{
    _system = &system;
    const auto bound = _opts->bound.value_or( std::numeric_limits< int >::max() );

    setup_versioning();

    for ( auto i = 0; i < bound; ++i )
    {
        if ( i % solver_refresh_rate == 0 )
            refresh_solver( i );

        const auto maybe_model = check_for( i );

        if ( maybe_model.has_value() )
            return *maybe_model;
    }

    return unknown{ std::format( "counterexample not found after {} steps", bound ) };
}

// Load the solver with
//   Init(X_0) /\ Trans(X_0, Y_0, X_1) /\ ... /\ Trans(X_{bound - 1}, Y_{bound - 1}, X_{bound})
void bmc::refresh_solver( int bound )
{
    trace( "Refreshing the solver after {} steps", bound );

    assert( bound >= 0 );
    assert( _system );

    _solver = std::make_unique< CaDiCaL::Solver >();
    _activators.clear();

    assert_formula( _system->init() );

    for ( auto i = 0; i < bound; ++i )
        assert_formula( make_trans( i ) );
}

// Check the satisfiability of
//   Init(X_0) /\ Trans(X_0, Y_0, X_1) /\ ... /\ Trans(X_{step - 1}, Y_{step - 1}, X_{step}) /\ Error(X_{step})
std::optional< counterexample > bmc::check_for( int step )
{
    trace( "BMC entering step {}", step );

    assert( step >= 0 );
    assert( _solver );

    if ( step > 0 )
        assert_formula( make_trans( step - 1 ) );

    assert_formula( make_error( step ) );

    // The solver contains
    //    Error(X_i) /\ Error(X_{i + 1}) /\ ... /\ Error(X_{step - 1}),
    // for some i, which must be disabled.

    for ( std::size_t i = 0; i < _activators.size() - 1; ++i )
        _solver->assume( ( !_activators[ i ] ).value() );

    _solver->assume( _activators.back().value() );

    const auto res = _solver->solve();

    assert( res != CaDiCaL::UNKNOWN );

    if ( res == CaDiCaL::SATISFIABLE )
        return build_counterexample( step );

    return {};
}

counterexample bmc::build_counterexample( int step )
{
    trace( "Found a counterexample at step {}, building", step );

    assert( step >= 0 );
    assert( step <= _versioned_state_vars.size() );
    assert( step <= _versioned_input_vars.size() );

    auto inputs = std::vector< valuation >( step + 1 );
    auto states = std::vector< valuation >( step + 1 );

    for ( int i = 0; i <= step; ++i )
    {
        auto& input = inputs[ i ];
        auto& state = states[ i ];

        input.reserve( _system->input_vars().size() );
        state.reserve( _system->state_vars().size() );

        for ( int vi = 0; vi < _system->input_vars().size(); ++vi )
            input.emplace_back( _system->input_vars().nth( vi ),
                                !is_true( _versioned_input_vars[ i ].nth( vi ) ) );

        for ( int vi = 0; vi < _system->state_vars().size(); ++vi )
            state.emplace_back( _system->state_vars().nth( vi ),
                                !is_true( _versioned_state_vars[ i ].nth( vi ) ) );
    }

    return counterexample{ std::move( inputs ), std::move( states ) };
}

// Make the formula Trans(X_{step}, Y_{step}, X_{step + 1}).
// If the maximum step for a call to make_trans was k, a call is only allowed
// for make_trans( i ) where 0 <= i <= k + 1.
const cnf_formula& bmc::make_trans( int step )
{
    trace( "Making a new transition formula for step {}", step );

    assert( step >= 0 );

    if ( step < _versioned_trans.size() )
        return _versioned_trans[ step ];

    assert( step == _versioned_trans.size() );

    // Ensure that versioned variables exist in versions from 0 up to size + 1
    // (the + 1 is there to accommodate next state variables).
    const auto make_vars = [ & ]( variable_range unversioned, vars& versioned )
    {
        while ( versioned.size() <= step + 1 )
        {
            versioned.push_back( _store->make_range( unversioned.size(), [ & ]( int i )
            {
                return std::format( "{}/{}", _store->get_name( unversioned.nth( i ) ), step );
            } ) );
        }
    };

    make_vars( _system->state_vars(), _versioned_state_vars );
    make_vars( _system->input_vars(), _versioned_input_vars );
    make_vars( _system->aux_vars(), _versioned_aux_vars );

    assert( step + 1 < _versioned_state_vars.size() );
    assert( step < _versioned_input_vars.size() );
    assert( step < _versioned_aux_vars.size() );

    const auto ins = _versioned_input_vars[ step ];
    const auto here = _versioned_state_vars[ step ];
    const auto there = _versioned_state_vars[ step + 1 ];
    const auto aux = _versioned_aux_vars[ step ];

    return _versioned_trans.emplace_back( _system->trans().map( [ & ]( literal lit )
    {
        const auto [ type, pos ] = _system->get_var_info( lit.var() );

        switch ( type )
        {
            case var_type::input:
                return lit.substitute( ins.nth( pos ) );
            case var_type::state:
                return lit.substitute( here.nth( pos ) );
            case var_type::next_state:
                return lit.substitute( there.nth( pos ) );
            case var_type::auxiliary:
                return lit.substitute( aux.nth( pos ) );
        }
    } ) );
}

// Make Error(X_{step}) and return it, keeping track of its activation
// variable. This can be called only once we have generated the correct
// version of the state and aux variables, but that is handled by make_trans.
// We also assume that calls to make_error are made in sequence with increasing
// bounds.
cnf_formula bmc::make_error( int step )
{
    trace( "Making the error formula for step {}", step );

    assert( step >= 0 );
    assert( step < _versioned_state_vars.size() );
    assert( step < _versioned_aux_vars.size() );

    const auto here = _versioned_state_vars[ step ];
    const auto aux = _versioned_aux_vars[ step ];
    const auto activator = _store->make( std::format( "activator[{}]", step ) );

    assert( step == _activators.size() );
    _activators.emplace_back( activator );

    return _system->error().map( [ & ]( literal lit )
    {
        const auto [ type, pos ] = _system->get_var_info( lit.var() );

        switch ( type )
        {
            case var_type::state:
                return lit.substitute( here.nth( pos ) );
            case var_type::auxiliary:
                return lit.substitute( aux.nth( pos ) );
            default:
                trace( "An unexpected variable ({}) has occurred in the base error formula",
                       std::to_underlying( type ) );
                assert( false );
        }
    }).activate( activator );
}

} // namespace geyser