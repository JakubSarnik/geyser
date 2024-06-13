#pragma once

#include "base.hpp"
#include "cadical.hpp"
#include <optional>
#include <memory>
#include <vector>

namespace geyser
{

class bmc : public engine
{
    using engine::engine;

    using vars = std::vector< variable_range >;

    // Each solver_refresh_rate iterations, reset the solver by throwing away
    // all the accumulated (and disabled) error formulas.
    constexpr static int solver_refresh_rate = 100;

    std::unique_ptr< CaDiCaL::Solver > _solver;
    const transition_system* _system = nullptr;

    // Each state variable x in X (the set of state variables) occurs in various
    // versions x_0, x_1, ..., throughout the computation. We store the versioned
    // ranges contiguously, so that if the transition system contains e.g. state
    // variables a, b in X with IDs 4, 5 (i.e. _system.state_vars() = [4, 6)),
    // a_4 and b_4 will have IDs in range _versioned_state_vars[ 4 ] = [k, l)
    // for some integers k <= l. As a minor optimization, a_0 and b_0 are the
    // original variables a, b (i.e. a_0 has id 4, b_0 has id 5). The same is
    // true for input variables y in Y and auxiliary/tseitin/and variables.

    vars _versioned_state_vars;
    vars _versioned_input_vars;
    vars _versioned_aux_vars;

    // A cache for versioned transition formulas. _versioned_trans[ i ] is the
    // formula Trans(X_i, Y_i, X_{i + 1}).
    std::vector< cnf_formula > _versioned_trans;

    // _activators[ i ] is the activation literal (a positive variable) for
    // formula Error(X_i).
    std::vector< literal > _activators;

    void assert_formula( const cnf_formula& formula )
    {
        assert( _solver );

        for ( const auto lit : formula.literals() )
            _solver->add( lit.value() );
    }

    void setup_versioning()
    {
        assert( _system );

        _versioned_state_vars.push_back( _system->state_vars() );
        _versioned_input_vars.push_back( _system->input_vars() );
        _versioned_aux_vars.push_back( _system->aux_vars() );
    }

    bool is_true( variable var )
    {
        assert( _solver );
        assert( ( _solver->state() & CaDiCaL::SATISFIED ) != 0 );

        return _solver->val( var.id() ) > 0;
    }

    void refresh_solver( int bound );
    std::optional< counterexample > check_for( int step );
    counterexample build_counterexample( int step );

    const cnf_formula& make_trans( int step );
    cnf_formula make_error( int step );

public:
    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace geyser