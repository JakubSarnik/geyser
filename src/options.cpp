#include "options.hpp"

namespace
{

enum class state
{
    expecting_engine,
    expecting_bound,
    anything,
    finished
};

} // namespace <anonymous>

namespace geyser
{

std::expected< options, std::string > parse_cli( int argc, char const* const* argv )
{
    auto state = state::anything;

    auto bound = std::optional< int >{};
    auto engine_name = std::optional< std::string >{};
    auto input_file = std::optional< std::string >{};
    auto verbosity = verbosity_level::silent;

    for ( int i = 1; i < argc; ++i )
    {
        const auto arg = std::string{ argv[ i ] };

        switch ( state )
        {
            case state::expecting_engine:
            {
                engine_name = arg;
                state = state::anything;
            } break;

            case state::expecting_bound:
            {
                size_t chars;
                int val;

                try
                {
                    val = std::stoi( arg, &chars );
                }
                catch (...)
                {
                    return std::unexpected{ "the bound must be a valid positive number" };
                }

                if ( chars != arg.length() || val < 0 )
                    return std::unexpected{ "the bound must be a valid positive number" };

                bound = val;
                state = state::anything;
            } break;

            case state::anything:
            {
                if ( arg == "-v" || arg == "--verbose" )
                    verbosity = verbosity_level::loud;
                else if ( arg == "-k" || arg == "-b" || arg == "--bound" )
                    state = state::expecting_bound;
                else if ( arg == "-e" || arg == "--engine" )
                    state = state::expecting_engine;
                else
                {
                    input_file = arg;
                    state = state::finished;
                }
            } break;

            case state::finished:
                return std::unexpected{ "no arguments are expected after the input file name" };
        }
    }

    if ( state == state::expecting_engine )
        return std::unexpected{ "expected an engine name" };
    if ( state == state::expecting_bound )
        return std::unexpected{ "expected a bound value" };
    if ( !engine_name.has_value() )
        return std::unexpected{ "no engine name given" };

    auto opts = options
    {
        .input_file = input_file,
        .engine_name = *engine_name,
        .verbosity = verbosity,
        .bound = bound
    };

    return opts;
}

} // namespace geyser