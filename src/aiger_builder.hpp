#pragma once

#include "formula.hpp"
#include "transition_system.hpp"
#include "caiger.hpp"
#include <string>
#include <expected>

namespace geyser
{

class aiger_builder
{
    using variables = std::vector< variable >;

    variable_store* _store;

    variables _input_vars;
    variables _state_vars;
    variables _next_state_vars;
    variables _aux_vars; // Tseitin encoding auxiliaries

    cnf_formula build_init( const aiger& aig );
    cnf_formula build_trans( const aiger& aig );
    cnf_formula build_error( const aiger& aig );

public:
    explicit aiger_builder( variable_store& store ) : _store{ &store } {}

    aiger_builder( const aiger_builder& ) = delete;
    aiger_builder& operator=( const aiger_builder& ) = delete;

    aiger_builder( aiger_builder&& ) = delete;
    aiger_builder& operator=( aiger_builder&& ) = delete;

    ~aiger_builder() = default;

    // This consumes this builder, as it moves its data into the transition system!
    [[nodiscard]] std::expected< transition_system, std::string > build( const aiger& aig );
};

} // namespace geyser