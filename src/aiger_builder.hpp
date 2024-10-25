#pragma once

#include "logic.hpp"
#include "transition_system.hpp"
#include "caiger.hpp"
#include <utility>
#include <string>
#include <expected>
#include <exception>
#include <unordered_set>

namespace geyser::builder
{

using aiger_literal = unsigned int;

struct context
{
    aiger* aig;

    variable_range input_vars;
    variable_range state_vars;
    variable_range next_state_vars;
    variable_range and_vars;

    // Aiger literals that are necessarily true. For any literal here, its
    // negation aiger_not( lit ) is necessarily false. This is used to
    // propagate constants and simplify the clausified formula.
    std::unordered_set< aiger_literal > true_literals;

    // The cone of influence of the error formula. Contains literals
    // corresponding to the latches that are necessary to decide the value
    // of the error literal. Any other latch x can have x' = false in the
    // transition formula and this won't compromise correctness.
    std::unordered_set< aiger_literal > error_coi;
};

inline literal from_aiger_lit( context& ctx, aiger_literal lit )
{
    const auto from_aiger_var = [ & ]( aiger_literal var )
    {
        // The aiger lib expects this to be a positive literal (i.e. a variable).
        assert( var % 2 == 0 );
        assert( var >= 2 ); // Not constants true/false

        if ( const auto *ptr = aiger_is_input( ctx.aig, var ); ptr )
            return ctx.input_vars.nth( int( ptr - ctx.aig->inputs ) );
        if ( const auto *ptr = aiger_is_latch( ctx.aig, var ); ptr )
            return ctx.state_vars.nth( int( ptr - ctx.aig->latches ) );
        if ( const auto *ptr = aiger_is_and( ctx.aig, var ); ptr )
            return ctx.and_vars.nth( int( ptr - ctx.aig->ands ) );

        std::terminate(); // Unreachable
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