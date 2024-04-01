cmake_minimum_required(VERSION 3.9)

add_library(
        wcc
        SHARED
        wcc.cpp
        wcc.h)

target_include_directories(
        wcc
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_compile_options(
        wcc
        PUBLIC
        -fPIC
        -O3
        -ffast-math
        -ffinite-math-only
        -funroll-loops               # Unroll loops for better performance
        -ftree-vectorize             # Enable vectorization
        -pthread)

target_sources(
        wcc
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/wcc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/wcc.h)

target_link_libraries(
        wcc
        PUBLIC
        logsystem
        fmt)

if(OpenMP_CXX_FOUND)
    target_link_libraries(wcc PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(${EXECUTABLE_NAME} PUBLIC ${OpenMP_CXX_FLAGS} -fopenmp-simd)
endif()

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/algorithms)
add_custom_command(
        TARGET wcc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:wcc> ${output_directory})