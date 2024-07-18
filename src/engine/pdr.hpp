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

    // Returns true if this syntactically subsumes that, i.e. if literals in
    // this form a subset of literals in that. Note that c.subsumes( d ) = true
    // guarantees that d entails c.
    [[nodiscard]]
    bool subsumes( const sorted_cube& that ) const
    {
        if ( this->_literals.size() > that._literals.size() )
            return false;

        return std::ranges::includes( that._literals, this->_literals );
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