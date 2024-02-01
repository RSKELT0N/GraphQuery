cmake_minimum_required(VERSION 3.9)

add_library(
        sssp
        SHARED
        sssp.cpp
        sssp.h)

target_include_directories(
        sssp
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        sssp
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/sssp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sssp.h)

target_link_libraries(
        sssp
        PUBLIC
        logsystem
        fmt)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET sssp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sssp> ${output_directory})