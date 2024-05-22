#include <catch2/catch_test_macros.hpp>
#include <formula.hpp>
#include <vector>

using namespace geyser;

namespace
{

std::vector< int > formula_to_nums( const cnf_formula& formula )
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

TEST_CASE( "Variable store counts handed out variables correctly" )
{
    auto store = variable_store{};

    REQUIRE( store.next_id() == 1 );

    store.make();

    REQUIRE( store.next_id() == 2 );

    store.make();
    store.make();

    REQUIRE( store.next_id() == 4 );
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

TEST_CASE( "CNF formula is built correctly using add_clause" )
{
    auto store = variable_store{};
    auto formula = cnf_formula{};

    REQUIRE( formula.literals().empty() );

    auto a = literal{ store.make() };
    auto b = literal{ store.make() };

    formula.add_clause( { a, b } );

    REQUIRE( formula.literals() == std::vector{ a, b, literal::separator } );
    REQUIRE( formula_to_nums( formula ) == std::vector{ 1, 2, 0 } );

    formula.add_clause( !a );

    REQUIRE( formula_to_nums( formula ) == std::vector{ 1, 2, 0, -1, 0 } );

    auto c = literal{ store.make() };

    formula.add_clause( c, !c );

    REQUIRE( formula_to_nums( formula ) == std::vector{ 1, 2, 0, -1, 0, 3, -3, 0 } );

    formula.add_clause( {} );

    REQUIRE( formula_to_nums( formula ) == std::vector{ 1, 2, 0, -1, 0, 3, -3, 0, 0 } );
}

TEST_CASE( "CNF formula is built correctly using add_cnf" )
{
    auto store = variable_store{};

    auto f1 = cnf_formula{};

    auto a = literal{ store.make() };
    auto b = literal{ store.make() };

    f1.add_clause(a, b, b);
    f1.add_clause(!b);

    REQUIRE( formula_to_nums( f1 ) == std::vector{ 1, 2, 2, 0, -2, 0 } );

    auto f2 = cnf_formula{};

    auto c = literal{ store.make() };

    f2.add_clause(a);
    f2.add_clause(b, !c);

    REQUIRE( formula_to_nums( f2 ) == std::vector{ 1, 0, 2, -3, 0 } );

    f1.add_cnf( f2 );

    REQUIRE( formula_to_nums( f1 ) == std::vector{ 1, 2, 2, 0, -2, 0, 1, 0, 2, -3, 0 } );
}