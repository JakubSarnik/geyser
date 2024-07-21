#pragma once
#include "logic.hpp"
#include <vector>
#include <cmath>

inline std::vector< int > to_nums( const std::vector< geyser::literal >& literals )
{
    auto res = std::vector< int >{};

    for ( const auto lit : literals )
        res.push_back( lit.value() );

    return res;
}

inline std::vector< int > to_nums( const geyser::cnf_formula& formula )
{
    return to_nums( formula.literals() );
}

inline std::vector< geyser::literal > to_literals( const std::vector< int >& nums )
{
    auto res = std::vector< geyser::literal >{};

    for ( const auto num : nums )
    {
        if ( num == 0 )
            res.push_back( geyser::literal::separator );
        else
            res.emplace_back( geyser::variable{ std::abs( num ) }, num < 0 );
    }

    return res;
}