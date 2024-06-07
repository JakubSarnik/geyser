#pragma once

#include "logic.hpp"
#include <cassert>
#include <utility>

namespace geyser
{

enum class var_type
{
    input,
    state,
    next_state,
    auxiliary
};

class transition_system
{
    // The transition system consists of 3 formulas. The initial formula _init
    // describes the initial states, the error formula _error describes the bad
    // states and the transition formula _trans describes the transition
    // relation. The formulas _init and _trans range over state variables, and
    // _trans ranges over those as well, plus the primed (next) state variables
    // and the input variables. In addition, all three formulas might contain
    // additional auxiliary Tseitin variables.

    var_id_range _input_vars;
    var_id_range _state_vars;
    var_id_range _next_state_vars;
    var_id_range _aux_vars;

    cnf_formula _init;
    cnf_formula _trans;
    cnf_formula _error;

public:
    transition_system( var_id_range input_vars, var_id_range state_vars, var_id_range next_state_vars,
                       var_id_range aux_vars, cnf_formula init, cnf_formula trans, cnf_formula error )
            : _input_vars{ input_vars }, _state_vars{ state_vars }, _next_state_vars{ next_state_vars },
              _aux_vars{ aux_vars }, _init{ std::move( init ) }, _trans{ std::move( trans ) },
              _error{ std::move( error ) }
    {
        assert( _input_vars.first <= _input_vars.second );
        assert( _state_vars.first <= _state_vars.second );
        assert( _aux_vars.first <= _aux_vars.second );
        assert( _next_state_vars.first <= _next_state_vars.second );
        assert( _next_state_vars.second - _next_state_vars.first == _state_vars.second - _state_vars.first );
    }

    [[nodiscard]] var_id_range input_vars() const { return _input_vars; };
    [[nodiscard]] var_id_range state_vars() const { return _state_vars; };
    [[nodiscard]] var_id_range next_state_vars() const { return _next_state_vars; };
    [[nodiscard]] var_id_range aux_vars() const { return _aux_vars; }

    [[nodiscard]] const cnf_formula& init() const { return _init; }
    [[nodiscard]] const cnf_formula& trans() const { return _trans; }
    [[nodiscard]] const cnf_formula& error() const { return _error; }

    // Returns the type of the variable and its position within the respective
    // variable range.
    [[nodiscard]] std::pair< var_type, int > get_var_info( variable var ) const
    {
        const auto id = var.id();

        if ( range_contains( _input_vars, var ) )
            return { var_type::input, id - _input_vars.first };
        if ( range_contains( _state_vars, var ) )
            return { var_type::state, id - _state_vars.first };
        if ( range_contains( _next_state_vars, var ) )
            return { var_type::next_state, id - _next_state_vars.first };
        if ( range_contains( _aux_vars, var ) )
            return { var_type::auxiliary, id - _aux_vars.first };

        assert( false ); // Unreachable
    }
};

} // namespace geyser