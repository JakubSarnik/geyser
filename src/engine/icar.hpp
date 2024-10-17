#pragma once

#include "base.hpp"
#include "options.hpp"
#include "solver.hpp"
#include <optional>
#include <span>
#include <concepts>
#include <format>

// This is an "incremental" implementation of CAR, as per doc/incremental_car.
// A lot of code here is straight up copied over from the basic implementation
// of CAR. We use the same state pool as in CAR, but a proof obligation scheme
// more similar to classic PDR. Unlike basic CAR, we don't support the backward
// mode to make our implementation simpler and easier to follow.

namespace geyser::icar
{

class bad_cube_handle
{
    friend class cotrace_pool;

    std::size_t _value;

    explicit bad_cube_handle( std::size_t value ) : _value{ value } {}

public:
    friend auto operator<=>( bad_cube_handle, bad_cube_handle ) = default;
};

class bad_cube
{
    cube _state_vars;
    cube _input_vars;
    std::optional< bad_cube_handle > _successor;

public:
    bad_cube( cube state_vars, cube input_vars, std::optional< bad_cube_handle > successor )
            : _state_vars{ std::move( state_vars ) }, _input_vars{ std::move( input_vars ) },
              _successor{ successor } {}

    [[nodiscard]] const cube& state_vars() const { return _state_vars; }
    [[nodiscard]] const cube& input_vars() const { return _input_vars; }
    [[nodiscard]] std::optional< bad_cube_handle > successor() const { return _successor; }
};

class cotrace_pool
{
    std::vector< bad_cube > _entries;

public:
    [[nodiscard]]
    bad_cube_handle make( cube state_vars, cube input_vars,
                          std::optional< bad_cube_handle > successor = std::nullopt )
    {
        _entries.emplace_back( std::move( state_vars ), std::move( input_vars ), successor );
        return bad_cube_handle{ _entries.size() - 1 };
    }

    [[nodiscard]] bad_cube& get( bad_cube_handle handle )
    {
        assert( 0 <= handle._value && handle._value < _entries.size() );
        return _entries[ handle._value ];
    }
};

class proof_obligation
{
    int _level;
    bad_cube_handle _handle;

public:
    proof_obligation( bad_cube_handle handle, int level ) : _level{ level }, _handle{ handle }
    {
        assert( _level >= 0 );
    };

    friend auto operator<=>( const proof_obligation&, const proof_obligation& ) = default;

    [[nodiscard]] int level() const { return _level; }
    [[nodiscard]] bad_cube_handle handle() const { return _handle; }
};

class icar : public engine
{
    variable_store* _store;

    solver _solver;
    const transition_system* _system = nullptr;

    literal _transition_activator;
    literal _error_activator;

    cnf_formula _activated_init;
    cnf_formula _activated_trans;
    cnf_formula _activated_error;

    cnf_formula _init_negated;

    using cube_set = std::vector< cube >;

    std::vector< cube_set > _trace_blocked_cubes;
    std::vector< literal > _trace_activators;

    // The cotrace is now flat. Instead of storing levels, we store activation
    // literals corresponding to added bad cubes and their pool handles. The
    // handles are needed for counterexample reconstruction.
    // Note that we also need to assert all the bad cubes while refreshing the
    // solver!
    std::vector< std::pair< bad_cube_handle, literal > > _cotrace_found_cubes;
    cotrace_pool _cotrace;

    constexpr static int solver_refresh_rate = 5000000;
    int _queries = 0;

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
        _trace_activators.emplace_back( _store->make( std::format( "Act[{}]", depth() + 1 ) ) );
    }

    void add_blocked_to_solver( bad_cube_handle h, literal act );

    void initialize();
    result check();

    std::optional< bad_cube_handle > get_error_state();
    std::optional< counterexample > solve_obligation( const proof_obligation& starting_po );
    counterexample build_counterexample( bad_cube_handle initial );
    bool is_already_blocked( const proof_obligation& po );

    bool has_predecessor( std::span< const literal > s, int i );
    bad_cube_handle get_predecessor( const proof_obligation& po );
    cube generalize_blocked( const proof_obligation& po );
    std::vector< literal > get_minimal_core( std::span< const literal > seed,
                                             std::invocable< std::span< const literal > > auto requery );

    bool propagate();
    bool is_inductive();

    void add_reaching( bad_cube_handle h );
    void add_blocked_at( const cube& c, int level );

    [[maybe_unused]] bool is_state_cube( std::span< const literal > literals ) const;
    [[maybe_unused]] bool is_next_state_cube( std::span< const literal > literals ) const;

    void log_trace_content() const;
    void log_cotrace_content() const;

public:
    icar( const options& opts, variable_store& store )
            : _store{ &store }, _transition_activator{ _store->make( "ActT" ) },
              _error_activator{ _store->make( "ActE" ) } {}

    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace <geyser::car>