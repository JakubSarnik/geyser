#include "aiger_builder.hpp"

#include <unordered_set>

namespace geyser
{

namespace
{

using aiger_literal = unsigned int;

inline std::string symbol_to_string( std::string prefix, unsigned i, aiger_symbol& symbol )
{
    // Anonymous inputs/state vars get names like y[10]/x[2], respectively.
    if ( symbol.name == nullptr )
        return std::string{ std::move( prefix ) } + "[" + std::to_string( i ) + "]";
    else
        return std::string{ symbol.name };
}

// Turn an Aiger declaration
//   lhs = rhs0 /\ rhs1
// into a set of clauses using a Tseitin transformation. This must take care
// when either of rhs0/rhs1 is a constant 0 (false) or 1 (true).
inline cnf_formula clausify_and( aiger_literal lhs, aiger_literal rhs0, aiger_literal rhs1 )
{
    const auto make_equivalence = []( aiger_literal x, aiger_literal y ){
        // x = y
        // ~> (x -> y) /\ (y -> x)
        // ~> (-x \/ y) /\ (-y \/ x)

        auto res = cnf_formula{};

        // TODO: We need to make new auxiliary Tseitin variables here to
        //       represent lhs. Or rather do it globally in build(), just
        //       as for _state_vars etc!

        return res;
    };

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
        // res.add_clause( /* TODO: Literal! */ )

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

    // TODO + see above

    return res;
}

} // namespace <anonymous>

std::expected< transition_system, std::string > aiger_builder::build( const aiger& aig )
{
    if ( aig.num_outputs + aig.num_bad != 1 )
        return std::unexpected( "the input AIG has to contain precisely one output (aiger <1.9)"
                                " or precisely one bad specification (aiger 1.9)");

    if ( aig.num_fairness > 0 || aig.num_justice > 0 )
        return std::unexpected( "aiger justice constraints and fairness properties"
                                " are not supported" );

    if ( aig.num_constraints > 0 )
        return std::unexpected( "NOT IMPLEMENTED: aiger 1.9 invariant constraints are not"
                                " implemented yet" );

    _input_vars.reserve( aig.num_inputs );
    _state_vars.reserve( aig.num_latches );
    _next_state_vars.reserve( aig.num_latches );

    for ( auto i = 0u; i < aig.num_inputs; ++i )
        _input_vars.push_back( _store->make( symbol_to_string( "y", i, aig.inputs[ i ] ) ) );

    // The following two loops CANNOT be merged because of sequentiality constraints!
    for ( auto i = 0u; i < aig.num_latches; ++i )
        _state_vars.push_back( _store->make( symbol_to_string( "x", i, aig.latches[ i ] ) ) );

    for ( auto i = 0u; i < aig.num_latches; ++i )
        _next_state_vars.push_back( _store->make( symbol_to_string("x'", i, aig.latches[ i ]) ) );

    auto init = build_init( aig );
    auto trans = build_trans( aig );
    auto error = build_error( aig );

    return transition_system
    {
        std::move( _input_vars ),
        std::move( _state_vars ),
        std::move( _next_state_vars ),
        std::move( init ),
        std::move( trans ),
        std::move( error )
    };
}

// The Aiger to CNF conversion is heavily inspired by the code in IC3Ref
// by Aaron Bradley with the important difference that we don't trivially map
// Aiger variable IDs onto our solver variable IDs.

cnf_formula aiger_builder::build_init( const aiger& aig )
{
    auto init = cnf_formula{};

    for ( auto i = 0u; i < aig.num_latches; ++i )
        init.add_clause( literal{ _state_vars[ i ], aig.latches[ i ].reset == 0 } );

    return init;
}

cnf_formula aiger_builder::build_trans( const aiger& aig )
{
    // TODO
    return cnf_formula();
}

cnf_formula aiger_builder::build_error( const aiger& aig )
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

    auto required = std::unordered_set< aiger_literal >{
        ( aig.num_outputs > 0 ? aig.outputs[ 0 ] : aig.bad[ 0 ] ).lit
    };

    // I hate unsigned integers.
    for ( auto i = aig.num_ands - 1; i --> 0; )
    {
        const auto lhs = aig.ands[ i ].lhs;
        const auto rhs0 = aig.ands[ i ].rhs0;
        const auto rhs1 = aig.ands[ i ].rhs1;

        if ( !required.contains( lhs ) && !required.contains( aiger_not( lhs ) ) )
            continue;

        error.add_cnf( clausify_and( lhs, rhs0, rhs1 ) );

        required.insert( rhs0 );
        required.insert( rhs1 );
    }

    return error;
}

} // namespace geyser