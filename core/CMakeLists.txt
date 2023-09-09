# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

# Setting project definitions.
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB SOURCES ${SOURCE_DIR}/*.cpp)

# Set source files to compile for generating executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})
target_compile_options(${EXECUTABLE_NAME} PUBLIC -Wall -Werror -Wpedantic -g)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE fmt
                                                 imgui)								 
# Add program include directories
target_include_directories(${EXECUTABLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)