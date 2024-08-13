#include "options.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace geyser;

TEST_CASE( "No engine" )
{
    auto cli = std::vector{ "", "-k", "10", "input.aig" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "engine" ) );
}

TEST_CASE( "Awaiting engine" )
{
    auto cli = std::vector{ "", "-k", "10", "-e" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "engine" ) );
}

TEST_CASE( "Awaiting bound" )
{
    auto cli = std::vector{ "", "-e", "bmc", "-k" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "bound" ) );
}

TEST_CASE( "Negative bound" )
{
    auto cli = std::vector{ "", "-e", "bmc", "-k", "-2" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "positive number" ) );
}

TEST_CASE( "Non-integer bound 1" )
{
    auto cli = std::vector{ "", "-e", "bmc", "-k", "hello" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "positive number" ) );
}

TEST_CASE( "Non-integer bound 2" )
{
    auto cli = std::vector{ "", "-e", "bmc", "-k", "2.0" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "positive number" ) );
}

TEST_CASE( "Non-integer bound 3" )
{
    auto cli = std::vector{ "", "-e", "bmc", "-k", "2x" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "positive number" ) );
}

TEST_CASE( "Valid, non-verbose, bounded file input" )
{
    SECTION( "Engine before bound" )
    {
        auto cli = std::vector{ "", "-e", "bmc", "-k", "2", "input.aig" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( opts->input_file == "input.aig" );
        REQUIRE( opts->verbosity == verbosity_level::silent );
    }

    SECTION( "Bound before engine" )
    {
        auto cli = std::vector{ "", "-k", "2", "-e", "bmc", "input.aig" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( opts->input_file == "input.aig" );
        REQUIRE( opts->verbosity == verbosity_level::silent );
    }
}

TEST_CASE( "Valid, verbose, bounded file input" )
{
    SECTION( "Engine, verbosity, bound" )
    {
        auto cli = std::vector{ "", "-e", "bmc", "-v", "-k", "2", "input.aig" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( opts->input_file == "input.aig" );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }

    SECTION( "Bound, engine, verbosity" )
    {
        auto cli = std::vector{ "", "-k", "2", "-e", "bmc", "-v", "input.aig" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( opts->input_file == "input.aig" );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }

    SECTION( "Verbosity, bound, engine" )
    {
        auto cli = std::vector{ "", "-v", "-k", "2", "-e", "bmc", "input.aig" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( opts->input_file == "input.aig" );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }
}

TEST_CASE( "Valid, verbose, bounded stdin input" )
{
    SECTION( "Engine, verbosity, bound" )
    {
        auto cli = std::vector{ "", "-e", "bmc", "-v", "-k", "2" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( !opts->input_file.has_value() );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }

    SECTION( "Bound, engine, verbosity" )
    {
        auto cli = std::vector{ "", "-k", "2", "-e", "bmc", "-v" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( !opts->input_file.has_value() );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }

    SECTION( "Verbosity, bound, engine" )
    {
        auto cli = std::vector{ "", "-v", "-k", "2", "-e", "bmc" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( opts->bound == 2 );
        REQUIRE( !opts->input_file.has_value() );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }
}

TEST_CASE( "Valid, verbose, unbounded stdin input" )
{
    SECTION( "Engine, verbosity" )
    {
        auto cli = std::vector{ "", "-e", "bmc", "-v" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( !opts->bound.has_value() );
        REQUIRE( !opts->input_file.has_value() );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }

    SECTION( "Verbosity, engine" )
    {
        auto cli = std::vector{ "", "-v", "-e", "bmc" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->engine_name == "bmc" );
        REQUIRE( !opts->bound.has_value() );
        REQUIRE( !opts->input_file.has_value() );
        REQUIRE( opts->verbosity == verbosity_level::loud );
    }
}

TEST_CASE( "Rejected arguments after input file" )
{
    SECTION( "Verbosity" )
    {
        auto cli = std::vector{ "", "-e", "bmc", "input.txt", "-v" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( !opts.has_value() );
        REQUIRE( opts.error().contains( "after the input" ) );
    }

    SECTION( "Engine" )
    {
        auto cli = std::vector{ "", "-v", "input.txt", "-e", "-bmc" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( !opts.has_value() );
        REQUIRE( opts.error().contains( "after the input" ) );
    }

    SECTION( "Bound" )
    {
        auto cli = std::vector{ "", "-e", "bmc", "input.txt", "-k", "2" };
        auto opts = parse_cli( int( cli.size() ), cli.data() );

        REQUIRE( !opts.has_value() );
        REQUIRE( opts.error().contains( "after the input" ) );
    }

}