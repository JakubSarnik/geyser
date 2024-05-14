#include "aiger_builder.hpp"

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

std::expected< transition_system, std::string > aiger_builder::build( const aiger& aig )
{
    if ( aig.num_outputs + aig.num_bad != 1 )
        return std::unexpected( "the input AIG has to contain precisely one output (aiger <1.9)"
                                " or precisely one bad specification (aiger 1.9)");

    if ( aig.num_fairness > 0 || aig.num_justice > 0 )
        return std::unexpected( "aiger justice constraints and fairness properties"
                                " are not supported" );

    _input_vars.reserve( aig.num_inputs );
    _state_vars.reserve( aig.num_latches );
    _next_state_vars.reserve( aig.num_latches );

    for ( auto i = 0u; i < aig.num_inputs; ++i )
        _input_vars.push_back( _store->make( symbol_to_string( "y", i, aig.inputs[ i ] ) ) );

    // The following two loops CANNOT be merged because of sequentiality constraints!
    for ( auto i = 0u; i < aig.num_latches; ++i )
        _state_vars.push_back( _store->make( symbol_to_string( "x", i, aig.latches[ i ] ) ) );

    for ( auto i = 0u; i < aig.num_latches; ++i )
        _next_state_vars.push_back( _store->make( symbol_to_string("x'", i, aig.latches[ i ]) ) );

    return transition_system
    {
        std::move( _input_vars ),
        std::move( _state_vars ),
        std::move( _next_state_vars ),
        build_init( aig ),
        build_trans( aig ),
        build_error( aig )
    };
}

cnf_formula aiger_builder::build_init( const aiger& aig )
{
    return cnf_formula();
}

cnf_formula aiger_builder::build_trans( const aiger& aig )
{
    return cnf_formula();
}

cnf_formula aiger_builder::build_error( const aiger& aig )
{
    return cnf_formula();
}

} // namespace geyser