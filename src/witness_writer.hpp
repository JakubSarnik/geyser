#pragma once

#include <engine/base.hpp>
#include <string>

namespace geyser
{

std::string write_aiger_witness( const result& res );

} // namespace geyser