#pragma once

#include "cadical.hpp"
#include "logic.hpp"
#include <concepts>
#include <memory>
#include <vector>
#include <span>

// This file includes a lightweight solver API abstracting over low-level
// details of CaDiCaL.

namespace geyser
{

class solver
{
    std::unique_ptr< CaDiCaL::Solver > _solver;
public:
    class query_builder
    {
        solver* _s;

    public:
        explicit query_builder( solver& s ) : _s{ &s } {}

        query_builder( const query_builder& ) = delete;
        query_builder( query_builder&& ) = delete;

        query_builder& operator=( const query_builder& ) = delete;
        query_builder& operator=( query_builder&& ) = delete;

        ~query_builder() = default;

        query_builder& assume( literal l )
        {
            assert( _s );
            assert( _s->_solver );

            _s->_solver->assume( l.value() );
            return *this;
        }

        query_builder& assume( std::span< const literal > literals )
        {
            for ( const auto l : literals )
                assume( l );

            return *this;
        }

        query_builder& assume_mapped( std::span< const literal > literals,
                                      const std::regular_invocable< literal > auto& f )
        {
            for ( const auto l : literals )
                assume( f( l ) );

            return *this;
        }

        query_builder& constrain_not( std::span< const literal > cube )
        {
            assert( _s );
            assert( _s->_solver );

            for ( const auto l : cube )
                _s->_solver->constrain( ( !l ).value() );
            _s->_solver->constrain( 0 );

            return *this;
        }

        query_builder& constrain_not_mapped( std::span< const literal > cube,
                                             const std::regular_invocable< literal > auto& f )
        {
            assert( _s );
            assert( _s->_solver );

            for ( const auto l : cube )
                _s->_solver->constrain( f( !l ).value() );
            _s->_solver->constrain( 0 );

            return *this;
        }

        query_builder& constrain_clause( std::span< const literal > clause )
        {
            assert( _s );
            assert( _s->_solver );

            for ( const auto l : clause )
                _s->_solver->constrain( l.value() );
            _s->_solver->constrain( 0 );

            return *this;
        }

        [[nodiscard]]
        bool is_sat()
        {
            assert( _s );
            assert( _s->_solver );

            const auto res = _s->_solver->solve();
            assert( res != CaDiCaL::UNKNOWN );

            return res == CaDiCaL::SATISFIABLE;
        }
    };

    solver() : _solver{ std::make_unique< CaDiCaL::Solver >() } {};

    void reset()
    {
        _solver = std::make_unique< CaDiCaL::Solver >();
    }

    void assert_formula( const cnf_formula& formula )
    {
        assert( _solver );

        for ( const auto lit : formula.literals() )
            _solver->add( lit.value() );
    }

    [[nodiscard]]
    bool is_true_in_model( variable var )
    {
        assert( _solver );
        assert( ( _solver->state() & CaDiCaL::SATISFIED ) != 0 );

        return _solver->val( var.id() ) > 0;
    }

    [[nodiscard]]
    std::vector< literal > get_model( variable_range range )
    {
        auto val = std::vector< literal >{};
        val.reserve( range.size() );

        for ( const auto var : range )
            val.emplace_back( var, !is_true_in_model( var ) );

        return val;
    }

    [[nodiscard]]
    bool is_in_core( literal lit )
    {
        assert( _solver );
        assert( ( _solver->state() & CaDiCaL::UNSATISFIED ) != 0 );

        return _solver->failed( lit.value() );
    }

    [[nodiscard]]
    std::vector< literal > get_core( std::span< const literal > literals )
    {
        auto core = std::vector< literal >{};

        for ( const auto lit : literals )
            if ( is_in_core( lit ) )
                core.emplace_back( lit );

        return core;
    }

    [[nodiscard]]
    std::vector< literal > get_core( variable_range variables )
    {
        auto core = std::vector< literal >{};

        for ( const auto var : variables )
        {
            const auto lit = literal{ var };

            if ( is_in_core( lit ) )
                core.emplace_back( lit );
            else if ( is_in_core( !lit ) )
                core.emplace_back( !lit );
        }

        return core;
    }

    [[nodiscard]]
    query_builder query()
    {
        return query_builder{ *this };
    }
};

} // namespace geyser