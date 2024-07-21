#pragma once

#include "base.hpp"
#include "cadical.hpp"
#include <algorithm>
#include <memory>
#include <vector>
#include <span>

namespace geyser::pdr
{

struct sorted_tag_t {};
constexpr sorted_tag_t sorted_tag{};

class ordered_cube
{
    std::vector< literal > _literals;

public:
    explicit ordered_cube( std::vector< literal > literals )
        : _literals{ std::move( literals ) }
    {
        std::ranges::sort( _literals );
    };

    ordered_cube( std::vector< literal > literals, sorted_tag_t ) // NOLINT
        : _literals{ std::move( literals ) }
    {
        assert( std::ranges::is_sorted( _literals ) );
    }

    friend bool operator==( const ordered_cube&, const ordered_cube& ) = default;

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

    [[nodiscard]]
    std::optional< literal > find( variable var ) const
    {
        const auto lit = literal{ var };

        if ( std::ranges::binary_search( _literals, lit ) )
            return lit;
        if ( std::ranges::binary_search( _literals, !lit ) )
            return !lit;

        return {};
    }

    [[nodiscard]]
    const std::vector< literal >& literals() const { return _literals; }
};

// CTI (counterexample to induction) is either a (possibly generalized)
// model of the query SAT( R[ k ] /\ E ), i.e. it is a model of both state
// variables X and input variables Y so that input Y in state X leads to
// property violation, or a found predecessor of such state. Following
// Bradley's IC3Ref, we store these entries in a memory pool indexed by
// numbers. Also, each entry stores its _successor's index, so that we can
// recover a counterexample.
using cti_handle = int;

// TODO: Investigate whether we want to remove states in the middle
//       of the pool and keep a freelist/used flag!.
class cti_entry
{
    friend class cti_pool;

    ordered_cube _state_vars;
    ordered_cube _input_vars;
    std::optional< cti_handle > _successor;

public:
    cti_entry( ordered_cube state_vars, ordered_cube input_vars, std::optional< cti_handle > successor )
            : _state_vars{ std::move( state_vars ) },
              _input_vars{ std::move( input_vars ) },
              _successor{ successor } {}

    [[nodiscard]] const ordered_cube& state_vars() const { return _state_vars; }
    [[nodiscard]] const ordered_cube& input_vars() const { return _input_vars; }
    [[nodiscard]] std::optional< cti_handle > successor() const { return _successor; }
};

class cti_pool
{
    // CTI entries are allocated in a pool _entries. At each point in time, it
    // holds _num_entries at indices [0, _num_entries). All other entries in
    // [_num_entries, _entries.size()) are unused and their memory is ready to
    // be reused.
    std::vector< cti_entry > _entries;
    cti_handle _num_entries = 0;

public:
    // Beware that the handle is invalidated after the next call to flush!
    [[nodiscard]]
    cti_handle make( ordered_cube state_vars, ordered_cube input_vars,
                     std::optional< cti_handle > successor = std::nullopt )
    {
        if ( _num_entries >= ( cti_handle ) _entries.size() )
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

        return _num_entries++;
    }

    [[nodiscard]] cti_entry& get( cti_handle handle )
    {
        assert( 0 <= handle && handle < _num_entries );
        return _entries[ handle ];
    }

    void flush()
    {
        _num_entries = 0;
    }
};

// TODO: Bradley also stores distance to the error and uses it as
//       a heuristic in the ordering. Investigate? (Pass it by ref if it
//       becomes larger.)
class proof_obligation
{
    // Declared in this order so that the defaulted comparison operator
    // orders by level primarily.
    int _level;
    cti_handle _handle;

public:
    proof_obligation( cti_handle handle, int level ) : _level{ level }, _handle{ handle } {}; // NOLINT

    friend auto operator<=>( proof_obligation, proof_obligation ) = default;

    [[nodiscard]] int level() const { return _level; }
    [[nodiscard]] cti_handle handle() const { return _handle; }
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

        query_builder& assume( literal l )
        {
            _solver->assume( l.value() );
            return *this;
        }

        query_builder& assume( std::span< const literal > literals )
        {
            for ( const auto l : literals )
                assume( l );

            return *this;
        }

