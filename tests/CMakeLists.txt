cmake_minimum_required(VERSION 3.18)

project(pico_skyace_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pico_skyace_host_tests
    game_rules_test.cpp
    ../src/game/fixed_math.cpp
    ../src/game/game_rules.cpp
)
target_include_directories(pico_skyace_host_tests PRIVATE ../src)
target_compile_options(pico_skyace_host_tests PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME pico_skyace_host_tests COMMAND pico_skyace_host_tests)
endif()
