#pragma once

#include "logic.hpp"
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

inline literal from_aiger_lit( context& ctx, aiger_literal lit )
{
    const auto from_aiger_var = [ & ]( aiger_literal var )
    {
        // The aiger lib expects this to be a positive literal (i.e. a variable).
        assert( var % 2 == 0 );
        assert( var >= 2 ); // Not constants true/false

        if ( const auto *ptr = aiger_is_input( ctx.aig, var ); ptr )
            return get_var( ctx.input_vars, int( ptr - ctx.aig->inputs ) );
        if ( const auto *ptr = aiger_is_latch( ctx.aig, var ); ptr )
            return get_var( ctx.state_vars, int( ptr - ctx.aig->latches ) );
        if ( const auto *ptr = aiger_is_and( ctx.aig, var ); ptr )
            return get_var( ctx.and_vars, int( ptr - ctx.aig->ands ) );

        assert( false ); // Unreachable
    };

    return literal
    {
        from_aiger_var( aiger_strip( lit ) ), // NOLINT
        aiger_sign( lit ) == 1 // NOLINT
    };
}

[[nodiscard]]
std::expected< transition_system, std::string > build_from_aiger( variable_store& store, aiger& aig );

[[nodiscard]]
std::expected< context, std::string > make_context( variable_store& store, aiger& aig );

[[nodiscard]]
transition_system build_from_context( context& ctx );


} // namespace geyser::builder