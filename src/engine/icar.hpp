#pragma once

#include "base.hpp"
#include "options.hpp"
#include "solver.hpp"
#include "car.hpp"
#include <optional>
#include <span>
#include <concepts>
#include <format>

// This is an "incremental" implementation of CAR, as per doc/incremental_car.
// A lot of code here is straight up copied over from the basic implementation
// of CAR. We use the same scheme for proof obligation handling and the same
// state pool as well. Unlike basic CAR, we don't support the backward mode
// to make our implementation simpler and easier to follow.

namespace geyser::car
{

class icar : public engine
{
    car_options _opts;
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

public:
    icar( const options& opts, variable_store& store )
            : _opts{ opts }, _store{ &store }, _transition_activator{ _store->make( "ActT" ) },
              _error_activator{ _store->make( "ActE" ) } {}

    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace <geyser::car>