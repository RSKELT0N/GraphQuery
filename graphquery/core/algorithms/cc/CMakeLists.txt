cmake_minimum_required(VERSION 3.9)

add_library(
        cc
        SHARED
        connected_components.cpp
        connected_components.h)

target_include_directories(
        cc
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        cc
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/connected_components.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connected_components.h)

target_link_libraries(
        cc
        PUBLIC
        logsystem
        fmt)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET cc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cc> ${output_directory})