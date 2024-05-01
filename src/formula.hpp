#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <cmath>

namespace geyser
{

class variable
{
    friend class variable_store;
    friend class literal;

    int _id;

    explicit variable( int id ) : _id{ id } // NOLINT
    {
        assert( id > 0 );
    }

public:
    [[nodiscard]] int id() const { return _id; }

    friend auto operator<=>( variable, variable ) = default;
};

using valuation = std::unordered_map< variable, bool >;

class literal
{
    int _value;

    explicit literal( int value ) : _value{ value } {}

public:
    explicit literal( variable var ) : _value{ var.id() } {}

    static literal separator;

    friend literal operator!( literal lit )
    {
        return literal{ -lit._value };
    }

    [[nodiscard]] int value() const { return _value; }
    [[nodiscard]] variable var() const { return variable{ std::abs( _value ) }; }
    [[nodiscard]] bool sign() const { return _value >= 0; }

    friend auto operator<=>( literal, literal ) = default;
};

class variable_store
{
    // Maps a variable identifier (a positive integer) to its name.
    std::vector< std::string > _names;

public:
    // A dummy value for 0
    variable_store() : _names{ "" } {}

    [[nodiscard]]
    variable make( std::string name = "" )
    {
        _names.emplace_back( std::move( name ) );
        return variable{ static_cast< int >( _names.size() ) };
    }

    [[nodiscard]]
    const std::string& get_name( variable var )
    {
        assert( var.id() < _names.size() );

        return _names[ var.id() ];
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

    void add_cnf( const cnf_formula& formula )
    {
        _literals.reserve( _literals.size() + formula._literals.size() );
        _literals.insert( _literals.end(), formula._literals.cbegin(), formula._literals.cend() );
    }

    [[nodiscard]] const std::vector< literal >& literals() const { return _literals; }
};

} // namespace geyser