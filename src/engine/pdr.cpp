#include "pdr.hpp"

namespace geyser::pdr
{

result pdr::run( const transition_system& system )
{
    _system = &system;
    const auto bound = _opts->bound.value_or( std::numeric_limits< int >::max() );

    return unknown{};
}

} // namespace geyser::pdr