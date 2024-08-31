#include "common.hpp"
#include "logic.hpp"
#include "transition_system.hpp"

using namespace geyser;

namespace
{

transition_system make_system( int input_vars, int state_vars, int aux_vars )
{
    auto store = variable_store{};

    return transition_system( store.make_range( input_vars ), store.make_range( state_vars ),
                              store.make_range( state_vars ), store.make_range( aux_vars ),
                              cnf_formula{}, cnf_formula{}, cnf_formula{} );
}

} // namespace <anonymous>

TEST_CASE( "Variable types and positions are correctly determined" )
{
    SECTION( "Only state variables" )
    {
        const auto system = make_system( 0, 2, 0 );

        {
            const auto [ type, pos ] = system.get_var_info( system.state_vars().nth( 0 ) );

            REQUIRE( type == geyser::var_type::state );
            REQUIRE( pos == 0 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.state_vars().nth( 1 ) );

            REQUIRE( type == geyser::var_type::state );
            REQUIRE( pos == 1 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.next_state_vars().nth( 0 ) );

            REQUIRE( type == geyser::var_type::next_state );
            REQUIRE( pos == 0 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.next_state_vars().nth( 1 ) );

            REQUIRE( type == geyser::var_type::next_state );
            REQUIRE( pos == 1 );
        }
    }

    SECTION( "All types of variables" )
    {
        const auto system = make_system( 3, 2, 5 );

        {
            const auto [ type, pos ] = system.get_var_info( system.input_vars().nth( 0 ) );

            REQUIRE( type == geyser::var_type::input );
            REQUIRE( pos == 0 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.state_vars().nth( 1 ) );

            REQUIRE( type == geyser::var_type::state );
            REQUIRE( pos == 1 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.next_state_vars().nth( 0 ) );

            REQUIRE( type == geyser::var_type::next_state );
            REQUIRE( pos == 0 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.aux_vars().nth( 0 ) );

            REQUIRE( type == geyser::var_type::auxiliary );
            REQUIRE( pos == 0 );
        }

        {
            const auto [ type, pos ] = system.get_var_info( system.aux_vars().nth( 3 ) );

            REQUIRE( type == geyser::var_type::auxiliary );
            REQUIRE( pos == 3 );
        }
    }
}

TEST_CASE( "State variables are correctly primed and unprimed" )
{
    const auto system = make_system( 3, 3, 5 );

    const auto a = system.state_vars().nth( 0 );
    const auto b = system.state_vars().nth( 1 );
    const auto c = system.state_vars().nth( 2 );

    const auto ap = system.next_state_vars().nth( 0 );
    const auto bp = system.next_state_vars().nth( 1 );
    const auto cp = system.next_state_vars().nth( 2 );

    REQUIRE( system.prime( literal{ a } ) == literal{ ap } );
    REQUIRE( system.prime( literal{ b } ) == literal{ bp } );
    REQUIRE( system.prime( literal{ c } ) == literal{ cp } );

    REQUIRE( system.unprime( literal{ ap } ) == literal{ a } );
    REQUIRE( system.unprime( literal{ bp } ) == literal{ b } );
    REQUIRE( system.unprime( literal{ cp } ) == literal{ c } );
}