# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Add files for compilation.
add_library(logsystem
        SHARED
        ${SOURCES})

target_link_libraries(logsystem
        PUBLIC
        fmt)

# Add program include directories
target_include_directories(logsystem
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/)
