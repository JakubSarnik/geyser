#include "witness_writer.hpp"
#include <format>

namespace geyser
{

namespace
{

// TODO: If we want to allow more bad property specifications in the input,
//       make this dynamic.
constexpr const char* property = "b0";

std::string row( const valuation& val )
{
    auto res = std::string{};

    // We assume that the order of iteration (i.e. with increasing ID) is the
    // same as the order of the original variables in the Aiger.

    for ( const auto& [ _, state ] : val )
        res += state ? '1' : '0';

    res += "\n";

    return res;
}

struct write_visitor
{
    std::string operator()( const ok& )
    {
        return std::format( "0\n{}\n.\n", property );
    }

    std::string operator()( const unknown& unknown )
    {
        // Include the reason as a comment.
        return std::format( "2\n{}\nc {}\n.\n", property, unknown.reason );
    }

    std::string operator()( const counterexample& cex )
    {
        auto witness = std::format( "1\n{}\n", property );

        if ( !cex.states().empty() )
            witness += row( cex.states().front() );

        for ( const auto& in : cex.inputs() )
            witness += row( in );

        witness += ".\n";

        return witness;
    }
};

} // namespace <anonymous>

std::string write_aiger_witness( const result& res )
{
    return std::visit( write_visitor{}, res );
}

} // namespace geyser