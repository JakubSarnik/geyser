add_subdirectory(${CMAKE_SOURCE_DIR}/dep/catch2 ${CMAKE_BINARY_DIR}/catch2)

add_executable(test formula.cpp)

target_link_libraries(test PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test PRIVATE geyser)