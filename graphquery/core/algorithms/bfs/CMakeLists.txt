cmake_minimum_required(VERSION 3.10)

add_library(
        bfs
        SHARED
        bfs.cpp
        bfs.h)

target_compile_options(
        bfs
        PUBLIC
        -fPIC
        -O3
        -funroll-loops               # Unroll loops for better performance
        -ftree-vectorize             # Enable vectorization
        -pthread)

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

if(OpenMP_CXX_FOUND)
    target_link_libraries(bfs PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(${EXECUTABLE_NAME} PUBLIC ${OpenMP_CXX_FLAGS} -fopenmp-simd)
endif()

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET bfs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bfs> ${output_directory})
