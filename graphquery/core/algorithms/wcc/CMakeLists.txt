cmake_minimum_required(VERSION 3.9)

add_library(
        cc
        SHARED
        wcc.cpp
        wcc.h)

target_include_directories(
        cc
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_compile_options(
        cc
        PUBLIC
        -fPIC
        -O3
        -ffast-math
        -ffinite-math-only
        -funroll-loops               # Unroll loops for better performance
        -ftree-vectorize             # Enable vectorization
        -pthread)

target_sources(
        cc
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/wcc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/wcc.h)

target_link_libraries(
        cc
        PUBLIC
        logsystem
        fmt)

if(OpenMP_CXX_FOUND)
    target_link_libraries(cc PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(cc PUBLIC ${OpenMP_CXX_FLAGS})
endif()

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET cc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cc> ${output_directory})