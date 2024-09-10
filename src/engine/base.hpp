#pragma once

#include "logic.hpp"
#include "transition_system.hpp"
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
    valuation _initial_state;
    std::vector< valuation > _inputs; // Maps steps to inputs, i.e. valuations of input variables.

public:
    counterexample( valuation initial_state, std::vector< valuation > inputs )
        : _initial_state{ std::move( initial_state ) }, _inputs{ std::move( inputs ) } {}

    [[nodiscard]] const valuation& initial_state() const { return _initial_state; }
    [[nodiscard]] const std::vector< valuation >& inputs() const { return _inputs; }
};

using result = std::variant< ok, unknown, counterexample >;

class engine // NOLINT (default virtual destructor is ok)
{
public:
    virtual ~engine() = default;

    [[nodiscard]] virtual result run( const transition_system& system ) = 0;
};

} // namespace geyser