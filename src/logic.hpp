#pragma once

#include <utility>
#include <string>
#include <vector>
#include <concepts>
#include <algorithm>
#include <ranges>
#include <span>
#include <iterator>
#include <optional>
#include <cassert>
#include <cmath>

namespace geyser
{

class variable
{
    int _id;

public:
    explicit variable( int id ) : _id{ id }
    {
        assert( id > 0 );
    }

    [[nodiscard]] int id() const { return _id; }

    friend auto operator<=>( variable, variable ) = default;
};

class variable_range
{
    int _begin;
    int _end;

public:
    // So much ceremony for such a simple thing, ugh...
    class iterator
    {
        int _i;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = int;
        using value_type        = variable;
        using pointer           = const variable*;  // or also value_type*
        using reference         = const variable&;  // or also value_type&

        iterator() : _i{ 0 } {}
        explicit iterator( int i ) : _i{ i } {}

        variable operator*() const { return variable{ _i }; }
        iterator& operator++() { ++_i; return *this; }
        iterator& operator--() { --_i; return *this; }

        iterator operator++( int )
        {
            const auto copy = *this;
            operator++();
            return copy;
        }

        iterator operator--( int )
        {
            const auto copy = *this;
            operator--();
            return copy;
        }

        friend auto operator<=>( iterator, iterator ) = default;
    };

    // Construct a range representing variables in range [begin, end).
    variable_range( int begin, int end ) : _begin{ begin }, _end{ end }
    {
        assert( begin > 0 );
        assert( begin <= end );
    }

    [[nodiscard]] int size() const { return _end - _begin; }

    [[nodiscard]] bool contains( variable var ) const
    {
        return _begin <= var.id() && var.id() < _end;
    }

    [[nodiscard]] variable nth( int n ) const
    {
        const auto var = variable{ _begin + n };
        assert( contains( var ) );
        return var;
    }

    [[nodiscard]] int offset( variable var ) const
    {
        assert( contains( var ) );
        return( var.id() - _begin );
    }

    [[nodiscard]] iterator begin() const { return iterator{ _begin }; }
    [[nodiscard]] iterator end() const { return iterator{ _end }; }
};

class literal
{
    int _value;

    explicit literal( int value ) : _value{ value } {}

public:
    // TODO: Get rid of the inverted logic here, change the second parameter
    //       to bool positive = true.
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

    [[nodiscard]] literal substitute( variable var ) const
    {
        return literal{ var, !sign() };
    }

    [[nodiscard]] int value() const { return _value; }
    [[nodiscard]] variable var() const { return variable{ std::abs( _value ) }; }
    [[nodiscard]] bool sign() const { return _value >= 0; }

    friend auto operator<=>( literal, literal ) = default;
};

inline literal literal::separator{ 0 };

using valuation = std::vector< literal >;

class variable_store
{
    int _next_id = 1;

public:
    variable_store() = default;

    variable make()
    {
        return variable{ _next_id++ };
    }

    [[nodiscard]]
    variable_range make_range( int n )
    {
        const auto fst = _next_id;

        for ( auto i = 0; i < n; ++i )
            make();

        const auto snd = _next_id;

        return { fst, snd };
    }
};

class cnf_formula
{
    // Literals are stored in DIMACS format, clauses are terminated by zeroes.
    std::vector< literal > _literals;

public:
    static cnf_formula constant( bool value )
    {
        if ( value )
            return cnf_formula{};

        auto contradiction = cnf_formula{};
        contradiction.add_clause( {} );

        return contradiction;
    }

    static cnf_formula clause( std::span< const literal > c )
    {
        auto f = cnf_formula{};
        f.add_clause( c );

        return f;
    }

    void add_clause( std::span< const literal > clause )
    {
        _literals.reserve( _literals.size() + clause.size() + 1 );
        _literals.insert( _literals.end(), clause.begin(), clause.end() );
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

    [[nodiscard]] cnf_formula map( const std::regular_invocable< literal > auto& f ) const
    {
        auto res = cnf_formula{};
        res._literals.reserve( _literals.size() );

        for ( const auto lit : _literals )
            res._literals.push_back( lit == literal::separator ? literal::separator : f( lit ) );

        return res;
    }

    void inplace_transform( const std::regular_invocable< literal > auto& f )
    {
        for ( auto& lit : _literals )
            if ( lit != literal::separator )
                lit = f( lit );
    }

    [[nodiscard]] cnf_formula activate( variable activator ) const
    {
        auto res = cnf_formula{};
        res._literals.reserve( _literals.size() );

        for ( const auto lit : _literals )
        {
            if ( lit == literal::separator )
                res._literals.push_back( !literal{ activator } );

            res._literals.push_back( lit );
        }

        return res;
    }
};

// Representation of cubes makes use of literal ordering which is more
// complicated than comparing its underlying integer value. We order literals
// lexicographically first on their absolute value and second on their sign.
// This means that, given variables with values 1, 2 and 3, the following
// vectors are ordered:
//   1, 2, 3
//   -1, 2, 3
//   1, -2, 2, 3
// while the following are not:
//   2, 1
//   -2, 1, 3
//   1, -1, 2, 3

inline bool cube_literal_lt( literal l1, literal l2 )
{
    return ( l1.var().id() < l2.var().id() ) ||
           ( l1.var().id() == l2.var().id() && !l1.sign() && l2.sign() );
}

class cube
{
    std::vector< literal > _literals;

public:
    cube() = default;

    explicit cube( std::vector< literal > literals ) : _literals{ std::move( literals ) }
    {
        std::ranges::sort( _literals, cube_literal_lt );
    };

    friend auto operator<=>( const cube&, const cube& ) = default;

    [[nodiscard]] const std::vector< literal >& literals() const { return _literals; }

    // Returns true if this syntactically subsumes that, i.e. if literals in
    // this form a subset of literals in that. Note that c.subsumes( d ) = true
    // guarantees that d entails c.
    [[nodiscard]]
    bool subsumes( const cube& that ) const
    {
        return std::ranges::includes( that._literals, _literals, cube_literal_lt );
    }

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

    [[nodiscard]]
    bool contains( literal lit ) const
    {
        return std::ranges::binary_search( _literals, lit, cube_literal_lt );
    }

    // Assuming the cube doesn't contain a pair of literals with the same
    // variable but different polarities, return the literal in which the given
    // variable appears in the cube (or nothing if the variable doesn't appear
    // at all).
    [[nodiscard]]
    std::optional< literal > find( variable var ) const
    {
        const auto lit = literal{ var };

        if ( contains( lit ) )
            return lit;
        if ( contains( !lit ) )
            return !lit;

        return {};
    }
};

inline std::string cube_to_string( const cube& c )
{
    auto res = std::string{};
    auto sep = "";

    for ( const auto lit : c.literals() )
    {
        res += sep + std::to_string( lit.value() );
        sep = ", ";
    }

    return res;
}

inline cube formula_as_cube( const cnf_formula& f )
{
    // Assert that this is indeed a cube.
    assert( std::ranges::count( f.literals(), literal::separator ) == f.literals().size() / 2 );

    auto lits = std::vector< literal >{};

    for ( const auto lit : f.literals() )
        if ( lit != literal::separator )
            lits.push_back( lit );

    return cube{ lits };
}

} // namespace geyser

template<>
struct std::hash< geyser::variable >
{
    std::size_t operator()( geyser::variable var ) const noexcept
    {
        return std::hash< int >{}( var.id() );
    }
};