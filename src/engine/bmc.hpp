#pragma once

#include "base.hpp"

namespace geyser
{

class bmc : public engine
{
public:
    result run( const transition_system& system, options opts ) override;
};

} // namespace geyser