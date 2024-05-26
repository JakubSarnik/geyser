#pragma once

#include "logic.hpp"
#include "transition_system.hpp"
#include "options.hpp"
#include <string>
#include <vector>
#include <variant>

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
public:
    virtual ~engine() = default;

    virtual result run( const transition_system& system, const options& opts ) = 0;
};

} // namespace geyser