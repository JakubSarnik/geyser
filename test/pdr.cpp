#include "common.hpp"
#include "engine/pdr.hpp"
#include "aiger_builder.hpp"
#include <vector>

using namespace geyser;
using namespace geyser::pdr;

namespace
{

transition_system system_from_aiger( variable_store& store, const char* str )
{
    auto aig = read_aiger( str );
    auto sys = builder::build_from_aiger( store, *aig );

    REQUIRE( sys.has_value() );

    return *sys;
}

counterexample get_counterexample( const result& res )
{
    REQUIRE( std::holds_alternative< counterexample >( res ) );
    return std::get< counterexample >( res );
}

} // namespace <anonymous>

TEST_CASE( "Cube construction works" )
{
    const auto v1 = variable{ 1 };
    const auto v2 = variable{ 2 };
    const auto v3 = variable{ 3 };

    const auto x = literal{ v1 };
    const auto y = literal{ v2 };
    const auto z = literal{ v3 };

    SECTION( "From an empty vector" )
    {
        REQUIRE( to_nums( cube{ {} }.literals() )
                 == std::vector< int >{} );
    }

    SECTION( "From a nonempty vector" )
    {
        REQUIRE( to_nums( cube{ { x, z } }.literals() )
                 == std::vector{ 1, 3 } );
        REQUIRE( to_nums( cube{ { !x, z } }.literals() )
                 == std::vector{ -1, 3 } );
        REQUIRE( to_nums( cube{ { x, y, z } }.literals() )
            == std::vector{ 1, 2, 3 } );
        REQUIRE( to_nums( cube{ { x, !y, z } }.literals() )
                 == std::vector{ 1, -2, 3 } );
        REQUIRE( to_nums( cube{ { !x, !y, !z } }.literals() )
                 == std::vector{ -1, -2, -3 } );
    }
}

TEST_CASE( "Cube negation works" )
{
    SECTION( "Empty cube" )
    {
        REQUIRE( to_nums( cube{ {} }.negate() ) == std::vector{ 0 } );
    }

    SECTION( "Non-empty cube" )
    {
        auto a = literal{ variable{ 1 } };
        auto b = literal{ variable{ 2 } };
        auto c = literal{ variable{ 3 } };

        REQUIRE( to_nums( cube{ { a } }.negate() )
                 == std::vector{ -1, 0 } );
        REQUIRE( to_nums( cube{ { !a } }.negate() )
                 == std::vector{ 1, 0 } );
        REQUIRE( to_nums( cube{ { a, !b, c } }.negate() )
                == std::vector{ -1, 2, -3, 0 } );
        REQUIRE( to_nums( cube{ { !a, !b, c } }.negate() )
                 == std::vector{ 1, 2, -3, 0 } );
        REQUIRE( to_nums( cube{ { a, b, c } }.negate() )
                 == std::vector{ -1, -2, -3, 0 } );
        REQUIRE( to_nums( cube{ { !a, !b, !c } }.negate() )
                 == std::vector{ 1, 2, 3, 0 } );
    }
}

