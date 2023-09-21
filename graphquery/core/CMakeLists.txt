# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB_RECURSE
        SOURCES
        main.cpp
        *.cpp)

# Set source files to compile for generating executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_compile_options(${EXECUTABLE_NAME}
        PUBLIC
        -Wall
        -Werror
        -Wpedantic)

target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
        fmt
        imgui)

# Add program include directories
target_include_directories(${EXECUTABLE_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})