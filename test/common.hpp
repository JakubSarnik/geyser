#pragma once
#include "logic.hpp"
#include <vector>

inline std::vector< int > to_nums( const geyser::cnf_formula& formula )
{
    auto res = std::vector< int >{};

    for ( const auto lit : formula.literals() )
        res.push_back( lit.value() );

    return res;
}