TEST_CASE( "Cube subsumption works" )
{
    const auto mk_cube = []( std::initializer_list< int > vals )
    {
        auto v = std::vector< literal >{};

        for ( auto i : vals )
            v.emplace_back( variable{ std::abs( i ) }, i < 0 );

        return cube( v );
    };

    auto c0 = mk_cube( {} );
    auto c1 = mk_cube( { 1, 2, 3 } );
    auto c2 = mk_cube( { -1, 2, -3 } );
    auto c3 = mk_cube( { 1, 2, 3, 8 } );
    auto c4 = mk_cube( { 2 } );
    auto c5 = mk_cube( { -2 } );
    auto c6 = mk_cube( { 9, 8, 7, 3, 2, 1, -10 } );
    auto c7 = mk_cube( { -2, 2 } );

    REQUIRE( c0.subsumes( c0 ) );
    REQUIRE( c0.subsumes( c1 ) );
    REQUIRE( c1.subsumes( c1 ) );
    REQUIRE( !c1.subsumes( c4 ) );
    REQUIRE( !c1.subsumes( c5 ) );
    REQUIRE( c1.subsumes( c3 ) );
    REQUIRE( c1.subsumes( c6 ) );
    REQUIRE( c2.subsumes( c2 ) );
    REQUIRE( !c2.subsumes( c4 ) );
    REQUIRE( !c2.subsumes( c1 ) );
    REQUIRE( !c3.subsumes( c1 ) );
    REQUIRE( c3.subsumes( c6 ) );
    REQUIRE( !c4.subsumes( c5 ) );
    REQUIRE( c4.subsumes( c6 ) );
    REQUIRE( c4.subsumes( c7 ) );
    REQUIRE( !c5.subsumes( c4 ) );
    REQUIRE( c5.subsumes( c7 ) );
    REQUIRE( !c6.subsumes( c3 ) );
    REQUIRE( !c6.subsumes( c1 ) );
}

TEST_CASE( "Literals are correctly found in ordered cubes" )
{
    const auto v1 = variable{ 1 };
    const auto v2 = variable{ 2 };
    const auto v3 = variable{ 3 };

    const auto x = literal{ v1 };
    const auto y = literal{ v2 };
    const auto z = literal{ v3 };

    SECTION( "Empty cube" )
    {
        const auto c = cube{ {} };

        REQUIRE( !c.find( v1 ).has_value() );
        REQUIRE( !c.find( v2 ).has_value() );
        REQUIRE( !c.find( v3 ).has_value() );
    }

    SECTION( "Single positive literal" )
    {
        const auto c = cube{ { y } };

        REQUIRE( !c.find( v1 ).has_value() );
        REQUIRE( c.find( v2 ).has_value() );
        REQUIRE( *c.find( v2 ) == y );
        REQUIRE( !c.find( v3 ).has_value() );
    }

    SECTION( "Single negative literal" )
    {
        const auto c = cube{ { !y } };

        REQUIRE( !c.find( v1 ).has_value() );
        REQUIRE( c.find( v2 ).has_value() );
        REQUIRE( *c.find( v2 ) == !y );
        REQUIRE( !c.find( v3 ).has_value() );
    }

    SECTION( "Two literals, in order" )
    {
        const auto c = cube{ { x, z } };

        REQUIRE( c.find( v1 ).has_value() );
        REQUIRE( *c.find( v1 ) == x );
        REQUIRE( !c.find( v2 ).has_value() );
        REQUIRE( c.find( v3 ).has_value() );
        REQUIRE( *c.find( v3 ) == z );
    }

    SECTION( "Two literals, out of order" )
    {
        const auto c = cube{ { z, x } };

        REQUIRE( c.find( v1 ).has_value() );
        REQUIRE( *c.find( v1 ) == x );
        REQUIRE( !c.find( v2 ).has_value() );
        REQUIRE( c.find( v3 ).has_value() );
        REQUIRE( *c.find( v3 ) == z );
    }

    SECTION( "Three literals, all positive" )
    {
        const auto c = cube{ { x, y, z } };

        REQUIRE( c.find( v1 ).has_value() );
        REQUIRE( *c.find( v1 ) == x );
        REQUIRE( c.find( v2 ).has_value() );
        REQUIRE( *c.find( v2 ) == y );
        REQUIRE( c.find( v3 ).has_value() );
        REQUIRE( *c.find( v3 ) == z );
    }

    SECTION( "Three literals, all negative" )
    {
        const auto c = cube{ { !x, !y, !z } };

        REQUIRE( c.find( v1 ).has_value() );
        REQUIRE( *c.find( v1 ) == !x );
        REQUIRE( c.find( v2 ).has_value() );
        REQUIRE( *c.find( v2 ) == !y );
        REQUIRE( c.find( v3 ).has_value() );
        REQUIRE( *c.find( v3 ) == !z );
    }

    SECTION( "Three literals, mixed 1" )
    {
        const auto c = cube{ { !x, y, !z } };

        REQUIRE( c.find( v1 ).has_value() );
        REQUIRE( *c.find( v1 ) == !x );
        REQUIRE( c.find( v2 ).has_value() );
        REQUIRE( *c.find( v2 ) == y );
        REQUIRE( c.find( v3 ).has_value() );
        REQUIRE( *c.find( v3 ) == !z );
    }

    SECTION( "Three literals, mixed 2" )
    {
        const auto c = cube{ { x, y, !z } };

        REQUIRE( c.find( v1 ).has_value() );
        REQUIRE( *c.find( v1 ) == x );
        REQUIRE( c.find( v2 ).has_value() );
        REQUIRE( *c.find( v2 ) == y );
        REQUIRE( c.find( v3 ).has_value() );
        REQUIRE( *c.find( v3 ) == !z );
    }
}

