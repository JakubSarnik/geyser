#include "aiger_builder.hpp"

#include <unordered_set>

namespace geyser
{

namespace
{

inline std::string symbol_to_string( std::string prefix, unsigned i, aiger_symbol& symbol )
{
    // Anonymous inputs/state vars get names like y[10]/x[2], respectively.
    if ( symbol.name == nullptr )
        return std::string{ std::move( prefix ) } + "[" + std::to_string( i ) + "]";
    else
        return std::string{ symbol.name };
}

} // namespace <anonymous>

std::expected< transition_system, std::string > aiger_builder::build()
{
    if ( _aig->num_outputs + _aig->num_bad != 1 )
        return std::unexpected( "the input AIG has to contain precisely one output (aiger <1.9)"
                                " or precisely one bad specification (aiger 1.9)");

    if ( _aig->num_fairness > 0 || _aig->num_justice > 0 )
        return std::unexpected( "aiger justice constraints and fairness properties"
                                " are not supported" );

    if ( _aig->num_constraints > 0 )
        return std::unexpected( "NOT IMPLEMENTED: aiger 1.9 invariant constraints are not"
                                " implemented yet" );

    _input_vars_begin = _store->get_variable_count();

    for ( auto i = 0u; i < _aig->num_inputs; ++i )
        _store->make( symbol_to_string( "y", i, _aig->inputs[ i ] ) );

    _input_vars_end = _store->get_variable_count();
    _state_vars_begin = _input_vars_end;

    // The following two loops CANNOT be merged because of sequentiality constraints!
    for ( auto i = 0u; i < _aig->num_latches; ++i )
        _store->make( symbol_to_string( "x", i, _aig->latches[ i ] ) );

    _state_vars_end = _store->get_variable_count();
    _next_state_vars_begin = _state_vars_end;

    for ( auto i = 0u; i < _aig->num_latches; ++i )
        _store->make( symbol_to_string( "x'", i, _aig->latches[ i ] ) );

    _next_state_vars_end = _store->get_variable_count();
    _and_vars_begin = _next_state_vars_end;

    for ( auto i = 0u; i < _aig->num_ands; ++i )
        _store->make( std::string{"and["} + std::to_string( i ) + "]" );

    _and_vars_end = _store->get_variable_count();

    auto init = build_init();
    auto trans = build_trans();
    auto error = build_error();

    return transition_system
    {
        { _input_vars_begin, _input_vars_end },
        { _state_vars_begin, _state_vars_end },
        { _next_state_vars_begin, _next_state_vars_end },
        std::move( init ),
        std::move( trans ),
        std::move( error )
    };
}

// The Aiger to CNF conversion is heavily inspired by the code in IC3Ref
// by Aaron Bradley with the important difference that we don't trivially map
// Aiger variable IDs onto our solver variable IDs.

variable aiger_builder::aiger_var_to_our_var( aiger_literal lit, bool primed ) const
{
    // The aiger lib expects this to be a positive literal (i.e. a variable).
    assert( lit % 2 == 0 );

    if ( const auto *ptr = aiger_is_input( _aig, lit ); ptr )
    {
        const auto pos = static_cast< int >( ptr - _aig->inputs );
        return get_input_var( pos );
    }

    if ( const auto *ptr = aiger_is_latch( _aig, lit ); ptr )
    {
        const auto pos = static_cast< int >( ptr - _aig->latches );

        if ( primed )
            return get_next_state_var( pos );
        else
            return get_input_var( pos );
    }

    if ( const auto *ptr = aiger_is_and( _aig, lit ); ptr )
    {
        const auto pos = static_cast< int >( ptr - _aig->ands );
        return get_and_var( pos );
    }

    assert( false ); // Unreachable
}

// Turn an Aiger declaration
//   lhs = rhs0 /\ rhs1
// into a set of clauses using a Tseitin transformation. This must take care
// when either of rhs0/rhs1 is a constant 0 (false) or 1 (true).
cnf_formula aiger_builder::clausify_and( aiger_literal lhs, aiger_literal rhs0, aiger_literal rhs1 )
{
    const auto make_equivalence = [ & ]( aiger_literal x, aiger_literal y ){
        // x = y [i.e. x <-> y]
        // ~> (x -> y) /\ (y -> x)
        // ~> (-x \/ y) /\ (-y \/ x)

        auto res = cnf_formula{};

        res.add_clause( !aiger_lit_to_our_lit( x ), aiger_lit_to_our_lit( y ) );
        res.add_clause( !aiger_lit_to_our_lit( y ), aiger_lit_to_our_lit( x ) );

        return res;
    };

    // Anything AND false is a contradiction, i.e. a formula with an empty
    // clause.
    if ( rhs0 == aiger_false || rhs1 == aiger_false )
    {
        auto res = cnf_formula{};
        res.add_clause( {} );

        return res;
    }

    // lhs = true
    if ( rhs0 == aiger_true && rhs1 == aiger_true )
    {
        auto res = cnf_formula{};
        res.add_clause( aiger_lit_to_our_lit( lhs ) );

        return res;
    }

    if ( rhs0 == aiger_true )
        return make_equivalence( lhs, rhs1 );

    if ( rhs1 == aiger_true )
        return make_equivalence( lhs, rhs0 );

    auto res = cnf_formula{};

    // lhs = rhs0 /\ rhs1
    // ~> (lhs -> rhs0) /\ (lhs -> rhs1) /\ (rhs0 /\ rhs1 -> lhs)
    // ~> (-lhs \/ rhs0) /\ (-lhs \/ rhs1) /\ (-rhs0 \/ -rhs1 \/ lhs)

    res.add_clause( !aiger_lit_to_our_lit( lhs ), aiger_lit_to_our_lit( rhs0 ) );
    res.add_clause( !aiger_lit_to_our_lit( lhs ), aiger_lit_to_our_lit( rhs1 ) );
    res.add_clause( !aiger_lit_to_our_lit( rhs0 ), !aiger_lit_to_our_lit( rhs1 ),
                    aiger_lit_to_our_lit( lhs ) );

    return res;
}

cnf_formula aiger_builder::build_init()
{
    auto init = cnf_formula{};

    for ( auto i = 0u; i < _aig->num_latches; ++i )
    {
        const auto reset = _aig->latches[ i ].reset;

        // In Aiger 1.9, the reset can be either 0 (initialized as false),
        // 1 (initialized as true) or equal to the latch literal, which means
        // that the latch has a nondeterministic initial value.

        if ( aiger_is_constant( reset ) )
            init.add_clause( literal{ get_state_var( static_cast< int >( i ) ), reset == 0 } );
    }

    return init;
}

cnf_formula aiger_builder::build_trans()
{
    // TODO
    return cnf_formula();
}

cnf_formula aiger_builder::build_error()
{
    // Do a reverse traversal from the Aiger literal representing the error
    // (either the single output or the single bad property) through all the
    // ANDs and ending with leaves consisting of state variables and inputs.
    //
    // NOTE: Aiger literals are the numbers in the aiger file
    // ([0 .. 2 * (aig.maxvar) + 1]; 0 = false, 1 = true), which does NOT
    // correspond to our variable ordering in any way.
    //
    // Notably, aiger literal's parity denotes whether it's positive (even)
    // or negative (odd).

    auto error = cnf_formula{};

    auto required = std::unordered_set< aiger_literal >{
        ( _aig->num_outputs > 0 ? _aig->outputs[ 0 ] : _aig->bad[ 0 ] ).lit
    };

    // I hate unsigned integers.
    for ( auto i = _aig->num_ands - 1; i --> 0; )
    {
        const auto lhs = _aig->ands[ i ].lhs;
        const auto rhs0 = _aig->ands[ i ].rhs0;
        const auto rhs1 = _aig->ands[ i ].rhs1;

        if ( !required.contains( lhs ) && !required.contains( aiger_not( lhs ) ) )
            continue;

        error.add_cnf( clausify_and( lhs, rhs0, rhs1 ) );

        required.insert( rhs0 );
        required.insert( rhs1 );
    }

    return error;
}

} // namespace geyser