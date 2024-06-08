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

struct expected_system
{
    std::vector< int > init;
    std::vector< int > trans;
    std::vector< int > error;
};

void check_system( context& ctx, const expected_system& expected )
{
    const auto res = build_from_context( ctx );

    REQUIRE( res.init().literals() == to_literals( expected.init ) );
    REQUIRE( res.trans().literals() == to_literals( expected.trans ) );
    REQUIRE( res.error().literals() == to_literals( expected.error ) );
}

struct expected_counts
{
    int input_vars;
    int state_vars;
    int next_state_vars;
    int and_vars;
};

void check_sizes( const context& ctx, const expected_counts& expected )
{
    REQUIRE( ctx.input_vars.size() == expected.input_vars );
    REQUIRE( ctx.state_vars.size() == expected.state_vars );
    REQUIRE( ctx.next_state_vars.size() == expected.next_state_vars );
    REQUIRE( ctx.and_vars.size() == expected.and_vars );
}

} // namespace <anonymous>

// ASCII Aiger files pre 1.9 have the following header:
// aag M I L O A, where
//   M = maximum variable index
//   I = number of inputs
//   L = number of latches
//   O = number of outputs
//   A = number of AND gates
// MAKE SURE that all the AIGs here are reencoded, so that the variable
// numberings and order of operands isn't changed by make_context.

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

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
            .input_vars = 1,
            .state_vars = 0,
            .next_state_vars = 0,
            .and_vars = 0
        } );

        const auto input = literal{ ctx->input_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == input );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !input );
    }

    SECTION( "The transition system is correct" )
    {
        const auto system = expected_system
        {
            .init = {},
            .trans = {},
            .error = { 1, 0 }
        };

        check_system( *ctx, system );
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

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 1,
                .state_vars = 0,
                .next_state_vars = 0,
                .and_vars = 0
        } );

        const auto input = literal{ ctx->input_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == input );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !input );
    }

    SECTION( "The transition system is correct" )
    {
        const auto system = expected_system
        {
            .init = {},
            .trans = {},
            .error = { -1, 0 }
        };

        check_system( *ctx, system );
    }
}

TEST_CASE( "And gate" )
{
    const auto* const str =
            "aag 3 2 0 1 1\n"
            "2\n"
            "4\n"
            "6\n"
            "6 4 2\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 2,
                .state_vars = 0,
                .next_state_vars = 0,
                .and_vars = 1
        } );

        const auto in0 = literal{ ctx->input_vars.nth( 0 ) };
        const auto in1 = literal{ ctx->input_vars.nth( 1 ) };
        const auto cnj = literal{ ctx->and_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == in0 );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !in0 );
        REQUIRE( from_aiger_lit( *ctx, 4 ) == in1 );
        REQUIRE( from_aiger_lit( *ctx, 5 ) == !in1 );
        REQUIRE( from_aiger_lit( *ctx, 6 ) == cnj );
        REQUIRE( from_aiger_lit( *ctx, 7 ) == !cnj );
    }

    SECTION( "The transition system is correct" )
    {
        // Inputs: x (1), y (2)
        // Ands: z (3)
        // Original formula: z = y /\ x [output z]
        // As implications: (z -> y) /\ (z -> x) /\ (y /\ x -> z)
        // Our formula: (-z \/ y) /\ (-z \/ x) /\ (-y \/ -x \/ z)

        const auto system = expected_system
        {
            .init = {},
            .trans = {},
            .error = { -3, 2, 0, -3, 1, 0, -2, -1, 3, 0, 3, 0 }
        };

        check_system( *ctx, system );
    }
}

