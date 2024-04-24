cmake_minimum_required(VERSION 3.28)
project(geyser)

set(CMAKE_CXX_STANDARD 23)

add_library(libaiger STATIC dep/aiger/aiger.c)
#add_library(libgeyser STATIC)
add_executable(geyser src/main.cpp)

#target_link_libraries(libgeyser PUBLIC libaiger)
#target_include_directories(libgeyser PUBLIC "dep/aiger")

#target_link_libraries(geyser PUBLIC libgeyser)
