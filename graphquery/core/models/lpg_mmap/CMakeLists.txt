cmake_minimum_required(VERSION 3.9)

add_library(
        lpg_mmap
        SHARED)

target_include_directories(
        lpg_mmap
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        lpg_mmap
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lpg_mmap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transaction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/block_file.hpp)

target_compile_options(
        lpg_mmap
        PUBLIC
        -fPIC)

target_link_libraries(
        lpg_mmap
        PUBLIC
        diskdriver
        logsystem
        fmt)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/models)
add_custom_command(
        TARGET lpg_mmap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lpg_mmap> ${output_directory})