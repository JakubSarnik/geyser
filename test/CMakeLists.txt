add_subdirectory(${CMAKE_SOURCE_DIR}/dep/catch2 ${CMAKE_BINARY_DIR}/catch2)

add_executable(test
        logic.cpp
        transition_system.cpp
        aiger_builder.cpp
        options.cpp
        pdr.cpp
        car.cpp
)

target_link_libraries(test PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test PRIVATE geyser)