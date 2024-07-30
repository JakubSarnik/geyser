#pragma once

#include "logic.hpp"
#include "cadical.hpp"
#include <algorithm>
#include <vector>
#include <memory>
#include <optional>
#include <queue>
#include <string>

namespace geyser::alt_pdr
{

class cube
{
    std::vector< literal > _literals;

public:
    explicit cube( std::vector< literal > literals ) : _literals{ std::move( literals ) } {};

    friend auto operator<=>( const cube&, const cube& ) = default;

    [[nodiscard]] const std::vector< literal >& literals() const { return _literals; }

    // Returns true if this syntactically subsumes that, i.e. if literals in
    // this form a subset of literals in that. Note that c.subsumes( d ) = true
    // guarantees that d entails c.
    [[nodiscard]]
    bool subsumes( const cube& that ) const
    {
        if ( this->_literals.size() > that._literals.size() )
            return false;

        for ( const auto lit : _literals )
            if ( std::find( that._literals.begin(), that._literals.end(), lit ) == that._literals.end() )
                return false;

        return true;
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
    std::string format() const
    {
        auto res = std::string{};
        auto sep = "";

        for ( const auto lit : _literals )
        {
            res += sep + std::to_string( lit.value() );
            sep = ", ";
        }

        return res;
    }
};

// We don't remember successors here, so no counterexample reconstruction!
class proof_obligation
{
    std::size_t _level;
    cube _state_vars_cube;

public:
    proof_obligation( std::size_t level, cube state_vars_cube )
        : _level{ level }, _state_vars_cube{ std::move( state_vars_cube ) } {}

    friend auto operator<=>( const proof_obligation&, const proof_obligation& ) = default;

    [[nodiscard]] std::size_t level() const { return _level; }
    [[nodiscard]] const cube& state_vars_cube() const { return _state_vars_cube; }
    [[nodiscard]] cube& state_vars_cube() { return _state_vars_cube; }
};

class pdr : public engine
{
    std::unique_ptr< CaDiCaL::Solver > _solver;
    const transition_system* _system = nullptr;

    literal _transition_activator;
    literal _error_activator;

    cnf_formula _activated_init; // This is activated by _trace[ 0 ].activator
    cnf_formula _activated_trans;
    cnf_formula _activated_error;

    std::vector< std::vector< cube > > _trace_blocked_cubes;
    std::vector< literal > _trace_activators;

    std::size_t depth()
    {
        return _trace_blocked_cubes.size() - 1;
    }

    void assert_formula( const cnf_formula& formula )
    {
        assert( _solver );

        for ( const auto lit : formula.literals() )
            _solver->add( lit.value() );
    }

    cube get_model( variable_range range )
    {
        auto lits = std::vector< literal >{};
        lits.reserve( range.size() );

        for ( const auto var : range )
        {
            if ( _solver->val( var.id() ) > 0 )
                lits.push_back( literal{ var } ); // NOLINT
            else
                lits.push_back( !literal{ var } );
        }

        return cube{ std::move( lits ) };
    }

    void push_frame()
    {
        _trace_blocked_cubes.emplace_back();

        while ( _trace_activators.size() <= depth() )
            _trace_activators.emplace_back( _store->make() );

        assert( depth() < _trace_activators.size() );
    }

    void assume_trace_from( std::size_t start )
    {
        for ( std::size_t i = start; i <= depth(); ++i )
            _solver->assume( _trace_activators[ i ].value() );
    }

    literal prime_literal( literal lit ) const // NOLINT
    {
        const auto [ type, pos ] = _system->get_var_info( lit.var() );
        assert( type == var_type::state );

        return lit.substitute( _system->next_state_vars().nth( pos ) );
    }

    std::optional< cube > get_bad_state()
    {
        assume_trace_from( depth() );
        _solver->assume( _error_activator.value() );

        if ( _solver->solve() == CaDiCaL::SATISFIABLE )
            return get_model( _system->state_vars() );

        return {};
    }

    bool is_already_blocked( const proof_obligation& po )
    {
        for ( std::size_t i = po.level(); i <= depth(); ++i )
            for ( const auto& c : _trace_blocked_cubes[ i ] )
                if ( c.subsumes( po.state_vars_cube() ) )
                    return true;

        assume_trace_from( po.level() );

        for ( const auto lit : po.state_vars_cube().literals() )
            _solver->assume( lit.value() );

        return _solver->solve() == CaDiCaL::UNSATISFIABLE;
    }

