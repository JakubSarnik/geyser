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

// TODO: Do we need to store all the states here, or only the initial state?
//       The initial is enough for witnesses according to the AIGER witness
//       spec.
class counterexample
{
    // Maps steps to inputs, i.e. valuations of input variables.
    std::vector< valuation > _inputs;
    // Maps steps to states, i.e. valuations of state variables.
    std::vector< valuation > _states;

public:
    explicit counterexample( std::vector< valuation > inputs, std::vector< valuation > states )
        : _inputs{ std::move( inputs ) }, _states{ std::move( states ) } {}

    [[nodiscard]] const std::vector< valuation >& inputs() const { return _inputs; }
    [[nodiscard]] const std::vector< valuation >& states() const { return _states; }
};

using result = std::variant< ok, unknown, counterexample >;

class engine
{
protected:
    const options* _opts;
    variable_store* _store;

    template< class... Args >
    void trace( std::format_string<Args...> fmt, Args&&... args ) const
    {
        if ( _opts->verbosity == verbosity::loud )
            std::println( fmt, std::forward< Args >( args )... );
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