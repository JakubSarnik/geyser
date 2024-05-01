cmake_minimum_required(VERSION 3.28)
project(geyser)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 23)

add_library(aiger STATIC dep/aiger/aiger.c)

set(AIGER_HEADER_DIR "${CMAKE_SOURCE_DIR}/dep/aiger")

# CaDiCaL is not a CMake project and is built according to dep/cadical/BUILD.md
# Note that with the following settings, it is always build optimized and without
# debug symbols, even for a debug build of geyser.

ExternalProject_Add(cadical_build
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/dep/cadical"
    CONFIGURE_COMMAND "${CMAKE_SOURCE_DIR}/dep/cadical/configure"
    BUILD_COMMAND "make"
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    BUILD_IN_SOURCE 1 # We need this because the configure script contains relative paths
)

set(CADICAL_HEADER_DIR "${CMAKE_SOURCE_DIR}/dep/cadical/src")
set(CADICAL_BINARY_DIR "${CMAKE_SOURCE_DIR}/dep/cadical/build")

add_library(cadical STATIC IMPORTED)
set_target_properties(cadical PROPERTIES IMPORTED_LOCATION "${CADICAL_BINARY_DIR}/libcadical.a")
add_dependencies(cadical cadical_build)

add_executable(geyser src/main.cpp
        src/formula.cpp
        src/transition_system.cpp
        src/options.cpp
        src/engine/base.cpp
        src/engine/bmc.cpp)

target_link_libraries(geyser PUBLIC aiger)
target_link_libraries(geyser PUBLIC cadical)

target_include_directories(geyser PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(geyser PUBLIC "${AIGER_HEADER_DIR}")
target_include_directories(geyser PUBLIC "${CADICAL_HEADER_DIR}")