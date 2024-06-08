#include "logic.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <format>

using namespace geyser;

namespace
{

std::vector< int > to_nums( const cnf_formula& formula )
{
    auto res = std::vector< int >{};

    for ( const auto lit : formula.literals() )
        res.push_back( lit.value() );

    return res;
}

} // namespace <anonymous>

TEST_CASE( "Variables have the expected ids" )
{
    auto store = variable_store{};

    auto x = store.make();
    auto y = store.make();

    REQUIRE( x.id() == 1 );
    REQUIRE( y.id() == 2 );
}

TEST_CASE( "Variable store hands out different variables" )
{
    auto store = variable_store{};

    auto x = store.make();
    auto y = store.make();

    REQUIRE( x != y );
}

TEST_CASE( "Variables have the expected names" )
{
    auto store = variable_store{};

    auto x = store.make( "foo" );
    auto y = store.make( "bar" );

    REQUIRE( store.get_name( x ) == "foo" );
    REQUIRE( store.get_name( y ) == "bar" );
    REQUIRE( store.get_name( variable{ 1 } ) == "foo" );
}

TEST_CASE( "Variable ranges have the expected sizes" )
{
    SECTION( "Empty range" )
    {
        REQUIRE( variable_range{ 1, 1 }.size() == 0 );
        REQUIRE( variable_range{ 3, 3 }.size() == 0 );
    }

    SECTION( "Unit range" )
    {
        REQUIRE( variable_range{ 1, 2 }.size() == 1 );
        REQUIRE( variable_range{ 3, 4 }.size() == 1 );
    }

    SECTION( "Longer range" )
    {
        REQUIRE( variable_range{ 1, 5 }.size() == 4 );
        REQUIRE( variable_range{ 15, 20 }.size() == 5 );
    }
}

TEST_CASE( "Variable ranges contain what they should contain" )
{
    SECTION( "Element is there" )
    {
        REQUIRE( variable_range{ 1, 9 }.contains( variable{ 1 } ) );
        REQUIRE( variable_range{ 1, 9 }.contains( variable{ 3 } ) );
        REQUIRE( variable_range{ 1, 9 }.contains( variable{ 6 } ) );
    }

    SECTION( "Element is not there" )
    {
        REQUIRE( !variable_range{ 1, 9 }.contains( variable{ 9 } ) );
        REQUIRE( !variable_range{ 1, 9 }.contains( variable{ 10 } ) );
        REQUIRE( !variable_range{ 1, 9 }.contains( variable{ 15 } ) );
        REQUIRE( !variable_range{ 3, 6 }.contains( variable{ 2 } ) );
    }
}

TEST_CASE( "Variable ranges are correctly iterable" )
{
    const auto range = variable_range{ 4, 6 };
    auto it = range.begin();

    REQUIRE( *it == variable{ 4 } );

    it++;
    REQUIRE( *it == variable{ 5 } );

    ++it;
    REQUIRE( it == range.end() );

    it--;
    --it;
    REQUIRE( it == range.begin() );
}

TEST_CASE( "Nth and offset works for ranges" )
{
    const auto range = variable_range{ 2, 5 };

    REQUIRE( range.nth( 0 ) == variable{ 2 } );
    REQUIRE( range.nth( 1 ) == variable{ 3 } );
    REQUIRE( range.nth( 2 ) == variable{ 4 } );

    REQUIRE( range.offset( variable{ 2 } ) == 0 );
    REQUIRE( range.offset( variable{ 3 } ) == 1 );
    REQUIRE( range.offset( variable{ 4 } ) == 2 );
}

TEST_CASE( "Variable store hands out ranges correctly" )
{
    auto store = variable_store{};

    const auto r1 = store.make_range( 3 );

    REQUIRE( r1.size() == 3 );
    REQUIRE( r1.contains( variable{ 1 } ) );
    REQUIRE( r1.contains( variable{ 2 } ) );
    REQUIRE( r1.contains( variable{ 3 } ) );

    const auto r2 = store.make_range( 5 );

    REQUIRE( r2.size() == 5 );
    REQUIRE( r2.contains( variable{ 4 } ) );
    REQUIRE( r2.contains( variable{ 5 } ) );
    REQUIRE( r2.contains( variable{ 6 } ) );
    REQUIRE( r2.contains( variable{ 7 } ) );
    REQUIRE( r2.contains( variable{ 8 } ) );
}

