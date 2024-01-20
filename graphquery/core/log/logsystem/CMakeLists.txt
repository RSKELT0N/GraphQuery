cmake_minimum_required(VERSION 3.9)

add_library(
        logsystem
        STATIC)

target_include_directories(
        logsystem
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(
        logsystem
        PUBLIC
        -fPIC)

target_sources(
        logsystem
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/logsystem.cpp)

target_link_libraries(logsystem
        PUBLIC
        fmt)
