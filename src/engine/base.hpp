#pragma once

#include "logic.hpp"
#include "transition_system.hpp"
#include "options.hpp"
#include <string>
#include <vector>
#include <variant>
#include <format>
#include <iostream>

namespace geyser
{

struct ok {};

struct unknown
{
    std::string reason;
};

class counterexample
{
    valuation _initial_state;
    std::vector< valuation > _inputs; // Maps steps to inputs, i.e. valuations of input variables.

public:
    counterexample( valuation initial_state, std::vector< valuation > inputs )
        : _initial_state{ std::move( initial_state ) }, _inputs{ std::move( inputs ) } {}

    [[nodiscard]] const valuation& initial_state() const { return _initial_state; }
    [[nodiscard]] const std::vector< valuation >& inputs() const { return _inputs; }
};

using result = std::variant< ok, unknown, counterexample >;

class engine
{
protected:
    const options* _opts;
    variable_store* _store;

    // TODO: Move verbose output out of this into a separate printer, make this
    //       class purely abstract.
    template< class... Args >
    void log_line( verbosity_level min, std::format_string<Args...> fmt, Args&&... args ) const
    {
        if ( _opts->verbosity >= min )
            std::cout << std::format( fmt, std::forward< Args >( args )... ) << "\n";
    }

    template< class... Args >
    void log_line_loud( std::format_string<Args...> fmt, Args&&... args ) const
    {
        log_line( verbosity_level::loud, fmt, std::forward< Args >( args )... );
    }

    template< class... Args >
    void log_line_debug( std::format_string<Args...> fmt, Args&&... args ) const
    {
        log_line( verbosity_level::debug, fmt, std::forward< Args >( args )... );
    }

public:
    engine( const options& opts, variable_store& store ) : _opts{ &opts }, _store{ &store } {}

    engine( const engine& ) = delete;
    engine( engine&& ) = delete;

    engine& operator=( const engine& ) = delete;
    engine& operator=( engine&& ) = delete;

    virtual ~engine() = default;

    [[nodiscard]] virtual result run( const transition_system& system ) = 0;
};

} // namespace geyser