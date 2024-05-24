#pragma once

#include <memory>

extern "C"
{

#include "aiger.h"

}

namespace geyser
{

using aiger_ptr = std::unique_ptr< aiger, decltype( &aiger_reset ) >;

inline aiger_ptr make_aiger()
{
    return { aiger_init(), &aiger_reset };
}

} // namespace geyser