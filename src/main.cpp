#include "options.hpp"
#include "caiger.hpp"
#include "logic.hpp"
#include "aiger_builder.hpp"
#include "witness_writer.hpp"
#include "engine/base.hpp"
#include "engine/bmc.hpp"
#include "engine/pdr.hpp"
#include <string>
#include <iostream>
#include <map>

using namespace geyser;

namespace
{

std::unique_ptr< engine > get_engine( const options& opts, variable_store& store )
{
    const auto& name = opts.engine_name;

    if ( name == "bmc" )
        return std::make_unique< bmc >( opts, store );
    if ( name == "pdr" )
        return std::make_unique< pdr::pdr >( opts, store );

    return nullptr;
}

} // namespace <anonymous>

int main( int argc, char** argv )
{
    auto opts = parse_cli( argc, argv );

    if ( !opts.has_value() )
    {
        std::cerr << "error: " << opts.error() << "\n";
        return 1;
    }

    const auto trace = [ & ]( const std::string& s )
    {
        if ( opts->verbosity >= verbosity_level::loud )
            std::cout << s << std::flush;
    };

    auto aig = make_aiger();

    const char* msg = nullptr;

    trace( "Loading aig from file... " );

    if ( opts->input_file.has_value() )
        msg = aiger_open_and_read_from_file( aig.get(), opts->input_file->c_str() );
    else
        msg = aiger_read_from_file( aig.get(), stdin );

    if ( msg != nullptr )
    {
        std::cerr << "error: " << msg << "\n";
        return 1;
    }

    trace( "OK\n" );
    trace( "Loading the engine... " );

    auto store = variable_store{};
    auto engine = get_engine( *opts, store );

    if ( !engine )
    {
        std::cerr << "error: no engine named " << opts->engine_name << "\n";
        return 1;
    }

    trace( "OK\n" );
    trace( "Building the transition system... " );

    auto system = builder::build_from_aiger( store, *aig );

    if ( !system.has_value() )
    {
        std::cerr << "error: " << system.error() << "\n";
        return 1;
    }

    trace( "OK\n" );
    trace( "Running...\n\n" );

    const auto res = engine->run( *system );

    trace( "\nFinished\n" );
    trace( "Printing the witness to stdout...\n\n" );

    std::cout << write_aiger_witness( res );

    return 0;
}
