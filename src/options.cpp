#include "options.hpp"
#include <cassert>
#include <set>
#include <format>

namespace
{

bool requests_help( int argc, char const* const* argv )
{
    for ( int i = 1; i < argc; ++i )
        if ( std::set< std::string >{ "-h", "--help" }.contains( argv[ i ] ) )
            return true;

    return false;
}

bool is_reserved_opt( const std::string& opt )
{
    const auto reserved = std::set< std::string >{
        "-h", "--help",
        "-e", "--engine",
        "-v", "--verbose", "--debug"
    };

    return reserved.contains( opt );
}

std::optional< int > try_parse( const std::string& expected_int )
{
    size_t chars;
    int val;

    try
    {
        val = std::stoi( expected_int, &chars );
    }
    catch ( ... )
    {
        return {};
    }

    if ( chars != expected_int.length() )
        return {};

    return val;
}

} // namespace <anonymous>

namespace geyser
{

std::expected< options, std::string > parse_cli( int argc, char const* const* argv )
{
    if ( requests_help( argc, argv ) )
        return options::help();

    auto input_file = std::optional< std::string >{};
    auto opts = std::map< std::string, std::optional< std::string > >{};

    for ( int i = 1; i < argc; ++i )
    {
        const auto arg = std::string{ argv[ i ] };

        if ( arg.starts_with( "-" ) )
        {
            const auto pos = arg.find( '=' );

            if ( pos == std::string::npos )
                opts.emplace( arg, std::nullopt );
            else
                opts.emplace( arg.substr( 0, pos ), arg.substr( pos + 1 ) );
        }
        else
        {
            if ( input_file.has_value() )
                return std::unexpected{ std::format( "unexpected input file {} when {} already given", arg, *input_file ) };

            input_file = arg;
        }
    }

    if ( !input_file.has_value() )
        return std::unexpected{ "expected a path to the input file" };

    if ( !opts.contains( "-e" ) && !opts.contains( "--engine" ) )
        return std::unexpected{ "no engine name given" };
    if ( opts.contains( "-e" ) && !opts.at( "-e" ).has_value() )
        return std::unexpected{ "expected an engine name after -e" };
    if ( opts.contains( "--engine" ) && !opts.at( "--engine" ).has_value() )
        return std::unexpected{ "expected an engine name after --engine" };

    const auto engine_name = opts.contains( "-e" ) ? opts.at( "-e" ) : opts.at( "--engine" );

    const auto verbosity = [ & ]
    {
        if ( opts.contains( "-v" ) || opts.contains( "--verbose" ) )
            return verbosity_level::loud;
        if ( opts.contains( "--debug" ) )
            return verbosity_level::debug;

        return verbosity_level::silent;
    }();

    auto other = std::map< std::string, std::optional< int > >{};

    for ( const auto& [ key, value ] : opts )
    {
        if ( is_reserved_opt( key ) )
            continue;

        if ( value.has_value() )
        {
            auto num = try_parse( *value );

            if ( !num.has_value() )
                return std::unexpected{ std::format( "the switch {} requires an integer parameter", key ) };

            other.emplace( key, num );
        }
        else
        {
            other.emplace( key, std::nullopt );
        }
    }

    assert( input_file.has_value() );
    assert( engine_name.has_value() );

    return options{ *input_file, *engine_name, verbosity, std::move( other ) };
}

} // namespace geyser