cmake_minimum_required(VERSION 3.10)

add_library(
        pr
        SHARED
        pagerank.cpp
        pagerank.h)

target_include_directories(
        pr
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_compile_options(
        pr
        PUBLIC
        -fPIC
        -O3
        -funroll-loops               # Unroll loops for better performance
        -ftree-vectorize             # Enable vectorization
        -pthread)

target_sources(
        pr
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/pagerank.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pagerank.h)

target_link_libraries(
        pr
        PUBLIC
        logsystem
        fmt)

if(OpenMP_CXX_FOUND)
    target_link_libraries(pr PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(${EXECUTABLE_NAME} PUBLIC ${OpenMP_CXX_FLAGS} -fopenmp-simd)
endif()

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET pr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pr> ${output_directory})