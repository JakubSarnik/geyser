#pragma once

#include "base.hpp"
#include "options.hpp"
#include "solver.hpp"
#include <optional>
#include <span>
#include <concepts>
#include <format>

namespace geyser::car
{

// This code is to a large degree taken from the PDR engine.

// In PDR, we had a pool of CTIs. Here, the role is actually played by the
// cotrace. Unlike the pool, we need to keep it segregated by level (distance
// of the known path to the error state), and we never flush the pool. However,
// we actually might remove some states from the cotrace (if we find they are
// subsumed at the same level). Hence, we keep a simple pool as well, and our
// cotrace just points to its entries.

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

// There is no freeing of memory. We would only do that to release the memory
// of subsumed cubes, which is not worth the hassle.
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
    int _level; // The i so that the state is in F_i
    int _colevel; // The j so that the state is in B_j
    bad_cube_handle _handle;

public:
    proof_obligation( bad_cube_handle handle, int level, int colevel )
        : _level{ level }, _colevel{ colevel }, _handle{ handle }
    {
        assert( _level >= 0 );
        assert( _colevel >= 0 );
    };

    // Note that this defaulted comparison orders first by level (lesser frame
    // first) and then by colevel (closer to error first). This might be in
    // conflict with the order in which we scan the cotrace in check.
    // TODO: Investigate
    friend auto operator<=>( const proof_obligation&, const proof_obligation& ) = default;

    [[nodiscard]] int level() const { return _level; }
    [[nodiscard]] int colevel() const { return _colevel; }
    [[nodiscard]] bad_cube_handle handle() const { return _handle; }
};

class car : public engine
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

    // The negation of the init formula. How it's build differs between forward
    // and backward modes.
    cnf_formula _init_negated;

    using cube_set = std::vector< cube >;
    using handle_set = std::vector< bad_cube_handle >;

    std::vector< cube_set > _trace_blocked_cubes;
    std::vector< literal > _trace_activators;
    std::vector< handle_set > _cotrace_found_cubes;

    constexpr static int solver_refresh_rate = 5000000;
    int _queries = 0;

    // In principle, this engine shouldn't care about the direction. However,
    // there are still a few places where we need to care:
    //   1. Backward car needs to reverse counterexamples.
    //   2. Backward car cannot perform predecessor generalization.
    bool _forward;

    cotrace_pool _cotrace;

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

    [[nodiscard]] int codepth() const
    {
        return ( int ) _cotrace_found_cubes.size() - 1;
    }

    void push_frame()
    {
        assert( _trace_blocked_cubes.size() == _trace_activators.size() );

        _trace_blocked_cubes.emplace_back();
        _trace_activators.emplace_back( _store->make( std::format( "Act[{}]", depth() + 1 ) ) );
    }

    void push_coframe()
    {
        _cotrace_found_cubes.emplace_back();
    }

    void initialize();
    result check();
    std::optional< counterexample > check_existing_cotrace();
    std::optional< counterexample > check_new_error_states();

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

    void add_reaching_at( bad_cube_handle h, int level );
    void add_blocked_at( const cube& c, int level );

    [[maybe_unused]] bool is_state_cube( std::span< const literal > literals ) const;
    [[maybe_unused]] bool is_next_state_cube( std::span< const literal > literals ) const;

    void log_trace_content() const;
    void log_cotrace_content() const;

    cnf_formula negate_cnf( const cnf_formula& f );

public:
    car( const options& opts, variable_store& store, bool forward )
        : _opts{ &opts }, _store{ &store }, _transition_activator{ _store->make( "ActT" ) },
          _error_activator{ _store->make( "ActE" ) }, _forward{ forward } {}

    [[nodiscard]] result run( const transition_system& system ) override;
};

class forward_car : public car
{
public:
    forward_car( const options& opts, variable_store& store )
        : car( opts, store, true ) {}
};

class backward_car : public car
{
    static transition_system reverse_system( const transition_system& system );

public:
    backward_car( const options& opts, variable_store& store )
            : car( opts, store, false ) {}

    [[nodiscard]] result run( const transition_system& system ) override
    {
        const auto reversed = reverse_system( system );
        return car::run( reversed );
    }
};

} // namespace geyser::car