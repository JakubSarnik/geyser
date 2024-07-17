#pragma once

#include "base.hpp"
#include "cadical.hpp"
#include <algorithm>
#include <memory>
#include <vector>

namespace geyser::pdr
{

class sorted_cube
{
    std::vector< literal > _literals;

public:
    explicit sorted_cube( std::vector< literal > literals )
        : _literals{ std::move( literals ) }
    {
        std::ranges::sort( _literals );
    };

    // Returns the cube negated as a cnf_formula containing a single clause.
    [[nodiscard]]
    cnf_formula negate() const
    {
        auto f = cnf_formula{};
        f.add_clause( _literals );

        f.inplace_transform( []( literal lit )
        {
            return !lit;
        } );

        return f;
    }

    // Returns true if this syntactically subsumes that, which is a sufficient
    // condition that this |= that. Subsumption holds iff every literal in that
    // occurs in this (with the same polarity).
    // Example:
    //   A /\ B /\ -C subsumes any of B, B /\ -C, A /\ -C, ...
    //   A /\ B does not subsume B /\ -C
    [[nodiscard]]
    bool subsumes( const sorted_cube& that ) const
    {
        if ( that._literals.size() > this->_literals.size() )
            return false;

        return std::ranges::includes( this->_literals, that._literals );
    }
};

class pdr : public engine
{
    struct frame
    {

    };

    using engine::engine;

    std::unique_ptr< CaDiCaL::Solver > _solver;
    const transition_system* _system = nullptr;

public:
    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace geyser::pdr