TEST_CASE( "Or gate" )
{
    const auto* const str =
            "aag 3 2 0 1 1\n"
            "2\n"
            "4\n"
            "7\n"
            "6 5 3\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 2,
                .state_vars = 0,
                .next_state_vars = 0,
                .and_vars = 1
        } );

        const auto in0 = literal{ ctx->input_vars.nth( 0 ) };
        const auto in1 = literal{ ctx->input_vars.nth( 1 ) };
        const auto cnj = literal{ ctx->and_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == in0 );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !in0 );
        REQUIRE( from_aiger_lit( *ctx, 4 ) == in1 );
        REQUIRE( from_aiger_lit( *ctx, 5 ) == !in1 );
        REQUIRE( from_aiger_lit( *ctx, 6 ) == cnj );
        REQUIRE( from_aiger_lit( *ctx, 7 ) == !cnj );
    }

    SECTION( "The transition system is correct" )
    {
        const auto system = expected_system
        {
            .init = {},
            .trans = {},
            .error = { -3, -2, 0, -3, -1, 0, 2, 1, 3, 0, -3, 0 }
        };

        check_system( *ctx, system );
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

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 0,
                .state_vars = 1,
                .next_state_vars = 1,
                .and_vars = 0
        } );

        const auto x = literal{ ctx->state_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == x );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !x );
    }

    SECTION( "The transition system is correct" )
    {
        const auto system = expected_system
        {
            .init = { -1, 0 },               // -x
            .trans = { -2, 1, 0, -1, 2, 0 }, // x' = x (i.e. (-x' \/ x) /\ (-x \/ x'))
            .error = { 1, 0 }                // x
        };

        check_system( *ctx, system );
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

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 0,
                .state_vars = 1,
                .next_state_vars = 1,
                .and_vars = 0
        } );

        const auto x = literal{ ctx->state_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == x );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !x );
    }

    SECTION( "The transition system is correct" )
    {
        const auto system = expected_system
        {
            .init = { 1, 0 },                // x
            .trans = { -2, 1, 0, -1, 2, 0 }, // x' = x (i.e. (-x' \/ x) /\ (-x \/ x'))
            .error = { 1, 0 }                // x
        };

        check_system( *ctx, system );
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

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 0,
                .state_vars = 1,
                .next_state_vars = 1,
                .and_vars = 0
        } );

        const auto x = literal{ ctx->state_vars.nth( 0 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == x );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !x );
    }

    SECTION( "The transition system is correct" )
    {
        const auto system = expected_system
        {
            .init = { -1, 0 },               // -x
            .trans = { -2, -1, 0, 1, 2, 0 }, // x' = -x (i.e. (-x' \/ -x) /\ (x \/ x'))
            .error = { 1, 0 }                // x
        };

        check_system( *ctx, system );
    }
}

TEST_CASE( "More complicated flip flop" )
{
    const auto* const str =
            "aag 7 2 1 1 4\n"
            "2\n"
            "4\n"
            "6 14\n"
            "6\n"
            "8 6 2\n"
            "10 7 3\n"
            "12 11 9\n"
            "14 12 4\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    SECTION( "Context is set up correctly" )
    {
        REQUIRE( ctx->aig == aig.get() );

        check_sizes( *ctx, {
                .input_vars = 2,
                .state_vars = 1,
                .next_state_vars = 1,
                .and_vars = 4
        } );

        const auto y0 = literal{ ctx->input_vars.nth( 0 ) };
        const auto y1 = literal{ ctx->input_vars.nth( 1 ) };
        const auto x0 = literal{ ctx->state_vars.nth( 0 ) };
        const auto a0 = literal{ ctx->and_vars.nth( 0 ) };
        const auto a1 = literal{ ctx->and_vars.nth( 1 ) };
        const auto a2 = literal{ ctx->and_vars.nth( 2 ) };
        const auto a3 = literal{ ctx->and_vars.nth( 3 ) };

        REQUIRE( from_aiger_lit( *ctx, 2 ) == y0 );
        REQUIRE( from_aiger_lit( *ctx, 3 ) == !y0 );
        REQUIRE( from_aiger_lit( *ctx, 4 ) == y1 );
        REQUIRE( from_aiger_lit( *ctx, 5 ) == !y1 );

        REQUIRE( from_aiger_lit( *ctx, 6 ) == x0 );
        REQUIRE( from_aiger_lit( *ctx, 7 ) == !x0 );

        REQUIRE( from_aiger_lit( *ctx, 8 ) == a0 );
        REQUIRE( from_aiger_lit( *ctx, 9 ) == !a0 );
        REQUIRE( from_aiger_lit( *ctx, 10 ) == a1 );
        REQUIRE( from_aiger_lit( *ctx, 11 ) == !a1 );
        REQUIRE( from_aiger_lit( *ctx, 12 ) == a2 );
        REQUIRE( from_aiger_lit( *ctx, 13 ) == !a2 );
        REQUIRE( from_aiger_lit( *ctx, 14 ) == a3 );
        REQUIRE( from_aiger_lit( *ctx, 15 ) == !a3 );
    }

    SECTION( "The transition system is correct" )
    {
        const auto y  = std::vector{ 1, 2 };
        const auto x  = std::vector{ 3 };
        const auto xp = std::vector{ 4 };
        const auto a  = std::vector{ 5, 6, 7, 8 };

        // a3  =  a2  /\  y1
        // a2  = -a1  /\ -a0
        // a1  = -x0  /\ -y0
        // a0  =  x0  /\  y0
        // x'0 =  a3

        // (a3  ->  a2) /\ (a3 ->  y1) /\ ( a2 /\  y1 -> a3) /\
        // (a2  -> -a1) /\ (a2 -> -a0) /\ (-a1 /\ -a0 -> a2) /\
        // (a1  -> -x0) /\ (a1 -> -y0) /\ (-x0 /\ -y0 -> a1) /\
        // (a0  ->  x0) /\ (a0 ->  y0) /\ ( x0 /\  y0 -> a0) /\
        // (x'0 ->  a3) /\ (a3 -> x'0)

        // (-a3  \/  a2) /\ (-a3 \/  y1) /\ (-a2 \/ -y1 \/ a3)
        // (-a2  \/ -a1) /\ (-a2 \/ -a0) /\ ( a1 \/  a0 \/ a2)
        // (-a1  \/ -x0) /\ (-a1 \/ -y0) /\ ( x0 \/  y0 \/ a1)
        // (-a0  \/  x0) /\ (-a0 \/  y0) /\ (-x0 \/ -y0 \/ a0)
        // (-x'0 \/  a3) /\ (-a3 \/ x'0)

        const auto system = expected_system
        {
            .init = { -x[ 0 ], 0 },
            .trans =
            {
                 -a[ 3 ],  a[ 2 ], 0, -a[ 3 ],  y[ 1 ], 0, -a[ 2 ], -y[ 1 ], a[ 3 ], 0,
                 -a[ 2 ], -a[ 1 ], 0, -a[ 2 ], -a[ 0 ], 0,  a[ 1 ],  a[ 0 ], a[ 2 ], 0,
                 -a[ 1 ], -x[ 0 ], 0, -a[ 1 ], -y[ 0 ], 0,  x[ 0 ],  y[ 0 ], a[ 1 ], 0,
                 -a[ 0 ],  x[ 0 ], 0, -a[ 0 ],  y[ 0 ], 0, -x[ 0 ], -y[ 0 ], a[ 0 ], 0,
                -xp[ 0 ],  a[ 3 ], 0, -a[ 3 ], xp[ 0 ], 0
            },
            .error = { x[ 0 ], 0 }
        };

        check_system( *ctx, system );
    }
}

