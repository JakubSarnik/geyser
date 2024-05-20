#pragma once

#include "formula.hpp"
#include "transition_system.hpp"
#include "caiger.hpp"
#include <string>
#include <expected>

namespace geyser
{

namespace
{
using aiger_literal = unsigned int;
}

class aiger_builder
{
    variable_store* _store;
    aiger* _aig;

    int _input_vars_begin = 0, _input_vars_end = 0;
    int _state_vars_begin = 0, _state_vars_end = 0;
    int _next_state_vars_begin = 0, _next_state_vars_end = 0;
    int _and_vars_begin = 0;
    [[maybe_unused]] int _and_vars_end = 0;

    [[nodiscard]] variable get_input_var( int index ) const
    {
        const auto pos = _input_vars_begin + index;
        assert( pos < _input_vars_end );

        return variable{ pos };
    }

    [[nodiscard]] variable get_state_var( int index ) const
    {
        const auto pos = _state_vars_begin + index;
        assert( pos < _state_vars_end );

        return variable{ pos };
    }

    [[nodiscard]] variable get_next_state_var( int index ) const
    {
        const auto pos = _next_state_vars_begin + index;
        assert( pos < _next_state_vars_end );

        return variable{ pos };
    }

    [[nodiscard]] variable get_and_var( int index ) const
    {
        const auto pos = _and_vars_begin + index;
        assert( pos < _and_vars_end );

        return variable{ pos };
    }

    // TODO: Do we need the primed parameter?
    [[nodiscard]]
    variable aiger_var_to_our_var( aiger_literal lit, bool primed = false ) const;

    [[nodiscard]]
    literal aiger_lit_to_our_lit( aiger_literal lit, bool primed = false ) const
    {
        return literal
        {
            aiger_var_to_our_var( aiger_strip( lit ), primed ),
            aiger_sign( lit ) == 1
        };
    }

    [[nodiscard]]
    cnf_formula clausify_and( aiger_literal lhs, aiger_literal rhs0, aiger_literal rhs1 );

    cnf_formula build_init();
    cnf_formula build_trans();
    cnf_formula build_error();

public:
    explicit aiger_builder( variable_store& store, aiger& aig ) : _store{ &store }, _aig{ &aig } {}

    aiger_builder( const aiger_builder& ) = delete;
    aiger_builder& operator=( const aiger_builder& ) = delete;

    aiger_builder( aiger_builder&& ) = delete;
    aiger_builder& operator=( aiger_builder&& ) = delete;

    ~aiger_builder() = default;

    // This consumes this builder, as it moves its data into the transition system!
    [[nodiscard]] std::expected< transition_system, std::string > build();
};

} // namespace geyser