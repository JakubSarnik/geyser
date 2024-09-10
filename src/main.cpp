#include "options.hpp"
#include "caiger.hpp"
#include "logic.hpp"
#include "aiger_builder.hpp"
#include "witness_writer.hpp"
#include "logger.hpp"
#include "engine/base.hpp"
#include "engine/bmc.hpp"
#include "engine/pdr.hpp"
#include "engine/car.hpp"
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
    if ( name == "car" || name == "fcar" )
        return std::make_unique< car::forward_car >( opts, store );
    if ( name == "bcar" )
        return std::make_unique< car::backward_car >( opts, store );

    return nullptr;
}

} // namespace <anonymous>

int main( int argc, char** argv )
{
    auto opts = parse_cli( argc, argv );

    // TODO: Implement --help!
    if ( !opts.has_value() )
    {
        std::cerr << "error: " << opts.error() << "\n";
        return 1;
    }

    logger::set_verbosity( opts->verbosity );

    auto aig = make_aiger();

    const char* msg = nullptr;

    logger::log_loud( "Loading aig from file... " );

    if ( opts->input_file.has_value() )
        msg = aiger_open_and_read_from_file( aig.get(), opts->input_file->c_str() );
    else
        msg = aiger_read_from_file( aig.get(), stdin );

    if ( msg != nullptr )
    {
        std::cerr << "error: " << msg << "\n";
        return 1;
    }

    logger::log_loud( "OK\n" );
    logger::log_loud( "Loading the engine... " );

    auto store = variable_store{};
    auto engine = get_engine( *opts, store );

    if ( !engine )
    {
        std::cerr << "error: no engine named " << opts->engine_name << "\n";
        return 1;
    }

    logger::log_loud( "OK\n" );
    logger::log_loud( "Building the transition system... " );

    auto system = builder::build_from_aiger( store, *aig );

    if ( !system.has_value() )
    {
        std::cerr << "error: " << system.error() << "\n";
        return 1;
    }

    logger::log_loud( "OK\n" );
    logger::log_loud( "Running...\n\n" );

    const auto res = engine->run( *system );

    logger::log_loud( "\nFinished\n" );
    logger::log_loud( "Printing the witness to stdout...\n\n" );

    std::cout << write_aiger_witness( res );

    return 0;
}
