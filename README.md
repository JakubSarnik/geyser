# Geyser

Geyser is a prototype of a symbolic model checker for propositional transition
systems, currently in development. Its goal is to be a testbed for various
model checking algorithms, including PDR, CAR and its variants.

## Third party code

The repository contains a copy of CaDiCaL SAT solver by Armin Biere et al. (in
`dep/cadical`) and a part of his Aiger library for manipulating and-inverter
graphs (`dep/aiger`). Additionally, Catch2 is included for testing (`dep/catch2`).
See the respective directories for licenses of those parts.