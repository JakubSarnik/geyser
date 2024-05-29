#include "aiger_builder.hpp"
#include "caiger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace geyser;
using namespace geyser::builder;

namespace
{

aiger_ptr read_aiger( const char* str )
{
    auto aig = make_aiger();
    const auto* const err = aiger_read_from_string( aig.get(), str );

    REQUIRE( err == nullptr );

    return aig;
}

int amount_of( var_id_range range )
{
    return range.second - range.first;
}

std::vector< literal > to_literals( const std::vector< int >& nums )
{
    auto res = std::vector< literal >{};

    for ( const auto num : nums )
    {
        if ( num == 0 )
            res.push_back( literal::separator );
        else
            res.emplace_back( variable{ std::abs( num ) }, num < 0 );
    }

    return res;
}

} // namespace <anonymous>

// ASCII Aiger files pre 1.9 have the following header:
// aag M I L O A, where
// M = maximum variable index
// I = number of inputs
// L = number of latches
// O = number of outputs
// A = number of AND gates

TEST_CASE( "Empty aig" )
{
    auto aig = read_aiger( "aag 0 0 0 0 0\n" );

    auto store = variable_store{};
    auto res = build_from_aiger( store, *aig );

    REQUIRE( !res.has_value() );
}

TEST_CASE( "Buffer gate" )
{
    const auto* const str =
        "aag 1 1 0 1 0\n"
        "2\n"
        "2\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 1 );
        REQUIRE( amount_of( ctx.state_vars ) == 0 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 0 );
        REQUIRE( amount_of( ctx.and_vars ) == 0 );

        const auto input = literal{ variable{ get_var( ctx.input_vars, 0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == input );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !input );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        REQUIRE( res->init().literals().empty() );
        REQUIRE( res->trans().literals().empty() );
        REQUIRE( res->error().literals() == to_literals( { 1, 0 } ) );
    }
}

TEST_CASE( "Inverter gate" )
{
    const auto* const str =
            "aag 1 1 0 1 0\n"
            "2\n"
            "3\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 1 );
        REQUIRE( amount_of( ctx.state_vars ) == 0 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 0 );
        REQUIRE( amount_of( ctx.and_vars ) == 0 );

        const auto input = literal{ variable{ get_var( ctx.input_vars, 0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == input );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !input );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        REQUIRE( res->init().literals().empty() );
        REQUIRE( res->trans().literals().empty() );
        REQUIRE( res->error().literals() == to_literals( { -1, 0 } ) );
    }
}

TEST_CASE( "And gate" )
{
    const auto* const str =
            "aag 3 2 0 1 1\n"
            "2\n"
            "4\n"
            "6\n"
            "6 2 4\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 2 );
        REQUIRE( amount_of( ctx.state_vars ) == 0 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 0 );
        REQUIRE( amount_of( ctx.and_vars ) == 1 );

        const auto in0 = literal{ variable{ get_var( ctx.input_vars, 0 ) } };
        const auto in1 = literal{ variable{ get_var( ctx.input_vars, 1 ) } };
        const auto cnj = literal{ variable{ get_var( ctx.and_vars,   0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == in0 );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !in0 );
        REQUIRE( from_aiger_lit( ctx, 4 ) == in1 );
        REQUIRE( from_aiger_lit( ctx, 5 ) == !in1 );
        REQUIRE( from_aiger_lit( ctx, 6 ) == cnj );
        REQUIRE( from_aiger_lit( ctx, 7 ) == !cnj );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        REQUIRE( res->init().literals().empty() );
        REQUIRE( res->trans().literals().empty() );

        // Inputs: x (1), y (2)
        // Ands: z (3)
        // Original formula: z = x /\ y [output z]
        // As implications: (z -> x) /\ (z -> y) /\ (x /\ y -> z)
        // Our formula: (-z \/ x) /\ (-z \/ y) /\ (-x \/ -y \/ z)
        REQUIRE( res->error().literals() == to_literals( { -3, 1, 0, -3, 2, 0, -1, -2, 3, 0, 3, 0 } ) );
    }
}

