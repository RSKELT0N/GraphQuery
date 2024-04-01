cmake_minimum_required(VERSION 3.9)

add_library(
        scc
        SHARED
        scc.cpp
        scc.h)

target_include_directories(
        scc
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_compile_options(
        scc
        PUBLIC
        -fPIC
        -O3
        -ffast-math
        -ffinite-math-only
        -funroll-loops               # Unroll loops for better performance
        -ftree-vectorize             # Enable vectorization
        -pthread)

target_sources(
        scc
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/scc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/scc.h)

target_link_libraries(
        scc
        PUBLIC
        logsystem
        fmt)

if(OpenMP_CXX_FOUND)
    target_link_libraries(scc PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(${EXECUTABLE_NAME} PUBLIC ${OpenMP_CXX_FLAGS} -fopenmp-simd)
endif()

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET scc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:scc> ${output_directory})