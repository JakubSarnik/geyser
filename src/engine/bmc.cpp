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

result bmc::do_run()
{
    const auto bound = _opts->bound.value_or( std::numeric_limits< int >::max() );

    setup_versioning();
    setup_solver( 0 );

    for ( auto i = 0; i < bound; ++i )
    {
        const auto maybe_model = check_for( i );

        if ( maybe_model.has_value() )
            return *maybe_model;
    }

    return unknown{ std::format( "counterexample not found after {} steps", bound ) };
}

// Check the satisfiability of
//   Init(X_0) /\ Trans(X_0, Y_0, X_1) /\ ... /\ Trans(X_{step - 1}, Y_{step - 1}, X_{step}) /\ Error(X_{step})

std::optional< counterexample > bmc::check_for( int step )
{
    trace( "BMC entering step {}", step );

    assert( step >= 0 );
    assert( _solver );

    // TODO
}

// Load the solver with
//   Init(X_0) /\ Trans(X_0, Y_0, X_1) /\ ... /\ Trans(X_{bound - 1}, Y_{bound - 1}, X_{step})

void bmc::setup_solver( int bound )
{
    assert( bound >= 0 );

    _solver = std::make_unique< CaDiCaL::Solver >();

    assert_formula( _system->init() );

    for ( auto i = 0; i < bound; ++i )
        assert_formula( make_trans( i ) );
}

// Make the formula Trans(X_{step}, Y_{step}, X_{step + 1}).
// If the maximum step for a call to make_trans was k, a call is only allowed
// for make_trans( i ) where 0 <= i <= k + 1.
const cnf_formula& bmc::make_trans( int step )
{
    assert( step >= 0 );

    if ( step < _versioned_trans.size() )
        return _versioned_trans[ step ];

    assert( step == _versioned_trans.size() );

    trace( "Making new transition formula for step {}", step );

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

    make_vars( _system->state_vars(), _versioned_state_vars);
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

} // namespace geyser