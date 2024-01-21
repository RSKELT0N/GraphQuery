cmake_minimum_required(VERSION 3.9)

add_library(
        lpg_vector
        SHARED)

target_include_directories(
        lpg_vector
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        lpg_vector
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lpg_vector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transaction.cpp)

target_compile_options(
        lpg_vector
        PUBLIC
        -fPIC)

target_link_libraries(
        lpg_vector
        PUBLIC
        diskdriver
        logsystem
        fmt)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/models)
add_custom_command(
        TARGET lpg_vector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lpg_vector> ${output_directory})