        query_builder& assume( std::initializer_list< literal > literals )
        {
            return assume( { literals.begin(), literals.end() } );
        }

        query_builder& constrain( const cnf_formula& clause )
        {
            assert( std::ranges::count( clause.literals(), literal::separator ) == 1 );

            for ( const auto l : clause.literals() )
                _solver->constrain( l.value() );

            return *this;
        }

        bool is_sat()
        {
            const auto res = _solver->solve();
            assert( res != CaDiCaL::UNKNOWN );

            return res == CaDiCaL::SATISFIABLE;
        }
    };

    using engine::engine;

    std::unique_ptr< CaDiCaL::Solver > _solver;
    const transition_system* _system = nullptr;

    literal _transition_activator;
    literal _error_activator;

    cnf_formula _activated_init; // This is activated by _trace[ 0 ].activator
    cnf_formula _activated_trans;
    cnf_formula _activated_error;

    using cube_set = std::vector< ordered_cube >;

    std::vector< cube_set > _trace_blocked_cubes;
    std::vector< literal > _trace_activators;

    // How many solver queries to make before refreshing the solver to remove
    // all the accumulated subsumed clauses.
    constexpr static int solver_refresh_rate = 5000;
    int _queries = 0;

    cti_pool _ctis;

    void refresh_solver();

    query_builder with_solver()
    {
        if ( _queries++ % solver_refresh_rate == 0 )
            refresh_solver();

        assert( _solver );
        return query_builder{ *_solver };
    }

    // TODO: This is present also in the BMC engine, decompose by moving into
    //       base? Also is_true below.
    void assert_formula( const cnf_formula& formula )
    {
        assert( _solver );

        for ( const auto lit : formula.literals() )
            _solver->add( lit.value() );
    }

    bool is_true( variable var )
    {
        assert( _solver );
        assert( ( _solver->state() & CaDiCaL::SATISFIED ) != 0 );

        return _solver->val( var.id() ) > 0;
    }

    ordered_cube get_model( variable_range range )
    {
        auto val = valuation{};
        val.reserve( range.size() );

        for ( const auto var : range )
            val.emplace_back( var, !is_true( var ) );

        return ordered_cube{ val };
    }

    // Get a cube that contains only those literals from 'cube' that are in the
    // unsat core.
    ordered_cube get_unsat_core( const ordered_cube& cube )
    {
        assert( _solver );
        assert( ( _solver->state() & CaDiCaL::UNSATISFIED ) != 0 );

        auto lits = std::vector< literal >{};

        for ( const auto lit : cube.literals() )
            if ( _solver->failed( lit.value() ) )
                lits.emplace_back( lit );

        return ordered_cube{ lits, sorted_tag };
    }

    [[nodiscard]] int k() const
    {
        assert( _trace_blocked_cubes.size() == _trace_activators.size() );
        return (int) _trace_blocked_cubes.size() - 1;
    }

    void push_frame()
    {
        assert( _trace_blocked_cubes.size() == _trace_activators.size() );

        _trace_blocked_cubes.emplace_back();
        _trace_activators.emplace_back( _store->make( std::format( "Act[{}]", k() + 1 ) ) );
    }

    std::span< cube_set > frames_from( int level )
    {
        assert( 0 <= level && level <= k() );
        return std::span{ _trace_blocked_cubes }.subspan( level );
    }

    std::span< literal > activators_from( int level )
    {
        assert( 0 <= level && level <= k() );
        return std::span{ _trace_activators }.subspan( level );
    }

    void initialize();
    result check( int bound );

    std::optional< counterexample > block();

    std::optional< counterexample > solve_obligation( proof_obligation cti_po );
    counterexample build_counterexample( cti_handle initial );
    bool is_already_blocked( proof_obligation po );
    cti_handle generalize_predecessor( cti_handle cti );
    cti_handle generalize_inductive( proof_obligation po );
    void add_blocked_at( const ordered_cube& cube, int level, int start_from = 1 );

    bool propagate(); // Returns true if an invariant has been found

    ordered_cube prime_cube( const ordered_cube& cube ) const;

public:
    [[nodiscard]] result run( const transition_system& system ) override;
};

} // namespace geyser::pdr