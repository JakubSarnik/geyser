#include "aiger_builder.hpp"
#include <format>

// The Aiger to CNF conversion is heavily inspired by the code in IC3Ref
// by Aaron Bradley with the important difference that we don't trivially map
// Aiger variable IDs onto our solver variable IDs.

namespace geyser::builder
{

namespace
{

bool is_true( const context& ctx, aiger_literal lit )
{
    return ctx.true_literals.contains( lit );
}

bool is_false( const context& ctx, aiger_literal lit )
{
    return ctx.true_literals.contains( aiger_not( lit ) ); // NOLINT
}

bool is_decided( const context& ctx, aiger_literal lit )
{
    return is_true( ctx, lit ) || is_false( ctx, lit );
}

void propagate_trues( context& ctx )
{
    ctx.true_literals.emplace( aiger_true );

    for ( auto i = 0u; i < ctx.aig->num_ands; ++i )
    {
        const auto conj = ctx.aig->ands[ i ];
        const auto [ lhs, rhs0, rhs1 ] = conj;

        if ( is_true( ctx, rhs0 ) && is_true( ctx, rhs1 ) )
            ctx.true_literals.emplace( lhs );
        else if ( is_false( ctx, rhs0 ) || is_false( ctx, rhs1 ) )
            ctx.true_literals.emplace( aiger_not( lhs ) ); // NOLINT
    }
}

aiger_literal get_error_literal( const context& ctx )
{
    return ( ctx.aig->num_outputs > 0 ? ctx.aig->outputs[ 0 ] : ctx.aig->bad[ 0 ] ).lit;
}

void determine_coi( context& ctx )
{
    const auto error_literal = get_error_literal( ctx );

    if ( is_decided( ctx, error_literal ) )
        return;

    auto required = std::unordered_set< aiger_literal >{ error_literal };
    auto changed = true;

    while ( changed )
    {
        changed = false;

        for ( auto i = int( ctx.aig->num_ands ) - 1; i >= 0; --i )
        {
            const auto [ lhs, rhs0, rhs1 ] = ctx.aig->ands[ i ];

            if ( !required.contains( lhs ) && !required.contains( aiger_not( lhs ) ) ) // NOLINT
                continue;

            if ( is_decided( ctx, lhs ) )
                continue;

            const auto [ _it0, inserted_rhs0 ] = required.insert( rhs0 );
            const auto [ _it1, inserted_rhs1 ] = required.insert( rhs1 );

            changed |= inserted_rhs0 || inserted_rhs1;
        }

        for ( auto i = 0u; i < ctx.aig->num_latches; ++i )
        {
            const auto& latch = ctx.aig->latches[ i ];

            if ( required.contains( latch.lit ) || required.contains( aiger_not( latch.lit ) ) ) // NOLINT
            {
                const auto [ _it, inserted ] = required.insert( latch.next );
                changed |= inserted;
            }
        }
    }

    for ( auto i = 0u; i < ctx.aig->num_latches; ++i )
    {
        const auto lit = ctx.aig->latches[ i ].lit;

        if ( required.contains( lit ) || required.contains( aiger_not( lit ) ) ) // NOLINT
            ctx.error_coi.insert( lit );
    }
}

// Turn an Aiger declaration
//   lhs = rhs0 /\ rhs1
// into a set of clauses using a Tseitin transformation. This must take care
// when either of rhs0/rhs1 is a constant 0 (false) or 1 (true).
void clausify_and( context& ctx, const aiger_and& conj, cnf_formula& result )
{
    const auto mk_lit = [ & ]( aiger_literal lit )
    {
        return from_aiger_lit( ctx, lit );
    };

    const auto make_equivalence = [ & ]( aiger_literal x, aiger_literal y ){
        // x = y [i.e. x <-> y]
        // ~> (x -> y) /\ (y -> x)
        // ~> (-x \/ y) /\ (-y \/ x)

        result.add_clause( !mk_lit( x ), mk_lit( y ) );
        result.add_clause( !mk_lit( y ), mk_lit( x ) );
    };

    const auto [ lhs, rhs0, rhs1 ] = conj;

    assert( !is_decided( ctx, lhs ) );

    if ( is_true( ctx, rhs0 ) )
        make_equivalence( lhs, rhs1 );
    else if ( is_true( ctx, rhs1 ) )
        make_equivalence( lhs, rhs0 );
    else
    {
        // lhs = rhs0 /\ rhs1
        // ~> (lhs -> rhs0) /\ (lhs -> rhs1) /\ (rhs0 /\ rhs1 -> lhs)
        // ~> (-lhs \/ rhs0) /\ (-lhs \/ rhs1) /\ (-rhs0 \/ -rhs1 \/ lhs)

        result.add_clause( !mk_lit( lhs ), mk_lit( rhs0 ) );
        result.add_clause( !mk_lit( lhs ), mk_lit( rhs1 ) );
        result.add_clause( !mk_lit( rhs0 ), !mk_lit( rhs1 ),mk_lit( lhs ) );
    }
}

// Do a reverse traversal from the Aiger literals representing the results
// through all the ANDs and ending with leaves consisting of state variables
// and inputs.
//
// NOTE: Aiger literals are the numbers in the aiger file
// ([0 .. 2 * (aig.maxvar) + 1]; 0 = false, 1 = true), which does NOT
// correspond to our variable ordering.
//
// Notably, aiger literal's parity denotes whether it's positive (even)
// or negative (odd).
cnf_formula clausify_subgraph( context& ctx, std::unordered_set< aiger_literal > required )
{
    auto result = cnf_formula{};

    for ( auto i = int( ctx.aig->num_ands ) - 1; i >= 0; --i )
    {
        const auto conj = ctx.aig->ands[ i ];
        const auto [ lhs, rhs0, rhs1 ] = conj;

        if ( !required.contains( lhs ) && !required.contains( aiger_not( lhs ) ) ) // NOLINT
            continue;

        if ( is_decided( ctx, lhs ) )
            continue;

        clausify_and( ctx, conj, result );

        required.insert( rhs0 );
        required.insert( rhs1 );
    }

    return result;
}

cnf_formula build_init( context& ctx )
{
    auto init = cnf_formula{};

    for ( auto i = 0u; i < ctx.aig->num_latches; ++i )
    {
        const auto reset = ctx.aig->latches[ i ].reset;

        // In Aiger 1.9, the reset can be either 0 (initialized as false),
        // 1 (initialized as true) or equal to the latch literal, which means
        // that the latch has a nondeterministic initial value.

        if ( aiger_is_constant( reset ) )
            init.add_clause( literal{ ctx.state_vars.nth( int( i ) ), reset == 0 } );
    }

    return init;
}

// For each state variable x and its primed (next state) variant x', add
// a conjunct x' = phi, where phi is the formula represented by the AIG
// subgraph ending in the 'next' literal of the aiger literal for x.
cnf_formula build_trans( context& ctx )
{
    auto roots = std::unordered_set< aiger_literal >{};

    for ( auto i = 0u; i < ctx.aig->num_latches; ++i )
    {
        const auto latch = ctx.aig->latches[ i ];
        const auto aig_literal = latch.lit;
        const auto next_aig_literal = latch.next;

        if ( !is_decided( ctx, next_aig_literal ) && ctx.error_coi.contains( aig_literal ) )
            roots.insert( next_aig_literal );
    }

    auto trans = clausify_subgraph( ctx, std::move( roots ) );

    for ( auto i = 0u; i < ctx.aig->num_latches; ++i )
    {
        const auto latch = ctx.aig->latches[ i ];
        const auto aig_literal = latch.lit;
        const auto next_aig_literal = ctx.aig->latches[ i ].next;

        const auto next = literal{ ctx.next_state_vars.nth( int( i ) ) };

        if ( is_true( ctx, next_aig_literal ) )
            trans.add_clause( next ); // x' = true
        else if ( is_false( ctx, next_aig_literal ) || !ctx.error_coi.contains( aig_literal ) )
            trans.add_clause( !next ); // x' false or don't care
        else
        {
            // x' = phi
            // (x' is stored in next, phi is computed in result_aig_literal)
            // ~> (x' -> phi) /\ (phi -> x')
            // ~> (-x' \/ phi) /\ (-phi \/ x')

            trans.add_clause( !next, from_aiger_lit( ctx, next_aig_literal ) );
            trans.add_clause( !from_aiger_lit( ctx, next_aig_literal ), next );
        }
    }

    return trans;
}

cnf_formula build_error( context& ctx )
{
    const auto error_literal = get_error_literal( ctx );

    if ( is_true( ctx, error_literal ) )
        return cnf_formula::constant( true );
    if ( is_false( ctx, error_literal ) )
        return cnf_formula::constant( false );

    auto error = clausify_subgraph( ctx, { error_literal } );
    // An error happens when the error literal is true.
    error.add_clause( from_aiger_lit( ctx, error_literal ) );

    return error;
}

} // namespace <anonymous>

std::expected< transition_system, std::string > build_from_aiger( variable_store& store, aiger& aig )
{
    return make_context( store, aig ).transform(
            []( context ctx ){ return build_from_context( ctx ); } );
}

std::expected< context, std::string > make_context( variable_store& store, aiger& aig )
{
    if ( aig.num_outputs + aig.num_bad != 1 )
        return std::unexpected( std::format( "The input AIG has to contain precisely"
                                             " one output (aiger <1.9) or precisely one bad specification"
                                             " (aiger 1.9). The input contains {} outputs and {} bad specifications.",
                                             aig.num_outputs, aig.num_bad ));

    if ( aig.num_fairness > 0 || aig.num_justice > 0 )
        return std::unexpected( "Aiger justice constraints and fairness properties"
                                " are not supported." );

    if ( aig.num_constraints > 0 )
        return std::unexpected( "Aiger 1.9 invariant constraints are not"
                                " implemented. Unconstrain the system." );

    // clausify_subgraph depends on ordering of ANDs where each line refers
    // only to literals from previous lines. This, among other things, is
    // ensured by reencoding the AIG.
    if ( aiger_is_reencoded( &aig ) == 0 )
        aiger_reencode( &aig );

    return context
    {
        .aig = &aig,
        .input_vars = store.make_range( int( aig.num_inputs ) ),
        .state_vars = store.make_range( int( aig.num_latches ) ),
        .next_state_vars = store.make_range( int( aig.num_latches ) ),
        .and_vars = store.make_range( int( aig.num_ands ) )
    };
}

transition_system build_from_context( context& ctx )
{
    propagate_trues( ctx );
    determine_coi( ctx );

    return transition_system
    {
        ctx.input_vars, ctx.state_vars, ctx.next_state_vars, ctx.and_vars,
        build_init( ctx ), build_trans( ctx ), build_error( ctx )
    };
}

} // namespace geyser::builder