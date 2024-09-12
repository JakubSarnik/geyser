#pragma once

#include <string>
#include <optional>
#include <map>
#include <expected>

namespace geyser
{

enum class verbosity_level
{
    silent,
    loud,
    debug
};

class options
{
    bool _help_requested = false;

    std::string _input_file;
    std::string _engine_name;
    verbosity_level _verbosity;

    std::map< std::string, std::optional< int > > _other_opts; // Either toggles, or ints.

public:
    options( std::string input_file, std::string engine_name,
             verbosity_level verbosity, std::map< std::string, std::optional< int > > other_opts )
            : _input_file{ std::move( input_file ) },
              _engine_name{ std::move( engine_name ) }, _verbosity{ verbosity },
              _other_opts( std::move( other_opts ) ) {}

    static options help()
    {
        auto opts = options{ "", "", verbosity_level::silent, {} };
        opts._help_requested = true;

        return opts;
    }

    [[nodiscard]]
    bool help_requested() const
    {
        return _help_requested;
    }

    [[nodiscard]]
    const std::string& input_file() const
    {
        return _input_file;
    }

    [[nodiscard]]
    const std::string& engine_name() const
    {
        return _engine_name;
    }

    [[nodiscard]]
    verbosity_level verbosity() const
    {
        return _verbosity;
    }

    [[nodiscard]]
    bool has( const std::string& opt ) const
    {
        return _other_opts.contains( opt );
    }

    [[nodiscard]]
    int value_or( const std::string& opt, int def ) const
    {
        const auto it = _other_opts.find( opt );
        return ( it == _other_opts.end() || !it->second.has_value() ) ? def : it->second.value(); // NOLINT
    }
};

std::expected< options, std::string > parse_cli( int argc, char const* const* argv );

} // namespace geyser