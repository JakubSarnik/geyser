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

std::expected< options, std::string > parse_cli( int argc, char** argv )
{
    auto opts = options{};
    auto state = state::anything;

    for ( int i = 1; i < argc; ++i )
    {
        const auto arg = std::string{ argv[ i ] };

        switch ( state )
        {
            case state::expecting_engine:
            {
                opts.engine_name = arg;
                state = state::anything;
            } break;

            case state::expecting_bound:
            {
                size_t chars;
                int bound = std::stoi( arg, &chars );

                if ( chars != arg.length() || bound < 0 )
                    return std::unexpected{ "the bound must be a valid positive number" };

                opts.bound = bound;
                state = state::anything;
            } break;

            case state::anything:
            {
                if ( arg == "-v" || arg == "--verbose" )
                    opts.verbosity = verbosity::loud;
                else if ( arg == "-k" || arg == "-b" || arg == "--bound" )
                    state = state::expecting_bound;
                else if ( arg == "-e" || arg == "--engine" )
                    state = state::expecting_engine;
                else
                {
                    opts.input_file = arg;
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

    return opts;
}

} // namespace geyser