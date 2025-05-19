cmake_minimum_required(VERSION 3.10)

add_library(
        diskdriver
        STATIC)

target_include_directories(
        diskdriver
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_compile_options(
        diskdriver
        PUBLIC
        -fPIC
        -O3
        -funroll-loops               # Unroll loops for better performance
        -ftree-vectorize             # Enable vectorization
        -pthread
)

target_sources(
        diskdriver
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/diskdriver.cpp)

target_link_libraries(
        diskdriver
        PUBLIC
        logsystem
        fmt)
