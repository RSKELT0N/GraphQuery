# CMake version specified
cmake_minimum_required(VERSION 3.9)

# Project declaration of base project.
project(GraphQuery
        VERSION 0.1
        DESCRIPTION "A graph database to represent vertices and edges."
        LANGUAGES C CXX)

# Only perform if this is the root project
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

  set(PROJECT graphquery)

  set(CMAKE_CXX_STANDARD 20)                          # Define the C++ standard
  message("-- C++ standard is set to -std=c++${CMAKE_CXX_STANDARD}")

  set(EXECUTABLE_NAME graph-query)			              # Define executable name for project
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# add submodules
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/external/googletest)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/external/fmt)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/external/glfw)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/external/glew)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/external/dylib)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/external)          # imgui

# source
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/core/db/storage/diskdriver)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/core/log/logsystem)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/core/models)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/core)

  if(CMAKE_TESTING_ENABLED)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}/tests)
  endif()

  message("-- ${PROJECT_NAME} [${PROJECT_VERSION}] build files has been defined")
else()
  message("-- ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt must be the root config and not added as a subdirectory")
endif()
