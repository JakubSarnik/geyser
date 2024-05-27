#pragma once

#include "logic.hpp"
#include <cassert>
#include <utility>

namespace geyser
{

class transition_system
{
    // The transition system consists of 3 formulas. The initial formula _init
    // describes the initial states, the error formula _error describes the bad
    // states and the transition formula _trans describes the transition
    // relation. The formulas _init and _trans range over state variables, and
    // _trans ranges over those as well, plus the primed (next) state variables
    // and the input variables. In addition, all three formulas might contain
    // additional auxiliary Tseitin variables, but those are not important for
    // consumers of this (i.e. model checking engines).

    var_id_range _input_vars;
    var_id_range _state_vars;
    var_id_range _next_state_vars;

    cnf_formula _init;
    cnf_formula _trans;
    cnf_formula _error;

public:
    transition_system( var_id_range input_vars, var_id_range state_vars, var_id_range next_state_vars,
                       cnf_formula init, cnf_formula trans, cnf_formula error )
            : _input_vars{ input_vars }, _state_vars{ state_vars }, _next_state_vars{ next_state_vars },
              _init{ std::move( init ) }, _trans{ std::move( trans ) }, _error{ std::move( error ) }
    {
        assert( _input_vars.first <= _input_vars.second );
        assert( _state_vars.first <= _state_vars.second );
        assert( _next_state_vars.first <= _next_state_vars.second );
        assert( _next_state_vars.second - _next_state_vars.first == _state_vars.second - _state_vars.first );
    }

    [[nodiscard]] const cnf_formula& init() const { return _init; }
    [[nodiscard]] const cnf_formula& trans() const { return _trans; }
    [[nodiscard]] const cnf_formula& error() const { return _error; }
};

} // namespace geyser