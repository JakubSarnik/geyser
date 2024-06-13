#include "options.hpp"
#include "caiger.hpp"
#include <iostream>

int main( int argc, char** argv ) {
    auto opts = geyser::parse_cli( argc, argv );
    auto aig = geyser::make_aiger();

    const char* msg = nullptr;

    if ( opts.input_file.has_value() )
        msg = aiger_open_and_read_from_file( aig.get(), opts.input_file->c_str() );
    else
        msg = aiger_read_from_file( aig.get(), stdin );

    if ( msg != nullptr )
    {
        std::cerr << "error: " << msg << "\n";
        return 1;
    }

    return 0;
}
