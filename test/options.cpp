#include "options.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace geyser;

TEST_CASE( "Verbosity levels are correctly ordered" )
{
    REQUIRE( verbosity_level::silent < verbosity_level::loud );
    REQUIRE( verbosity_level::loud < verbosity_level::debug );
    REQUIRE( verbosity_level::loud <= verbosity_level::debug );
    REQUIRE( verbosity_level::debug <= verbosity_level::debug );
}

TEST_CASE( "Help requested" )
{
    SECTION( "Only -h" )
    {
        auto cli = std::vector{ "", "-h" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->help_requested() );
    }

    SECTION( "Only --help" )
    {
        auto cli = std::vector{ "", "--help" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->help_requested() );
    }

    SECTION( "Input and -h" )
    {
        auto cli = std::vector{ "", "-h", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->help_requested() );
    }

    SECTION( "Input, params and -h" )
    {
        auto cli = std::vector{ "", "-e=pdr", "-h", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->help_requested() );
    }

    SECTION( "Input, params and --help" )
    {
        auto cli = std::vector{ "", "--help", "-e=pdr", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( opts->help_requested() );
    }
}

TEST_CASE( "No engine" )
{
    auto cli = std::vector{ "", "-k=10", "input.aig" };
    auto opts = parse_cli( int ( cli.size() ), cli.data() );

    REQUIRE( !opts.has_value() );
    REQUIRE( opts.error().contains( "engine" ) );
}

TEST_CASE( "Engine and input given" )
{
    SECTION( "Engine name pdr" )
    {
        auto cli = std::vector{ "", "-e=pdr", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::silent );
    }

    SECTION( "Engine name car" )
    {
        auto cli = std::vector{ "", "-e=car", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "car" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::silent );
    }
}

TEST_CASE( "Verbosity set" )
{
    SECTION( "Verbose output, verbosity first, -v" )
    {
        auto cli = std::vector{ "", "-v", "-e=pdr", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::loud );
    }

    SECTION( "Verbose output, verbosity second, --verbose" )
    {
        auto cli = std::vector{ "", "-e=pdr", "--verbose", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::loud );
    }

    SECTION( "Debug output, verbosity first" )
    {
        auto cli = std::vector{ "", "--debug", "-e=pdr", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::debug );
    }
}

TEST_CASE( "Valid parameters given" )
{
    SECTION( "A single switch" )
    {
        auto cli = std::vector{ "", "-e=pdr", "-foo", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::silent );
        REQUIRE( opts->has( "-foo" ) );
    }

    SECTION( "Two switches" )
    {
        auto cli = std::vector{ "", "--bar", "-e=pdr", "-f", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::silent );
        REQUIRE( opts->has( "-f" ) );
        REQUIRE( opts->has( "--bar" ) );
    }

    SECTION( "Switch and an integer parameter" )
    {
        auto cli = std::vector{ "", "--bar", "-e=pdr", "-f=5", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::silent );
        REQUIRE( opts->has( "-f" ) );
        REQUIRE( opts->value_or( "-f", 0 ) == 5 );
        REQUIRE( opts->has( "--bar" ) );
    }

    SECTION( "Two integer parameters" )
    {
        auto cli = std::vector{ "", "--bar=-7", "-e=pdr", "-f=5", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::silent );
        REQUIRE( opts->has( "-f" ) );
        REQUIRE( opts->value_or( "-f", 0 ) == 5 );
        REQUIRE( opts->has( "--bar" ) );
        REQUIRE( opts->value_or( "--bar", 0 ) == -7 );
    }

    SECTION( "Two integer parameters, but no engine" )
    {
        auto cli = std::vector{ "", "--bar=-7", "-f=5", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( !opts.has_value() );
        REQUIRE( opts.error().contains( "engine" ) );
    }
}

TEST_CASE( "Invalid parameters given" )
{
    SECTION( "No switch" )
    {
        auto cli = std::vector{ "", "bar=-7", "-e=pdr", "-f=5", "input.aig" };
        auto opts = parse_cli( int( cli.size()), cli.data());

        REQUIRE( !opts.has_value() );
        REQUIRE( opts.error().contains( "input" ) );
    }

    SECTION( "Non-int argument" )
    {
        auto cli = std::vector{ "", "-e=pdr", "-f=hello", "input.aig" };
        auto opts = parse_cli( int( cli.size()), cli.data());

        REQUIRE( !opts.has_value() );
        REQUIRE( opts.error().contains( "integer" ) );
    }
}

TEST_CASE( "Typical CLI input" )
{
    SECTION( "BMC" )
    {
        auto cli = std::vector{ "", "-e=bmc", "-v", "-k=10", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "bmc" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::loud );
        REQUIRE( opts->has( "-k" ) );
        REQUIRE( opts->value_or( "-k", 0 ) == 10 );
    }

    SECTION( "PDR" )
    {
        auto cli = std::vector{ "", "--debug", "-e=pdr", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "pdr" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::debug );
    }

    SECTION( "CAR" )
    {
        auto cli = std::vector{ "", "--verbose", "-e=car", "--repush", "--no-predecessor-muc", "input.aig" };
        auto opts = parse_cli( int ( cli.size() ), cli.data() );

        REQUIRE( opts.has_value() );
        REQUIRE( !opts->help_requested() );
        REQUIRE( opts->engine_name() == "car" );
        REQUIRE( opts->input_file() == "input.aig" );
        REQUIRE( opts->verbosity() == verbosity_level::loud );
        REQUIRE( opts->has( "--repush" ) );
        REQUIRE( opts->has( "--no-predecessor-muc" ) );
    }
}