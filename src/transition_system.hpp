#pragma once

#include "formula.hpp"
#include <vector>

namespace geyser
{

class transition_system
{
    using variables = std::vector< variable >;

    variables _input_vars;
    variables _state_vars;
    variables _next_state_vars;

    cnf_formula _init; // Contains only _state_vars
    cnf_formula _trans; // Contains only _input_vars, _state_vars and _next_state_vars
    cnf_formula _prop; // Contains only _state_vars

public:
    transition_system( variables input_vars, variables state_vars, variables next_state_vars,
                       cnf_formula init, cnf_formula trans, cnf_formula prop )
    : _input_vars{ std::move( input_vars ) }, _state_vars{ std::move( state_vars ) },
      _next_state_vars{ std::move( next_state_vars ) },
      _init{ std::move( init ) }, _trans{ std::move( trans ) }, _prop{ std::move( prop ) } {}
};

}