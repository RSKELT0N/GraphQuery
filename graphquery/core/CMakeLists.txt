# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

add_subdirectory(logsystem)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB_RECURSE
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/core/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/src/*.cpp)

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
        imgui
        logsystem)

# Add program include directories
target_include_directories(${EXECUTABLE_NAME}
        PUBLIC
        core/include
        logsystem)