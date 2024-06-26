#pragma once

#include <string>
#include <optional>
#include <expected>

namespace geyser
{

enum class verbosity
{
    silent,
    loud
};

struct options
{
    std::optional< std::string > input_file; // If missing, load from stdin.
    std::string engine_name; // Must be present, otherwise yield error.

    verbosity verbosity = verbosity::silent;
    std::optional< int > bound; // In BMC, the upper bound on the length of a potential counterexample.
};

std::expected< options, std::string > parse_cli( int argc, char const* const* argv );

} // namespace geyser