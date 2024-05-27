#include "aiger_builder.hpp"

#include <format>
#include <unordered_set>

namespace geyser::builder
{

namespace
{

inline std::string symbol_to_string( const std::string& prefix, unsigned i, aiger_symbol& symbol )
{
    // Anonymous inputs/state vars get names like y[10]/x[2], respectively.
    if ( symbol.name == nullptr )
        return std::format( "{}[{}]", prefix, i );
    else // NOLINT else after return is fine here
        return std::string{ symbol.name };
}

} // namespace <anonymous>

std::expected< transition_system, std::string > build_from_aiger( variable_store& store, aiger& aig )
{
    if ( aig.num_outputs + aig.num_bad != 1 )
        return std::unexpected( std::format( "The input AIG has to contain precisely"
                                             "one output (aiger <1.9) or precisely one bad specification"
                                             "(aiger 1.9). The input contains {} outputs and {} bad specifications",
                                             aig.num_outputs, aig.num_bad ));

    if ( aig.num_fairness > 0 || aig.num_justice > 0 )
        return std::unexpected( "Aiger justice constraints and fairness properties"
                                " are not supported." );

    if ( aig.num_constraints > 0 )
        return std::unexpected( "NOT IMPLEMENTED: aiger 1.9 invariant constraints are not"
                                " implemented yet" );

    auto ctx = make_context( store, aig );

    auto init = build_init( ctx );
    auto trans = build_trans( ctx );
    auto error = build_error( ctx );

    return transition_system{ ctx.input_vars, ctx.state_vars, ctx.next_state_vars,
                              std::move( init ), std::move( trans ), std::move( error ) };
}

context make_context( variable_store& store, aiger& aig )
{
    return context
    {
            .aig = &aig,

            .input_vars = store.make_range( int( aig.num_inputs ), [ & ]( int i )
            {
                return symbol_to_string( "y", i, aig.inputs[ i ] );
            }),

            .state_vars = store.make_range( int( aig.num_latches ), [ & ]( int i )
            {
                return symbol_to_string( "x", i, aig.latches[ i ] );
            }),

            .next_state_vars = store.make_range( int( aig.num_latches ), [ & ]( int i )
            {
                return symbol_to_string( "x'", i, aig.latches[ i ] );
            }),

            .and_vars = store.make_range( int( aig.num_ands ), []( int i )
            {
                return std::format("and[{}]", i);
            } )
    };
}

// The Aiger to CNF conversion is heavily inspired by the code in IC3Ref
// by Aaron Bradley with the important difference that we don't trivially map
// Aiger variable IDs onto our solver variable IDs.

// Turn an Aiger declaration
//   lhs = rhs0 /\ rhs1
// into a set of clauses using a Tseitin transformation. This must take care
// when either of rhs0/rhs1 is a constant 0 (false) or 1 (true).
cnf_formula clausify_and( context& ctx, aiger_and conj )
{
    const auto mk_lit = [ & ]( aiger_literal lit )
    {
        return from_aiger_lit( ctx, lit );
    };

    const auto make_equivalence = [ & ]( aiger_literal x, aiger_literal y ){
        // x = y [i.e. x <-> y]
        // ~> (x -> y) /\ (y -> x)
        // ~> (-x \/ y) /\ (-y \/ x)

        auto res = cnf_formula{};

        res.add_clause( !mk_lit( x ), mk_lit( y ) );
        res.add_clause( !mk_lit( y ), mk_lit( x ) );

        return res;
    };

    const auto [ lhs, rhs0, rhs1 ] = conj;

    // Anything AND false is a contradiction, i.e. a formula with an empty
    // clause.
    if ( rhs0 == aiger_false || rhs1 == aiger_false )
    {
        auto res = cnf_formula{};
        res.add_clause( {} );

        return res;
    }

    // lhs = true
    if ( rhs0 == aiger_true && rhs1 == aiger_true )
    {
        auto res = cnf_formula{};
        res.add_clause( mk_lit( lhs ) );

        return res;
    }

    if ( rhs0 == aiger_true )
        return make_equivalence( lhs, rhs1 );

    if ( rhs1 == aiger_true )
        return make_equivalence( lhs, rhs0 );

    auto res = cnf_formula{};

    // lhs = rhs0 /\ rhs1
    // ~> (lhs -> rhs0) /\ (lhs -> rhs1) /\ (rhs0 /\ rhs1 -> lhs)
    // ~> (-lhs \/ rhs0) /\ (-lhs \/ rhs1) /\ (-rhs0 \/ -rhs1 \/ lhs)

    res.add_clause( !mk_lit( lhs ), mk_lit( rhs0 ) );
    res.add_clause( !mk_lit( lhs ), mk_lit( rhs1 ) );
    res.add_clause( !mk_lit( rhs0 ), !mk_lit( rhs1 ),mk_lit( lhs ) );

    return res;
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
            init.add_clause( literal{ get_var( ctx.input_vars, int( i ) ), reset == 0 } );
    }

    return init;
}

cnf_formula build_trans( context& ctx )
{
    return {}; // TODO
}

cnf_formula build_error( context& ctx )
{
    // Do a reverse traversal from the Aiger literal representing the error
    // (either the single output or the single bad property) through all the
    // ANDs and ending with leaves consisting of state variables and inputs.
    //
    // NOTE: Aiger literals are the numbers in the aiger file
    // ([0 .. 2 * (aig.maxvar) + 1]; 0 = false, 1 = true), which does NOT
    // correspond to our variable ordering in any way.
    //
    // Notably, aiger literal's parity denotes whether it's positive (even)
    // or negative (odd).

    auto error = cnf_formula{};
    const auto error_literal = ( ctx.aig->num_outputs > 0 ? ctx.aig->outputs[ 0 ] : ctx.aig->bad[ 0 ] ).lit;

    auto required = std::unordered_set< aiger_literal >{ error_literal };

    for ( auto i = int( ctx.aig->num_ands ) - 1; i >= 0; --i )
    {
        const auto conj = ctx.aig->ands[ i ];
        const auto [ lhs, rhs0, rhs1 ] = conj;

        if ( !required.contains( lhs ) && !required.contains( aiger_not( lhs ) ) ) // NOLINT
            continue;

        error.add_cnf( clausify_and( ctx, conj ) );

        required.insert( rhs0 );
        required.insert( rhs1 );
    }

    // If by now the formula is still empty, then the error literal must not be
    // a result of an and line and as such must be either directly an input or
    // a state variable.
    if ( error.literals().empty() )
        error.add_cnf( clausify_and( ctx, aiger_and
        {
            .lhs = error_literal,
            .rhs0 = aiger_true,
            .rhs1 = aiger_true
        } ) );

    return error;
}

} // namespace geyser::builder