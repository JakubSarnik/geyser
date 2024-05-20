#pragma once

#include "formula.hpp"
#include <cassert>
#include <utility>

namespace geyser
{

class transition_system
{
    // The transition system consists of 3 formulas. The initial formula _init
    // describes the initial states, the error formula _error describes the bad
    // states and the transition formula _trans describes the transition
    // relation. The formulas _init and _trans range over state variables, i.e.
    // variables with IDs in the range [_state_vars_begin, _state_vars_end),
    // and _trans ranges over those as well, plus the primed state variables in
    // [_next_state_vars_begin, _next_state_vars_end) and the input variables
    // in [_input_vars_begin, _input_vars_end). In addition, all three formulas
    // might contain additional auxiliary Tseitin variables.

    int _input_vars_begin, _input_vars_end;
    int _state_vars_begin, _state_vars_end;
    int _next_state_vars_begin, _next_state_vars_end;

    cnf_formula _init;
    cnf_formula _trans;
    cnf_formula _error;

public:
    using var_id_range = std::pair< int, int >;

    transition_system( var_id_range input_vars, var_id_range state_vars, var_id_range next_state_vars,
                       cnf_formula init, cnf_formula trans, cnf_formula error )
            : _input_vars_begin{ input_vars.first }, _input_vars_end{ input_vars.second },
              _state_vars_begin{ state_vars.first }, _state_vars_end{ state_vars.second },
              _next_state_vars_begin{ next_state_vars.first }, _next_state_vars_end{ next_state_vars.second },
              _init{ std::move( init ) }, _trans{ std::move( trans ) }, _error{ std::move( error ) }
    {
        assert( _input_vars_begin <= _input_vars_end );
        assert( _state_vars_begin <= _state_vars_end );
        assert( _next_state_vars_begin <= _next_state_vars_end );
        assert( _next_state_vars_end - _next_state_vars_begin == _state_vars_end - _state_vars_begin );
    }
};

} // namespace geyser