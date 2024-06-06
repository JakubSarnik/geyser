#pragma once

#include "logic.hpp"
#include "transition_system.hpp"
#include "options.hpp"
#include <string>
#include <vector>
#include <variant>
#include <print>

namespace geyser
{

struct ok {};

struct unknown
{
    std::string reason;
};

class counterexample
{
    // Maps steps to states, i.e. valuations of state variables.
    std::vector< valuation > _states;

public:
    explicit counterexample( std::vector< valuation > states ) : _states{ std::move( states ) } {}

    [[nodiscard]] const std::vector< valuation >& states() const { return _states; }
};

using result = std::variant< ok, unknown, counterexample >;

class engine
{
    virtual result do_run() = 0;

protected:
    const options* _opts = nullptr;
    variable_store* _store = nullptr;
    const transition_system* _system = nullptr;

    template< class... Args >
    void trace( std::format_string<Args...> fmt, Args&&... args )
    {
        if ( _opts->verbosity == verbosity::loud )
            std::println( fmt, std::forward< Args... >( args... ) );
    }

public:
    explicit engine( const options& opts ) : _opts{ &opts } {}

    engine( const engine& ) = delete;
    engine( engine&& ) = delete;

    engine& operator=( const engine& ) = delete;
    engine& operator=( engine&& ) = delete;

    virtual ~engine() = default;

    [[nodiscard]] result run( variable_store& store, const transition_system& system )
    {
        _store = &store;
        _system = &system;

        return do_run();
    }
};

} // namespace geyser