# CMake version specified
cmake_minimum_required(VERSION 3.17)

# Project declaration of base project.
project(
        GraphQuery
        VERSION 0.1
		    DESCRIPTION "A graph database to represent vertices and edges."
        LANGUAGES C CXX)

# Only perform if this is the root project
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(PROJECT_ROOT graphquery)
  set(SOURCE_FOLDER ${PROJECT_ROOT}/core)
  set(TESTS_FOLDER ${PROJECT_ROOT}/tests)

  set(CMAKE_CXX_STANDARD 20)                # Define the C++ standard
  message("-- C++ standard is set to -std=c++${CMAKE_CXX_STANDARD}")

  set(EXECUTABLE_NAME graph-query)			# Define executable name for project

# add submodules
  add_subdirectory(${PROJECT_ROOT}/external/googletest)
  add_subdirectory(${PROJECT_ROOT}/external/fmtlib)
  add_subdirectory(${PROJECT_ROOT}/external/glfw)
  add_subdirectory(${PROJECT_ROOT}/external/glew-cmake)
  add_subdirectory(${PROJECT_ROOT}/external)

# source
  add_subdirectory(${SOURCE_FOLDER})

  if(CMAKE_TESTING_ENABLED)
    enable_testing()
    add_subdirectory(${TESTS_FOLDER})
  endif()

  message("-- ${PROJECT_NAME} [${PROJECT_VERSION}] build files has been defined")
else()
  message("-- ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt must be the root config and not added as a subdirectory")
endif()
