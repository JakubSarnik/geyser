#include <iostream>

#include "cadical.hpp"

int main() {
    std::cout << "Hello, CaDiCaL!\n";

    auto solver = CaDiCaL::Solver{};

    int tie = 1;
    int shirt = 2;

    solver.add (-tie), solver.add (shirt), solver.add (0);
    solver.add (tie), solver.add (shirt), solver.add (0);
    solver.add (-tie), solver.add (-shirt), solver.add (0);

    int res = solver.solve();

    std::cout << "Result: " << res << "\n";

    return 0;
}
