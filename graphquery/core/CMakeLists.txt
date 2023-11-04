# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.9)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB_RECURSE
        SOURCES
        main.cpp
        db/*.cpp
        log/*.cpp
        interact/*.cpp
        db/utils/ring_buffer.h)

# Set source files to compile for generating executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_compile_definitions(${EXECUTABLE_NAME}
        PUBLIC
        PROJECT_NAME="${PROJECT_NAME}")

target_compile_options(${EXECUTABLE_NAME}
        PUBLIC
        -Wall
        -Werror
        -Wpedantic
        -O2
        -g)

target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE
        fmt
        imgui
        dylib)

# Add program include directories
target_include_directories(${EXECUTABLE_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