TEST_CASE( "CTI pool works" )
{
    const auto c0 = cube{ {} };
    const auto c1 = cube{ to_literals( { 1, 2, 3 } ) };
    const auto c2 = cube{ to_literals( { 1, -2, 3 } ) };
    const auto c3 = cube{ to_literals( { -10, 12 } ) };

    auto pool = cti_pool{};

    const auto check_handle = [ & ]( cti_handle h,
            const cube& s, const cube& i, std::optional< cti_handle > succ )
    {
        REQUIRE( pool.get( h ).state_vars() == s );
        REQUIRE( pool.get( h ).input_vars() == i );
        REQUIRE( pool.get( h ).successor() == succ );
    };

    {
        const auto h1 = pool.make( c1, c2 );

        check_handle( h1, c1, c2, std::nullopt );

        const auto h2 = pool.make( c0, c3, h1 );

        check_handle( h1, c1, c2, std::nullopt );
        check_handle( h2, c0, c3, h1 );
    }

    pool.flush();

    {
        const auto h1 = pool.make( c3, c2 );

        check_handle( h1, c3, c2, std::nullopt );

        const auto h2 = pool.make( c3, c3, h1 );

        check_handle( h1, c3, c2, std::nullopt );
        check_handle( h2, c3, c3, h1 );

        const auto h3 = pool.make( c1, c2, h1 );

        check_handle( h1, c3, c2, std::nullopt );
        check_handle( h2, c3, c3, h1 );
        check_handle( h3, c1, c2, h1 );
    }

    pool.flush();

    {
        const auto h1 = pool.make( c0, c0 );

        check_handle( h1, c0, c0, std::nullopt );
    }

    pool.flush();

    {
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
}

TEST_CASE( "Proof obligations are ordered by level" )
{
    auto pool = cti_pool{};

    const auto c = cube{ {} };

    const auto h1 = pool.make( c, c, {} );
    const auto h2 = pool.make( c, c, {} );
    const auto h3 = pool.make( c, c, {} );

    const auto po1 = proof_obligation{ h1, 0 };
    const auto po2 = proof_obligation{ h1, 1 };
    const auto po3 = proof_obligation{ h2, 1 };
    const auto po4 = proof_obligation{ h3, 0 };
    const auto po5 = proof_obligation{ h3, 2 };

    REQUIRE( po1 < po2 );
    REQUIRE( po1 < po3 );
    REQUIRE( po2 < po5 );
    REQUIRE( po3 < po5 );
    REQUIRE( po4 < po2 );
    REQUIRE( po4 < po2 );
    REQUIRE( po4 < po3 );
    REQUIRE( po4 < po5 );
}

TEST_CASE( "PDR works on a simple system" )
{
    auto store = variable_store{};
    const auto opts = options{ {}, "pdr", verbosity_level::silent, {} };

    auto engine = geyser::pdr::pdr{ opts, store };

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