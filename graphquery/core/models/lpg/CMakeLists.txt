cmake_minimum_required(VERSION 3.9)

add_library(lpg SHARED)

target_include_directories(
        lpg
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        lpg
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lpg.cpp)

target_compile_options(
        lpg
        PUBLIC
        -Wall
        -Werror
        -Wpedantic
        -g)

target_link_libraries(
        lpg
        PUBLIC
        diskdriver
        logsystem
        fmt)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/models)
add_custom_command(
        TARGET lpg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lpg> ${output_directory})