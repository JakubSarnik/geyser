#pragma once

#include "base.hpp"
#include "cadical.hpp"
#include <algorithm>
#include <memory>
#include <vector>
#include <span>

namespace geyser::pdr
{

class ordered_cube
{
    std::vector< literal > _literals;

public:
    explicit ordered_cube( std::vector< literal > literals )
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
    bool subsumes( const ordered_cube& that ) const
    {
        if ( this->_literals.size() > that._literals.size() )
            return false;

        return std::ranges::includes( that._literals, this->_literals );
    }
};

class pdr : public engine
{
    class query_builder
    {
        CaDiCaL::Solver* _solver;

    public:
        explicit query_builder( CaDiCaL::Solver& solver ) : _solver{ &solver } {}

        query_builder( const query_builder& ) = delete;
        query_builder( query_builder&& ) = delete;

        query_builder& operator=( const query_builder& ) = delete;
        query_builder& operator=( query_builder&& ) = delete;

        ~query_builder() = default;

        void assume( literal l )
        {
            _solver->assume( l.value() );
        }

        void assume( std::span< literal > literals )
        {
            for ( const auto l : literals )
                assume( l );
        }

        void constrain( const cnf_formula& clause )
        {
            assert( std::ranges::count( clause.literals(), literal::separator ) == 1 );

            for ( const auto l : clause.literals() )
                _solver->constrain( l.value() );
        }

        bool is_sat()
        {
            const auto res = _solver->solve();
            assert( res != CaDiCaL::UNKNOWN );

            return res == CaDiCaL::SATISFIABLE;
        }
    };

    struct frame
    {
        std::vector< ordered_cube > blocked_cubes;
        literal activator;

        explicit frame( literal activator ) : activator{ activator } {}
    };

    using engine::engine;

    std::unique_ptr< CaDiCaL::Solver > _solver;
    const transition_system* _system = nullptr;

    literal _transition_activator;
    literal _error_activator;

    cnf_formula _activated_init; // This is activated by _trace[ 0 ].activator
    cnf_formula _activated_trans;
    cnf_formula _activated_error;

    std::vector< frame > _trace;

    // How many solver queries to make before refreshing the solver to remove
    // all the accumulated subsumed clauses.
    constexpr static int solver_refresh_rate = 5000;
    int _queries = 0;

    void refresh_solver();

    [[nodiscard]] query_builder with_solver()
    {
        if ( _queries++ % solver_refresh_rate == 0 )
            refresh_solver();

        assert( _solver );
        return query_builder{ *_solver };
    }

    void assert_formula( const cnf_formula& formula )
    {
        assert( _solver );

        for ( const auto lit : formula.literals() )
            _solver->add( lit.value() );
    }

    [[nodiscard]] int k() const
    {
        return (int) _trace.size() - 1;
    }

    void push_frame()
    {
        _trace.emplace_back( literal{ _store->make( std::format( "Act[{}]", k() + 1 ) ) } );
    }

    void initialize();

public:
    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace geyser::pdr