#pragma once

#include "formula.hpp"
#include "transition_system.hpp"
#include "caiger.hpp"
#include <utility>
#include <string>
#include <expected>

namespace geyser::builder
{

using aiger_literal = unsigned int;

struct context
{
    aiger* aig;

    var_id_range input_vars;
    var_id_range state_vars;
    var_id_range next_state_vars;
    var_id_range and_vars;
};

inline variable get_var( var_id_range range, int index )
{
    const auto pos = range.first + index;

    assert( pos >= range.first );
    assert( pos < range.second );

    return variable{ pos };
}

inline variable from_aiger_var( context& ctx, aiger_literal lit )
{
    // The aiger lib expects this to be a positive literal (i.e. a variable).
    assert( lit % 2 == 0 );
    assert( lit >= 2 ); // Not constants true/false

    if ( const auto *ptr = aiger_is_input( ctx.aig, lit ); ptr )
        return get_var( ctx.input_vars, int( ptr - ctx.aig->inputs ) );
    if ( const auto *ptr = aiger_is_latch( ctx.aig, lit ); ptr )
        return get_var( ctx.state_vars, int( ptr - ctx.aig->latches ) );
    if ( const auto *ptr = aiger_is_and( ctx.aig, lit ); ptr )
        return get_var( ctx.and_vars, int( ptr - ctx.aig->ands ) );

    assert( false ); // Unreachable
}

inline literal from_aiger_lit( context& ctx, aiger_literal lit )
{
    return literal
    {
        from_aiger_var( ctx, aiger_strip( lit ) ), // NOLINT
        aiger_sign( lit ) == 1 // NOLINT
    };
}

cnf_formula clausify_and( context& ctx, aiger_and conj );

[[nodiscard]]
std::expected< transition_system, std::string > build_from_aiger( variable_store& store, aiger& aig );

cnf_formula build_init( context& ctx );
cnf_formula build_trans( context& ctx );
cnf_formula build_error( context& ctx );

} // namespace geyser::builder