#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <cmath>

namespace geyser
{

class literal
{
    friend class var_store;

    int _value;

    explicit literal( int value ) : _value{ value } {}

public:
    static literal separator;

    friend literal operator!( literal lit )
    {
        return literal{ -lit._value };
    }

    [[nodiscard]] int value() const { return _value; }
    [[nodiscard]] int variable() const { return std::abs( _value ); }
    [[nodiscard]] bool sign() const { return _value >= 0; }
};

// TODO: Do we need to store string->var_id mapping here? For priming, maybe?
class var_store
{
    // Maps a variable identifier (a positive integer) to its name.
    std::vector< std::string > _names;

public:
    // A dummy value for 0
    var_store() : _names{ "" } {}

    [[nodiscard]]
    literal make( std::string name = "" )
    {
        _names.emplace_back( std::move( name ) );
        return literal{ static_cast< int >( _names.size() ) };
    }

    [[nodiscard]]
    const std::string& get_name( literal lit )
    {
        assert( 0 < lit.variable() && lit.variable() < _names.size() );

        return _names[ lit.variable() ];
    }
};

class cnf_formula
{
    // Literals are stored in DIMACS format, clauses are terminated by zeroes.
    std::vector< literal > _literals;

public:
    void add_clause( const std::vector< literal >& clause )
    {
        _literals.reserve( _literals.size() + clause.size() + 1 );
        _literals.insert( _literals.end(), clause.cbegin(), clause.cend() );
        _literals.push_back( literal::separator );
    }
};

} // namespace geyser