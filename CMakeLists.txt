cmake_minimum_required(VERSION 3.28)
project(geyser)

set(CMAKE_CXX_STANDARD 23)

add_library(libgeyser STATIC aiger/aiger.c)
add_executable(geyser main.cpp)

target_link_libraries(geyser PUBLIC libgeyser)
target_include_directories(geyser PUBLIC "${PROJECT_SOURCE_DIR}/aiger")
