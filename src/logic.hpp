#pragma once

#include <utility>
#include <string>
#include <vector>
#include <unordered_map>
#include <concepts>
#include <cassert>
#include <cmath>

namespace geyser
{

using var_id_range = std::pair< int, int >;

class variable
{
    int _id;

public:
    explicit variable( int id ) : _id{ id } // NOLINT
    {
        assert( id > 0 );
    }

    [[nodiscard]] int id() const { return _id; }

    friend auto operator<=>( variable, variable ) = default;
};

using valuation = std::unordered_map< variable, bool >;

class literal
{
    int _value;

    explicit literal( int value ) : _value{ value } {}

public:
    explicit literal( variable var, bool negated = false ) : _value{ var.id() }
    {
        if ( negated )
            _value *= -1;
    }

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

inline literal literal::separator{ 0 };

class variable_store
{
    // Maps a variable identifier (a positive integer) to its name.
    std::vector< std::string > _names;

    [[nodiscard]] int get_next_id() const
    {
        return static_cast< int >( _names.size() );
    }

public:
    // A dummy value for 0
    variable_store() : _names{ "" } {}

    variable make( std::string name = "" )
    {
        _names.emplace_back( std::move( name ) );
        return variable{ static_cast< int >( _names.size() - 1 ) };
    }

    // Namer is a callback that receives the current index (0 based, not id) of
    // the variable and returns its name. Returns a left-inclusive, right-exclusive
    // pair of delimiting IDs.
    [[nodiscard]]
    std::pair< int, int > make_range( int n, const std::regular_invocable< int > auto& namer )
    {
        const auto fst = get_next_id();

        for ( auto i = 0; i < n; ++i )
            make( namer( i ) );

        const auto snd = get_next_id();

        return { fst, snd };
    }

    [[nodiscard]]
    std::pair< int, int > make_range( int n )
    {
        const auto namer = []( int )
        {
            return "";
        };

        return make_range( n, namer );
    }

    [[nodiscard]]
    const std::string& get_name( variable var ) const
    {
        assert( var.id() >= 0 );
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

    void add_clause( literal l1 )
    {
        add_clause( std::vector{ l1 } );
    }

    void add_clause( literal l1, literal l2 )
    {
        add_clause( std::vector{ l1, l2 } );
    }

    void add_clause( literal l1, literal l2, literal l3 )
    {
        add_clause( std::vector{ l1, l2, l3 } );
    }

    void add_cnf( const cnf_formula& formula )
    {
        _literals.reserve( _literals.size() + formula._literals.size() );
        _literals.insert( _literals.end(), formula._literals.cbegin(), formula._literals.cend() );
    }

    [[nodiscard]] const std::vector< literal >& literals() const { return _literals; }
};

} // namespace geyser

template<>
struct std::hash< geyser::variable >
{
    std::size_t operator()( geyser::variable var ) const noexcept
    {
        return std::hash< int >{}( var.id() );
    }
};