TEST_CASE( "Or gate" )
{
    const auto* const str =
            "aag 3 2 0 1 1\n"
            "2\n"
            "4\n"
            "7\n"
            "6 3 5\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 2 );
        REQUIRE( amount_of( ctx.state_vars ) == 0 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 0 );
        REQUIRE( amount_of( ctx.and_vars ) == 1 );

        const auto in0 = literal{ variable{ get_var( ctx.input_vars, 0 ) } };
        const auto in1 = literal{ variable{ get_var( ctx.input_vars, 1 ) } };
        const auto cnj = literal{ variable{ get_var( ctx.and_vars,   0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == in0 );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !in0 );
        REQUIRE( from_aiger_lit( ctx, 4 ) == in1 );
        REQUIRE( from_aiger_lit( ctx, 5 ) == !in1 );
        REQUIRE( from_aiger_lit( ctx, 6 ) == cnj );
        REQUIRE( from_aiger_lit( ctx, 7 ) == !cnj );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        REQUIRE( res->init().literals().empty() );
        REQUIRE( res->trans().literals().empty() );

        // Inputs: x (1), y (2)
        // Ands: z (3)
        // Original formula: z = -x /\ -y [output -z]
        // As implications: (z -> -x) /\ (z -> -y) /\ (-x /\ -y -> z)
        // Our formula: (-z \/ -x) /\ (-z \/ -y) /\ (x \/ y \/ z)
        REQUIRE( res->error().literals() == to_literals( { -3, -1, 0, -3, -2, 0, 1, 2, 3, 0, -3, 0 } ) );
    }
}

TEST_CASE( "Constant latch initialized with false" )
{
    const auto* const str =
            "aag 1 0 1 1 0\n"
            "2 2\n"
            "2\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 0 );
        REQUIRE( amount_of( ctx.state_vars ) == 1 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 1 );
        REQUIRE( amount_of( ctx.and_vars ) == 0 );

        const auto x = literal{ variable{ get_var( ctx.state_vars, 0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == x );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !x );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        // -x
        REQUIRE( res->init().literals() == to_literals( { -1, 0 } ) );
        // x' = x (i.e. (-x' \/ x) /\ (-x \/ x'))
        REQUIRE( res->trans().literals() == to_literals( { -2, 1, 0, -1, 2, 0 } ) );
        // x
        REQUIRE( res->error().literals() == to_literals( { 1, 0 } ) );
    }
}

TEST_CASE( "Constant latch initialized with true" )
{
    const auto* const str =
            "aag 1 0 1 1 0\n"
            "2 2 1\n"
            "2\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 0 );
        REQUIRE( amount_of( ctx.state_vars ) == 1 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 1 );
        REQUIRE( amount_of( ctx.and_vars ) == 0 );

        const auto x = literal{ variable{ get_var( ctx.state_vars, 0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == x );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !x );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        // x
        REQUIRE( res->init().literals() == to_literals( { 1, 0 } ) );
        // x' = x (i.e. (-x' \/ x) /\ (-x \/ x'))
        REQUIRE( res->trans().literals() == to_literals( { -2, 1, 0, -1, 2, 0 } ) );
        // x
        REQUIRE( res->error().literals() == to_literals( { 1, 0 } ) );
    }
}

TEST_CASE( "Simple flip flop" )
{
    const auto* const str =
            "aag 1 0 1 1 0\n"
            "2 3\n"
            "2\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    SECTION( "Context is set up correctly" )
    {
        auto ctx = make_context( store, *aig );

        REQUIRE( ctx.aig == aig.get() );

        REQUIRE( amount_of( ctx.input_vars ) == 0 );
        REQUIRE( amount_of( ctx.state_vars ) == 1 );
        REQUIRE( amount_of( ctx.next_state_vars ) == 1 );
        REQUIRE( amount_of( ctx.and_vars ) == 0 );

        const auto x = literal{ variable{ get_var( ctx.state_vars, 0 ) } };

        REQUIRE( from_aiger_lit( ctx, 2 ) == x );
        REQUIRE( from_aiger_lit( ctx, 3 ) == !x );
    }

    SECTION( "The transition system is correct" )
    {
        auto res = build_from_aiger( store, *aig );

        REQUIRE( res.has_value() );
        // -x
        REQUIRE( res->init().literals() == to_literals( { -1, 0 } ) );
        // x' = -x (i.e. (-x' \/ -x) /\ (x \/ x'))
        REQUIRE( res->trans().literals() == to_literals( { -2, -1, 0, 1, 2, 0 } ) );
        // x
        REQUIRE( res->error().literals() == to_literals( { 1, 0 } ) );
    }
}

TEST_CASE( "More complicated flip flop" )
{
    // TODO
}