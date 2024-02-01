# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.9)

add_compile_definitions(PROJECT_ROOT="${PROJECT_SOURCE_DIR}")

# Retrieve source files, under SOURCE_FOLDER
file(GLOB_RECURSE
        SOURCES
        main.cpp
        db/*.cpp
        db/*.h
        db/*.hpp
        interact/*.cpp
        log/loggers/*.cpp)

# Set source files to compile for generating executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_compile_definitions(${EXECUTABLE_NAME}
        PUBLIC
        PROJECT_NAME="${PROJECT_NAME}")

target_compile_options(
        ${EXECUTABLE_NAME}
        PUBLIC
        -Wall
        -Werror
        -Wpedantic
        -Wextra
        -Wshadow
        -pthread
        -O3)

target_link_libraries(${EXECUTABLE_NAME}
        PUBLIC
        diskdriver
        imgui
        dylib
        logsystem)

# Add program include directories
target_include_directories(${EXECUTABLE_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