TEST_CASE( "More complicated flip flop with negated next state function" )
{
    const auto* const str =
            "aag 7 2 1 1 4\n"
            "2\n"
            "4\n"
            "6 15\n"
            "6\n"
            "8 6 2\n"
            "10 7 3\n"
            "12 11 9\n"
            "14 12 4\n";

    auto aig = read_aiger( str );
    auto store = variable_store{};

    const auto y  = std::vector{ 1, 2 };
    const auto x  = std::vector{ 3 };
    const auto xp = std::vector{ 4 };
    const auto a  = std::vector{ 5, 6, 7, 8 };

    auto ctx = make_context( store, *aig );
    REQUIRE( ctx.has_value() );

    // a3  =  a2  /\  y1
    // a2  = -a1  /\ -a0
    // a1  = -x0  /\ -y0
    // a0  =  x0  /\  y0
    // x'0 = -a3

    // (a3  ->  a2) /\ ( a3 ->  y1) /\ ( a2 /\  y1 -> a3) /\
    // (a2  -> -a1) /\ ( a2 -> -a0) /\ (-a1 /\ -a0 -> a2) /\
    // (a1  -> -x0) /\ ( a1 -> -y0) /\ (-x0 /\ -y0 -> a1) /\
    // (a0  ->  x0) /\ ( a0 ->  y0) /\ ( x0 /\  y0 -> a0) /\
    // (x'0 -> -a3) /\ (-a3 -> x'0)

    // (-a3  \/  a2) /\ (-a3 \/  y1) /\ (-a2 \/ -y1 \/ a3)
    // (-a2  \/ -a1) /\ (-a2 \/ -a0) /\ ( a1 \/  a0 \/ a2)
    // (-a1  \/ -x0) /\ (-a1 \/ -y0) /\ ( x0 \/  y0 \/ a1)
    // (-a0  \/  x0) /\ (-a0 \/  y0) /\ (-x0 \/ -y0 \/ a0)
    // (-x'0 \/ -a3) /\ ( a3 \/ x'0)

    const auto system = expected_system
    {
        .init = { -x[ 0 ], 0 },
        .trans =
        {
             -a[ 3 ],  a[ 2 ], 0, -a[ 3 ],  y[ 1 ], 0, -a[ 2 ], -y[ 1 ], a[ 3 ], 0,
             -a[ 2 ], -a[ 1 ], 0, -a[ 2 ], -a[ 0 ], 0,  a[ 1 ],  a[ 0 ], a[ 2 ], 0,
             -a[ 1 ], -x[ 0 ], 0, -a[ 1 ], -y[ 0 ], 0,  x[ 0 ],  y[ 0 ], a[ 1 ], 0,
             -a[ 0 ],  x[ 0 ], 0, -a[ 0 ],  y[ 0 ], 0, -x[ 0 ], -y[ 0 ], a[ 0 ], 0,
            -xp[ 0 ], -a[ 3 ], 0,  a[ 3 ], xp[ 0 ], 0
        },
        .error = { x[ 0 ], 0 }
    };

    check_system( *ctx, system );
}