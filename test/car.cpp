#include "common.hpp"
#include "engine/car.hpp"

using namespace geyser;
using namespace geyser::car;

// Almost the same tests as for PDR.

TEST_CASE( "Cotrace pool works" )
{
    const auto c0 = cube{ {} };
    const auto c1 = cube{ to_literals( { 1, 2, 3 } ) };
    const auto c2 = cube{ to_literals( { 1, -2, 3 } ) };
    const auto c3 = cube{ to_literals( { -10, 12 } ) };

    auto pool = cotrace_pool{};

    const auto check_handle = [ & ]( bad_cube_handle h,
                                     const cube& s, const cube& i, std::optional< bad_cube_handle > succ )
    {
        REQUIRE( pool.get( h ).state_vars() == s );
        REQUIRE( pool.get( h ).input_vars() == i );
        REQUIRE( pool.get( h ).successor() == succ );
    };

    const auto h1 = pool.make( c1, c1 );

    check_handle( h1, c1, c1, std::nullopt );

    const auto h2 = pool.make( c2, c1, std::nullopt );

    check_handle( h1, c1, c1, std::nullopt );
    check_handle( h2, c2, c1, std::nullopt );

    const auto h3 = pool.make( c2, c3, h1 );

    check_handle( h1, c1, c1, std::nullopt );
    check_handle( h2, c2, c1, std::nullopt );
    check_handle( h3, c2, c3, h1 );

    const auto h4 = pool.make( c3, c1, h1 );

    check_handle( h1, c1, c1, std::nullopt );
    check_handle( h2, c2, c1, std::nullopt );
    check_handle( h3, c2, c3, h1 );
    check_handle( h4, c3, c1, h1 );
}

TEST_CASE( "Proof obligations are ordered by level in CAR" )
{
    auto pool = cotrace_pool{};

    const auto c = cube{ {} };

    const auto h1 = pool.make( c, c, {} );
    const auto h2 = pool.make( c, c, {} );
    const auto h3 = pool.make( c, c, {} );

    const auto po1 = proof_obligation{ h1, 0, 0 };
    const auto po2 = proof_obligation{ h1, 1, 1 };
    const auto po3 = proof_obligation{ h2, 1, 1 };
    const auto po4 = proof_obligation{ h3, 0, 2 };
    const auto po5 = proof_obligation{ h3, 2, 1 };

    REQUIRE( po1 < po2 );
    REQUIRE( po1 < po3 );
    REQUIRE( po2 < po5 );
    REQUIRE( po3 < po5 );
    REQUIRE( po4 < po2 );
    REQUIRE( po4 < po2 );
    REQUIRE( po4 < po3 );
    REQUIRE( po4 < po5 );
}

