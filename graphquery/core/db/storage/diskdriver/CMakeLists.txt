cmake_minimum_required(VERSION 3.9)

add_library(diskdriver STATIC)

target_include_directories(
        diskdriver
        PUBLIC
        ${PROJECT_SOURCE_DIR}/graphquery/core)

target_sources(
        diskdriver
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/diskdriver.cpp)

target_link_libraries(
        diskdriver
        PUBLIC
        logsystem
        fmt)