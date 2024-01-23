cmake_minimum_required(VERSION 3.9)

add_library(
        bfs
        SHARED
        bfs.cpp
        bfs.h)

target_include_directories(
        bfs
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        bfs
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/bfs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bfs.h)

target_link_libraries(
        bfs
        PUBLIC
        logsystem
        fmt)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET bfs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bfs> ${output_directory})