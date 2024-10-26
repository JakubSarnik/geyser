#pragma once

#include "base.hpp"
#include "options.hpp"
#include "solver.hpp"
#include <algorithm>
#include <memory>
#include <vector>
#include <span>
#include <concepts>
#include <format>
#include <random>

namespace geyser::pdr
{

// CTI (counterexample to induction) is either a (possibly generalized)
// model of the query SAT( R[ k ] /\ E ), i.e. it is a model of both state
// variables X and input variables Y so that input Y in state X leads to
// property violation, or a found predecessor of such state. Following
// Bradley's IC3Ref, we store these entries in a memory pool indexed by
// numbers. Also, each entry stores its _successor's index, so that we can
// recover a counterexample.
class cti_handle
{
    friend class cti_pool;

    std::size_t _value;

    explicit cti_handle( std::size_t value ) : _value{ value } {}

public:
    friend auto operator<=>( cti_handle, cti_handle ) = default;
};

class cti_entry
{
    friend class cti_pool;

    cube _state_vars;
    cube _input_vars;
    std::optional< cti_handle > _successor;

public:
    cti_entry( cube state_vars, cube input_vars, std::optional< cti_handle > successor )
            : _state_vars{ std::move( state_vars ) },
              _input_vars{ std::move( input_vars ) },
              _successor{ successor } {}

    [[nodiscard]] const cube& state_vars() const { return _state_vars; }
    [[nodiscard]] const cube& input_vars() const { return _input_vars; }
    [[nodiscard]] std::optional< cti_handle > successor() const { return _successor; }
};

class cti_pool
{
    // CTI entries are allocated in a pool _entries. At each point in time, it
    // holds _num_entries at indices [0, _num_entries). All other entries in
    // [_num_entries, _entries.size()) are unused and their memory is ready to
    // be reused.
    std::vector< cti_entry > _entries;
    std::size_t _num_entries = 0;

public:
    // Beware that the handle is invalidated after the next call to flush!
    [[nodiscard]]
    cti_handle make( cube state_vars, cube input_vars,
                     std::optional< cti_handle > successor = std::nullopt )
    {
        if ( _num_entries >= _entries.size() )
        {
            _entries.emplace_back( std::move( state_vars ), std::move( input_vars ), successor );
        }
        else
        {
            auto& entry = _entries[ _num_entries ];

            entry._state_vars = std::move( state_vars );
            entry._input_vars = std::move( input_vars );
            entry._successor = successor;
        }

        return cti_handle{ _num_entries++ };
    }

    [[nodiscard]] cti_entry& get( cti_handle handle )
    {
        assert( 0 <= handle._value && handle._value < _num_entries );
        return _entries[ handle._value ];
    }

    void flush()
    {
        _num_entries = 0;
    }
};

class proof_obligation
{
    // Declared in this order so that the defaulted comparison operator
    // orders by level primarily.
    int _level;
    cti_handle _handle;

public:
    proof_obligation( cti_handle handle, int level ) : _level{ level }, _handle{ handle }
    {
        assert( _level >= 0 );
    };

    friend auto operator<=>( const proof_obligation&, const proof_obligation& ) = default;

    [[nodiscard]] int level() const { return _level; }
    [[nodiscard]] cti_handle handle() const { return _handle; }
};

class pdr : public engine
{
    const options* _opts;
    variable_store* _store;

    solver _solver;
    const transition_system* _system = nullptr;

    literal _transition_activator;
    literal _error_activator;

    cnf_formula _activated_init; // This is activated by _trace[ 0 ].activator
    cnf_formula _activated_trans;
    cnf_formula _activated_error;

    // The cube corresponding to _init, which is assumed to be a single cube.
    cube _init_cube;

    using cube_set = std::vector< cube >;

    std::vector< cube_set > _trace_blocked_cubes;
    std::vector< literal > _trace_activators;

    // How many solver queries to make before refreshing the solver to remove
    // all the accumulated subsumed clauses.
    constexpr static int solver_refresh_rate = 5000000;
    int _queries = 0;

    cti_pool _ctis;

    std::random_device _rd;
    std::default_random_engine _rng{ _rd() };

    void refresh_solver();

    solver::query_builder with_solver()
    {
        if ( _queries % solver_refresh_rate == 0 )
            refresh_solver();

        ++_queries;

        return _solver.query();
    }

    [[nodiscard]] int depth() const
    {
        return ( int ) _trace_blocked_cubes.size() - 1;
    }

    void push_frame()
    {
        assert( _trace_blocked_cubes.size() == _trace_activators.size() );

        _trace_blocked_cubes.emplace_back();
        _trace_activators.emplace_back( _store->make() );
    }

    std::span< cube_set > frames_from( int level )
    {
        assert( 0 <= level && level <= depth() );
        return std::span{ _trace_blocked_cubes }.subspan( level );
    }

    std::span< literal > activators_from( int level )
    {
        assert( 0 <= level && level <= depth() );
        return std::span{ _trace_activators }.subspan( level );
    }

    void initialize();
    result check();

    std::optional< cti_handle > get_error_cti();
    std::optional< counterexample > solve_obligation( const proof_obligation& starting_po );
    counterexample build_counterexample( cti_handle initial );
    bool is_already_blocked( const proof_obligation& po );

    bool intersects_initial_states( std::span< const literal > c );
    bool is_relative_inductive( std::span< const literal > s, int i );
    cti_handle get_predecessor( const proof_obligation& po );
    std::pair< std::vector< literal >, int > generalize_from_core( std::span< const literal > s, int level );
    std::pair< cube, int > generalize_inductive( const proof_obligation& po );

    void add_blocked_at( const cube& cube, int level, int start_from = 1 );
    bool propagate();

    [[maybe_unused]] bool is_state_cube( std::span< const literal > literals ) const;

    void log_trace_content() const;

public:
    pdr( const options& opts, variable_store& store )
        : _opts{ &opts }, _store{ &store }, _transition_activator{ _store->make() },
          _error_activator{ _store->make() } {}

    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace geyser::pdr