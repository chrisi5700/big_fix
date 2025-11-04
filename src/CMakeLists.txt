# Add library definitions here
# See README.md for CMake library patterns and examples


target_add_library(bigfix BigFix.cpp)
target_include_directories(bigfix PUBLIC ${CMAKE_SOURCE_DIR}/include)