    bool intersects_initial_states( const cube& c ) // NOLINT
    {
        // Note that this would not work if we had initial formula more
        // complex than a cube (i.e. with invariance constraints)!

        const auto& init_lits = _system->init().literals();

        for ( const auto lit : c.literals() )
            if ( std::find( init_lits.begin(), init_lits.end(), !lit ) != init_lits.end() )
                return false;

        return true;

//        _solver->assume( _trace_activators[ 0 ].value() );
//        for ( const auto lit : c.literals() )
//            _solver->assume( lit.value() );
//
//        return _solver->solve() == CaDiCaL::SATISFIABLE;
    }

    // Check whether, given po = < s, i >, s is inductive relative to
    // R_{i - 1}, i.e. whether the formula R_{i - 1} /\ -s /\ T /\ s' is
    // unsatisfiable.
    //
    // If it is unsatisfiable, the function returns true and a new proof
    // obligation < t, j >, where t contains those literals x of s where x'
    // is in the core (plus those which are necessary to make the cube disjoint
    // with the initial states) and j is the lowest number such that Act[ j ]
    // is in the core (if no Act[ j ] is in the core, returns depth()).
    //
    // If it is satisfiable, the function returns false and a new proof
    // obligation < t, i - 1 >, where t is a model of state variables shortened
    // by an "equivalent" of ternary simulation (i.e. a generalized predecessor
    // of s).
    std::pair< bool, proof_obligation > is_relative_inductive( const proof_obligation& po )
    {
        assert( po.level() >= 1 );

        // -s
        for ( const auto lit : po.state_vars_cube().literals() )
            _solver->constrain( ( !lit ).value() );
        _solver->constrain( 0 );

        // R[ i - 1 ]
        assume_trace_from( po.level() - 1 );

        // T
        _solver->assume( _transition_activator.value() );

        // s'
        for ( const auto lit : po.state_vars_cube().literals() )
            _solver->assume( prime_literal( lit ).value() );

        if ( _solver->solve() == CaDiCaL::SATISFIABLE )
        {
            const auto ins = get_model( _system->input_vars() );
            auto p = get_model( _system->state_vars() );

            // -s'
            for ( const auto lit : po.state_vars_cube().literals() )
                _solver->constrain( prime_literal( !lit ).value() );
            _solver->constrain( 0 );

            // T
            _solver->assume( _transition_activator.value() );

            // ins
            for ( const auto lit : ins.literals() )
                _solver->assume( lit.value() );

            // p
            for ( const auto lit : p.literals() )
                _solver->assume( lit.value() );

            const auto res = _solver->solve();

            assert( res == CaDiCaL::UNSATISFIABLE );

            auto res_lits = std::vector< literal >{};

            for ( const auto lit : p.literals() )
                if ( _solver->failed( lit.value() ) )
                    res_lits.emplace_back( lit );

            return { false, proof_obligation{ po.level() - 1, cube{ std::move( res_lits ) } } };
        }
        else
        {
            std::size_t j = depth();

            for ( std::size_t i = po.level() - 1; i <= depth(); ++i )
            {
                if ( _solver->failed( _trace_activators[ i ].value() ) )
                {
                    j = i;
                    break;
                }
            }

            const auto all_lits = po.state_vars_cube().literals();
            auto res_lits = all_lits;

            for ( const auto lit : all_lits )
            {
                if ( !_solver->failed( lit.value() ) )
                    continue;

                auto shorter = res_lits;
                shorter.erase( std::remove( shorter.begin(), shorter.end(), lit ), shorter.end() );

                if ( !intersects_initial_states( cube{ shorter } ) )
                    res_lits = shorter;
            }

            //return { true, proof_obligation{ j + 1, cube{ res_lits } } };
            return { true, proof_obligation{ std::min( j + 1, depth() ), cube{ res_lits } } };
        }
    }

    proof_obligation generalize( const proof_obligation& po )
    {
        const auto all_lits = po.state_vars_cube().literals();
        auto res_lits = all_lits;

        for ( const auto lit : all_lits )
        {
            res_lits.erase( std::remove( res_lits.begin(), res_lits.end(), lit ), res_lits.end() );
            const auto shorter_po = proof_obligation{ po.level(), cube{ res_lits } };

            const auto [ rel_ind, _ ] = is_relative_inductive( shorter_po );

            if ( intersects_initial_states( shorter_po.state_vars_cube() ) || !rel_ind )
                res_lits.emplace_back( lit );
        }

        return proof_obligation{ po.level(), cube{ res_lits } };
    }