TEST_CASE( "Forward CAR works on a simple system" )
{
    auto store = variable_store{};
    const auto opts = options{ {}, "pdr", verbosity_level::silent, {} };

    auto engine = forward_car{ opts, store };

    SECTION( "Unsafe initial state" )
    {
        // 0 -> 1, 0 initial, 0 error
        const auto* const str =
                "aag 1 0 1 1 0\n"
                "2 1\n"
                "3\n";

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );
        const auto cex = get_counterexample( res );

        const auto x = literal{ system.state_vars().nth( 0 ) };
        REQUIRE( cex.initial_state() == std::vector{ !x } );
        REQUIRE( cex.inputs().size() == 1 );
        REQUIRE( cex.inputs()[ 0 ].empty() );
    }

    SECTION( "Unsafe when input is true in the initial state" )
    {
        // 0 -> 1, 0 initial, 0 error under input 1
        const auto* const str =
                "aag 2 1 1 1 0\n"
                "2\n"
                "4 1\n"
                "2\n";

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );
        const auto cex = get_counterexample( res );

        const auto x = literal{ system.state_vars().nth( 0 ) };
        const auto i = literal{ system.input_vars().nth( 0 ) };

        REQUIRE( cex.initial_state() == std::vector{ !x } );
        REQUIRE( cex.inputs().size() == 1 );
        REQUIRE( cex.inputs()[ 0 ] == std::vector{ i } );
    }

    SECTION( "Unsafe when input is false in the initial state" )
    {
        // 0 -> 1, 0 initial, 0 error under input 0
        const auto* const str =
                "aag 2 1 1 1 0\n"
                "2\n"
                "4 1\n"
                "3\n";

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );
        const auto cex = get_counterexample( res );

        const auto x = literal{ system.state_vars().nth( 0 ) };
        const auto i = literal{ system.input_vars().nth( 0 ) };

        REQUIRE( cex.initial_state() == std::vector{ !x } );
        REQUIRE( cex.inputs().size() == 1 );
        REQUIRE( cex.inputs()[ 0 ] == std::vector{ !i } );
    }

    SECTION( "Unsafe state in one step" )
    {
        // 0 -> 1, 0 initial, 1 error
        const auto* const str =
                "aag 1 0 1 1 0\n"
                "2 1\n"
                "2\n";

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );
        const auto cex = get_counterexample( res );

        const auto x = literal{ system.state_vars().nth( 0 ) };

        // Actually technically two steps, the first brings us from 0 to 1 and
        // the second from 1 to "error".
        REQUIRE( cex.initial_state() == std::vector{ !x } );
        REQUIRE( cex.inputs().size() == 2 );
        REQUIRE( cex.inputs()[ 0 ].empty() );
        REQUIRE( cex.inputs()[ 1 ].empty() );
    }

    SECTION( "Unsafe four state system" )
    {
        // 0 0 -> 1 0
        //  v      v
        // 0 1 -> 1 1
        //
        // x y = 0 0 is initial, 1 1 is error
        // Single input i: when 0, enable x, when 1, enable y

        const auto* const str =
                "aag 10 1 2 1 7\n"
                "2\n"         // i
                "4 19\n"      // x
                "6 21\n"      // y
                "12\n"        // error on x /\ y
                "8 5 3\n"     // -x /\ -i
                "10 7 2\n"    // -y /\ i
                "12 4 6\n"    // x /\ y
                "14 4 2\n"    // x /\ i
                "16 6 3\n"    // y /\ -i
                "18 9 15\n"   // -[ (-x /\ -i) \/ (x /\ i) ]
                "20 11 17\n"; // -[ (-y /\ i) \/ (y /\ -i) ]

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );
        const auto cex = get_counterexample( res );

        const auto x = literal{ system.state_vars().nth( 0 ) };
        const auto y = literal{ system.state_vars().nth( 1 ) };
        const auto i = literal{ system.input_vars().nth( 0 ) };

        REQUIRE( cex.initial_state() == std::vector{ !x, !y } );
        REQUIRE( cex.inputs().size() == 3 );

        const auto upper_path =
                cex.inputs()[ 0 ] == std::vector{ !i } &&
                cex.inputs()[ 1 ] == std::vector{ i };

        const auto lower_path =
                cex.inputs()[ 0 ] == std::vector{ i } &&
                cex.inputs()[ 1 ] == std::vector{ !i };

        REQUIRE( ( upper_path || lower_path ) );
    }

    SECTION( "Trivially safe four state system" )
    {
        const auto* const str =
                "aag 10 1 2 1 7\n"
                "2\n"         // i
                "4 19\n"      // x
                "6 21\n"      // y
                "0\n"         // error is False
                "8 5 3\n"     // -x /\ -i
                "10 7 2\n"    // -y /\ i
                "12 4 6\n"    // x /\ y
                "14 4 2\n"    // x /\ i
                "16 6 3\n"    // y /\ -i
                "18 9 15\n"   // -[ (-x /\ -i) \/ (x /\ i) ]
                "20 11 17\n"; // -[ (-y /\ i) \/ (y /\ -i) ]

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );

        REQUIRE( std::holds_alternative< ok >( res ) );
    }

    SECTION( "Non-trivially safe two state system" )
    {
        // States 0 and 1, self loops, 0 initial, 1 error
        const auto* const str =
                "aag 1 0 1 1 0\n"
                "2 2\n"
                "2\n";

        const auto system = system_from_aiger( store, str );
        auto res = engine.run( system );

        REQUIRE( std::holds_alternative< ok >( res ) );
    }
}