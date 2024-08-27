#pragma once

#include <utility>
#include <string>
#include <vector>
#include <concepts>
#include <algorithm>
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
    // TODO: Do we even need the names? Probably not, so remove.
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
    variable_range make_range( int n, const std::regular_invocable< int > auto& namer )
    {
        const auto fst = get_next_id();

        for ( auto i = 0; i < n; ++i )
            make( namer( i ) );

        const auto snd = get_next_id();

        return { fst, snd };
    }

    [[nodiscard]]
    variable_range make_range( int n )
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
    static cnf_formula constant( bool value )
    {
        if ( value )
            return cnf_formula{};

        auto contradiction = cnf_formula{};
        contradiction.add_clause( {} );

        return contradiction;
    }

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

class ordered_cube
{
    std::vector< literal > _literals;

public:
    explicit ordered_cube( std::vector< literal > literals ) : _literals{ std::move( literals ) }
    {
        std::ranges::sort( _literals, cube_literal_lt );
    };

    friend auto operator<=>( const ordered_cube&, const ordered_cube& ) = default;

    [[nodiscard]] const std::vector< literal >& literals() const { return _literals; }

    // Returns true if this syntactically subsumes that, i.e. if literals in
    // this form a subset of literals in that. Note that c.subsumes( d ) = true
    // guarantees that d entails c.
    [[nodiscard]]
    bool subsumes( const ordered_cube& that ) const
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

    // Returns the first literal corresponding to a variable var in the cube.
    // This is typically called when there is at most a single literal for a
    // given variable.
    [[nodiscard]]
    std::optional< literal > find( variable var ) const
    {
        // We could do a binary search here, but that's more complex code than
        // necessary for the typical use case of this function, i.e. building
        // a counterexample trace.

        for ( const auto lit : _literals )
            if ( lit.var() == var )
                return literal{ var, !lit.sign() };

        return {};
    }
};

inline std::string cube_to_string( const ordered_cube& cube )
{
    auto res = std::string{};
    auto sep = "";

    for ( const auto lit : cube.literals() )
    {
        res += sep + std::to_string( lit.value() );
        sep = ", ";
    }

    return res;
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