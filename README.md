# Geyser

Geyser is a simple symbolic model checker for propositional transition system
systems. Its goal is to be a testbed for various model checking algorithms,
mainly property directed reachability (PDR), complementary approximate
reachability (CAR), and variants of thereof. The code is part of my master's
thesis at the Faculty of Informatics, Masaryk University.

At the moment, the following algorithms are implemented:
- Bounded model checking (`engine/bmc`)
- Property directed reachability (`engine/pdr`)
- Complementary approximate reachability (`engine/car`)

The model checker works with input models given in the format Aiger 1.9
(https://fmv.jku.at/aiger/) with several restrictions. Justice properties and
fairness constraints are not and will not be supported. Invariance constraints
and multiple outputs are as of now also unsupported. The output is similarly
reported according to the counterexample syntax of Aiger 1.9.

## Compilation and usage

The project requires CMake 3.26 or newer. It also uses various features of
C++23 and thus requires a reasonably modern C++ toolchain. It is known to build
with clang/libc++ 16.0.0 and gcc/libstdc++ 13.1.0.

To build (assuming the repository was cloned to `geyser`):
```
cd geyser
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

After building, run with `./run-geyser [-v] -e <engine name> <input>`, where
`<engine name>` is one of the supported engines (e.g. `pdr`, `car` or `bmc`)
and `<input>` is a path to a transition system in the Aiger format. Geyser
then checks that the system is correct and outputs the result to the standard
output. The option `-v` enables verbose printing of progress and other
information. For full description of all the options, run
`./run-geyser --help`.

## Third-party code

The repository contains a copy of CaDiCaL SAT solver by Armin Biere et al. (in
`dep/cadical`) and a part of his Aiger library for manipulating and-inverter
graphs (`dep/aiger`). Additionally, Catch2 is included for testing (`dep/catch2`).
See the respective directories for licenses of third-party code.