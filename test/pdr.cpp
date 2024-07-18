#include "engine/pdr.hpp"
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace geyser;
using namespace geyser::pdr;

namespace
{

// TODO: This is duplicated in test/logic.cpp!
std::vector< int > to_nums( const cnf_formula& formula )
{
    auto res = std::vector< int >{};

    for ( const auto lit : formula.literals() )
        res.push_back( lit.value() );

    return res;
}

} // namespace <anonymous>

TEST_CASE( "Cube negation works" )
{
    SECTION( "Empty cube" )
    {
        REQUIRE( to_nums( sorted_cube{ {} }.negate() ) == std::vector{ 0 } );
    }

    SECTION( "Non-empty cube" )
    {
        auto a = literal{ variable{ 1 } };
        auto b = literal{ variable{ 2 } };
        auto c = literal{ variable{ 3 } };

        REQUIRE( to_nums( sorted_cube{ { a } }.negate() )
                 == std::vector{ -1, 0 } );
        REQUIRE( to_nums( sorted_cube{ { !a } }.negate() )
                 == std::vector{ 1, 0 } );
        REQUIRE( to_nums( sorted_cube{ { a, !b, c } }.negate() )
                == std::vector{ 2, -1, -3, 0 } );
        REQUIRE( to_nums( sorted_cube{ { !a, !b, c } }.negate() )
                 == std::vector{ 2, 1, -3, 0 } );
        REQUIRE( to_nums( sorted_cube{ { a, b, c } }.negate() )
                 == std::vector{ -1, -2, -3, 0 } );
        REQUIRE( to_nums( sorted_cube{ { !a, !b, !c } }.negate() )
                 == std::vector{ 3, 2, 1, 0 } );
    }
}

TEST_CASE( "Cube subsumption works" )
{
    const auto mk_cube = []( std::initializer_list< int > vals )
    {
        auto v = std::vector< literal >{};

        for ( auto i : vals )
            v.emplace_back( variable{ std::abs( i ) }, i < 0 );

        return sorted_cube( v );
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