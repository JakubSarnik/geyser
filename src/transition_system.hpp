#pragma once

#include "formula.hpp"
#include <vector>
#include <cassert>

namespace geyser
{

class transition_system
{
    using variables = std::vector< variable >;

    // The variables have to be stored in successive sequential order (i.e., if
    // _state_vars begins with variable 3 and stores 4 variables, it contains
    // 3, 4, 5, 6). This is necessary for quick substitutions.
    variables _input_vars;
    variables _state_vars;
    variables _next_state_vars;

    cnf_formula _init; // Contains _state_vars
    cnf_formula _trans; // Contains _input_vars, _state_vars and _next_state_vars
    cnf_formula _error; // Contains _state_vars

    // Note that the formulas also contain additional variables used for Tseitin
    // encoding purposes.

public:
    transition_system( variables input_vars, variables state_vars, variables next_state_vars,
                       cnf_formula init, cnf_formula trans, cnf_formula error )
            : _input_vars{ std::move( input_vars ) }, _state_vars{ std::move( state_vars ) },
              _next_state_vars{ std::move( next_state_vars ) },
              _init{ std::move( init ) }, _trans{ std::move( trans ) }, _error{ std::move( error ) }
    {
        // This is obviously not a complete proof that the var vectors are
        // correct (see the comment above), but it's a nice cheap sanity check.
        assert( _input_vars.back().id() - _input_vars.front().id() + 1 == _input_vars.size());
        assert( _state_vars.back().id() - _state_vars.front().id() + 1 == _state_vars.size());
        assert( _next_state_vars.back().id() - _next_state_vars.front().id() + 1 == _next_state_vars.size());
        assert( _state_vars.size() == _next_state_vars.size() );
    }
};

} // namespace geyser