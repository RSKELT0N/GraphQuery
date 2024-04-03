# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

add_compile_definitions(PROJECT_ROOT="${PROJECT_SOURCE_DIR}")
add_compile_definitions(PROJECT_NAME="${PROJECT_NAME}")

set(SUBMODULE_TESTS tests)
set(TESTS_FOLDER unit_tests)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB_RECURSE TEST_SOURCES
        main.cpp
        ${TESTS_FOLDER}/*.cpp
        ../core/db/*.cpp
        ../core/db/*.hpp
        ../core/interact/*.cpp
        ../core/log/*.cpp)

add_executable(${SUBMODULE_TESTS} ${TEST_SOURCES})

target_link_libraries(
        ${SUBMODULE_TESTS}
        PUBLIC
        gtest
        dylib
        imgui
        fmt
        csv "--coverage")

# Add program include directories
target_include_directories(
        ${SUBMODULE_TESTS}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../external
        ../core)

include(GoogleTest)
gtest_discover_tests(${SUBMODULE_TESTS})
