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

const cnf_formula& bmc::make_trans( int step )
{
    if ( step < _versioned_trans.size() )
        return _versioned_trans[ step ];

    ensure_versioned_vars( step );

    // TODO
}

void bmc::ensure_versioned_vars( int step )
{
    const auto filler = [ step ]( std::vector< var_id_range >& versioned_vars )
    {
        while ( versioned_vars.size() < step )
        {
            // TODO: We need to unversion the var here in order to get the original name
        }
    };

    filler( _versioned_state_vars );
    filler( _versioned_input_vars );
}

} // namespace geyser