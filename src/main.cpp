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
#include "engine/icar.hpp"
#include <string>
#include <iostream>
#include <map>

using namespace geyser;

namespace
{

std::unique_ptr< engine > get_engine( const options& opts, variable_store& store )
{
    const auto& name = opts.engine_name();

    if ( name == "bmc" )
        return std::make_unique< bmc >( opts, store );
    if ( name == "pdr" )
        return std::make_unique< pdr::pdr >( opts, store );
    if ( name == "car" || name == "fcar" )
        return std::make_unique< car::forward_car >( opts, store );
    if ( name == "bcar" )
        return std::make_unique< car::backward_car >( opts, store );
    if ( name == "icar" )
        return std::make_unique< icar::icar >( opts, store );

    return nullptr;
}

void print_help()
{
    std::cout << "Geyser symbolic model checker\n"
                 "Usage: run-geyser -e=<engine> [-v | --verbose] [arguments] <input.aig>\n\n";

    std::cout << "The following engines are available:\n"
                 "  * bmc  - simple bounded model checking\n"
                 "  * pdr  - property directed reachability\n"
                 "  * car  - complementary approximate reachability\n"
                 "  * bcar - backward variant of CAR\n"
                 "  * icar - alternative implementation of forward CAR using CaDiCaL's\n"
                 "           constrain API\n\n";

    std::cout << "Further arguments may be passed to the various engines:\n"
                 "  * bmc\n"
                 "    * -k=<bound> to limit bmc depth\n"
                 "  * pdr - no options at the moment\n"
                 "  * car\n"
                 "    * --no-propagate-cores - propagate blocked cubes as-is, without computation\n"
                 "                             of further unsat cores\n"
                 "    * --repush             - after blocking a proof obligation, try returning to"
                 "                             it again in the next frame (as in PDR)\n"
                 "    * --no-blocked-muc     - don't compute minimal unsat cores in generalization\n"
                 "                             of blocked states, use the cores returned by the\n"
                 "                             solver directly\n"
                 "    * --no-predecessor-muc - similar to --no-blocked-muc, but for predecessor\n"
                 "                             generalization instead\n"
                 "    * --no-cotrace         - don't generalize error states by the use of the\n"
                 "                             cotrace\n"
                 "  * bcar - same as for car, but --no-predecessor-muc has no effect\n"
                 "  * icar\n"
                 "    * --no-cotrace - don't generalize error states by the use of the cotrace\n";
}

} // namespace <anonymous>

int main( int argc, char** argv )
{
    auto opts = parse_cli( argc, argv );

    if ( !opts.has_value() )
    {
        std::cerr << "error: " << opts.error() << "\n\n";
        print_help();
        return 1;
    }

    if ( opts->help_requested() )
    {
        print_help();
        return 0;
    }

    logger::set_verbosity( opts->verbosity() );
    logger::log_loud( "Loading aig from file... " );

    auto aig = make_aiger();
    const char* msg = aiger_open_and_read_from_file( aig.get(), opts->input_file().c_str() );

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
        std::cerr << "error: no engine named " << opts->engine_name() << "\n";
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