    void add_blocked_cube( const proof_obligation& po )
    {
        const auto k = std::min( po.level(), depth() );

        for ( std::size_t d = 1; d <= k; ++d )
        {
            for ( std::size_t i = 0; i < _trace_blocked_cubes[ d ].size(); )
            {
                if ( po.state_vars_cube().subsumes( _trace_blocked_cubes[ d ][ i ] ) )
                {
                    _trace_blocked_cubes[ d ][ i ] = _trace_blocked_cubes[ d ].back();
                    _trace_blocked_cubes[ d ].pop_back();
                }
                else
                    ++i;
            }
        }

        _trace_blocked_cubes[ k ].emplace_back( po.state_vars_cube() );
        assert_formula( po.state_vars_cube().negate().activate( _trace_activators[ k ].var() ) );
    }

    // True if unsafe
    bool solve_obligation( const proof_obligation& starting_po )
    {
        assert( starting_po.level() <= depth() );

        auto min_queue = std::priority_queue< proof_obligation,
                std::vector< proof_obligation >, std::greater<> >{};

        min_queue.push( starting_po );

        while ( !min_queue.empty() )
        {
            auto po = min_queue.top();
            min_queue.pop();

            if ( po.level() == 0 )
                return true;

            if ( is_already_blocked( po ) )
                continue;

            assert( !intersects_initial_states( po.state_vars_cube() ) );

            const auto [ rel_ind, next_po ] = is_relative_inductive( po );

            if ( rel_ind )
            {
                auto gen_po = generalize( next_po );

                while ( gen_po.level() <= depth() )
                {
                    const auto next = proof_obligation{ gen_po.level() + 1, gen_po.state_vars_cube() };

                    const auto [ rel_ind2, _ ] = is_relative_inductive( next );

                    if ( rel_ind2 )
                        gen_po = next;
                    else
                        break;
                }

                trace( "{}: {}", gen_po.level(), gen_po.state_vars_cube().format() );
                add_blocked_cube( gen_po );

                // This actually seems to worsen sometimes. But for some models,
                // it actually improves it dramatically.
                if ( po.level() <= depth() )
                {
                    const auto next = proof_obligation{ po.level() + 1, po.state_vars_cube() };
                    min_queue.push( next );
                }
            }
            else
            {
                min_queue.push( next_po );
                min_queue.push( po );
            }
        }

        return false;
    }

    // True if invariant found (safe)
    bool propagate()
    {
        trace( "Propagating (k = {})", depth() );

        assert( _trace_blocked_cubes[ depth() ].empty() );

        for ( std::size_t i = 1; i < depth(); ++i )
        {
            // The copy is done since the _trace_blocked_cubes[ i ] will be changed
            // during the forthcoming iteration.
            const auto cubes = _trace_blocked_cubes[ i ];

            for ( const auto& c : cubes )
            {
                const auto po = proof_obligation{ i + 1, c };
                const auto [ rel_ind, _ ] = is_relative_inductive( po );

                if ( rel_ind )
                    add_blocked_cube( po );
            }

            if ( cubes.empty() )
                return true;
        }

        for ( int i = 1; i <= depth(); ++i )
            trace( "  F[ {} ]: {} cubes", i, _trace_blocked_cubes[ i ].size() );

        return false;
    }

    result check()
    {
        while ( true )
        {
            const auto err_state = get_bad_state();

            if ( err_state.has_value() )
            {
                const auto unsafe = solve_obligation( proof_obligation{ depth(), *err_state } );

                if ( unsafe )
                    return counterexample{ {}, {} };
            }
            else
            {
                push_frame();

                if ( propagate() )
                    return ok{};
            }
        }
    }

public:
    pdr( const options& opts, variable_store& store )
            : engine( opts, store ), _transition_activator{ _store->make( "ActT" ) },
              _error_activator{ _store->make( "ActE" ) } {}

    [[nodiscard]]
    result run( const transition_system& system ) override
    {
        _system = &system;
        _solver = std::make_unique< CaDiCaL::Solver >();

        push_frame();

        _activated_init = _system->init().activate( _trace_activators[ 0 ].var() );
        _activated_trans = _system->trans().activate( _transition_activator.var() );
        _activated_error = _system->error().activate( _error_activator.var() );

        assert_formula( _activated_init );
        assert_formula( _activated_trans );
        assert_formula( _activated_error );

        return check();
    }
};

} // alt_pdr