TEST_CASE( "Ranges are named correctly" )
{
    auto store = variable_store{};

    SECTION( "when no name is present" )
    {
        const auto range = store.make_range( 4 );

        for ( const auto var : range )
            REQUIRE( store.get_name( var ) == "" );
    }

    SECTION( "when a constant name is present" )
    {
        const auto namer = []( int )
        {
            return "name";
        };

        const auto range = store.make_range( 4, namer );

        for ( const auto var : range )
            REQUIRE( store.get_name( var ) == "name" );
    }

    SECTION( "when a dynamic name is present" )
    {
        const auto namer = []( int i )
        {
            return std::format("x{}", i);
        };

        const auto range = store.make_range( 3, namer );

        REQUIRE( store.get_name( range.nth( 0 ) ) == "x0" );
        REQUIRE( store.get_name( range.nth( 1 ) ) == "x1" );
        REQUIRE( store.get_name( range.nth( 2 ) ) == "x2" );
    }
}

TEST_CASE( "Literals have the expected IDs and values" )
{
    auto store = variable_store{};

    auto x = store.make();
    auto y = store.make();

    auto lx = literal{ x };
    auto ly = literal{ y };

    REQUIRE( lx.var() == x );
    REQUIRE( lx.value() == 1 );
    REQUIRE( lx.sign() == true );

    REQUIRE( ly.var() == y );
    REQUIRE( ly.value() == 2 );
    REQUIRE( ly.sign() == true );
}

TEST_CASE( "Literals are negated correctly" )
{
    auto store = variable_store{};

    auto var = store.make();

    SECTION( "Using the constructor" )
    {
        auto lit = literal{ var, true };

        REQUIRE( lit.var() == var );
        REQUIRE( lit.value() == -1 );
        REQUIRE( lit.sign() == false );
    }

    SECTION( "Using the negation operator" )
    {
        auto lit = !literal{ var };

        REQUIRE( lit.var() == var );
        REQUIRE( lit.value() == -1 );
        REQUIRE( lit.sign() == false );
    }
}

TEST_CASE( "Literals of different polarity are different" )
{
    auto store = variable_store{};
    auto var = store.make();
    auto lit = literal{ var };

    REQUIRE( lit != !lit );
}

TEST_CASE( "Literal substitution works as expected" )
{
    const auto v1 = variable{ 1 };
    const auto v2 = variable{ 2 };

    const auto lit = literal{ v1 };

    REQUIRE( lit.substitute( v2 ) == literal{ v2 } );
    REQUIRE( (!lit).substitute( v2 ) == !literal{ v2 } );
}

TEST_CASE( "CNF formula is built correctly using add_clause" )
{
    auto store = variable_store{};
    auto formula = cnf_formula{};

    REQUIRE( formula.literals().empty() );

    auto a = literal{ store.make() };
    auto b = literal{ store.make() };

    formula.add_clause( { a, b } );

    REQUIRE( formula.literals() == std::vector{ a, b, literal::separator } );
    REQUIRE( to_nums( formula ) == std::vector{ 1, 2, 0 } );

    formula.add_clause( !a );

    REQUIRE( to_nums( formula ) == std::vector{ 1, 2, 0, -1, 0 } );

    auto c = literal{ store.make() };

    formula.add_clause( c, !c );

    REQUIRE( to_nums( formula ) == std::vector{ 1, 2, 0, -1, 0, 3, -3, 0 } );

    formula.add_clause( {} );

    REQUIRE( to_nums( formula ) == std::vector{ 1, 2, 0, -1, 0, 3, -3, 0, 0 } );
}

TEST_CASE( "CNF formula is built correctly using add_cnf" )
{
    auto store = variable_store{};

    auto f1 = cnf_formula{};

    auto a = literal{ store.make() };
    auto b = literal{ store.make() };

    f1.add_clause(a, b, b);
    f1.add_clause(!b);

    REQUIRE( to_nums( f1 ) == std::vector{ 1, 2, 2, 0, -2, 0 } );

    auto f2 = cnf_formula{};

    auto c = literal{ store.make() };

    f2.add_clause(a);
    f2.add_clause(b, !c);

    REQUIRE( to_nums( f2 ) == std::vector{ 1, 0, 2, -3, 0 } );

    f1.add_cnf( f2 );

    REQUIRE( to_nums( f1 ) == std::vector{ 1, 2, 2, 0, -2, 0, 1, 0, 2, -3, 0 } );
}