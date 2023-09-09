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
  set(SOURCE_FOLDER core)
  set(TESTS_FOLDER tests)
  set(EXECUTABLE_NAME graph-query)			# Define executable name for project

#  set(CMAKE_CXX_EXTENSIONS off)            # Ensures -std=c++xx is provided, rather than -std=g++xx
  set(CMAKE_CXX_STANDARD 20)                # Define the C++ standard
  message("-- C++ standard is set to -std=c++${CMAKE_CXX_STANDARD}")

# add submodules
  add_subdirectory(external/googletest)
  add_subdirectory(external/fmtlib)
  add_subdirectory(external/glfw)
  add_subdirectory(external/glew-cmake)
  add_subdirectory(external)

# source
  add_subdirectory(${SOURCE_FOLDER})

  option(PACKAGE_TESTS "Build the tests" ON)
  if(PACKAGE_TESTS)
    enable_testing()
    add_subdirectory(${TESTS_FOLDER})
  endif()

  message("-- ${PROJECT_NAME} [${PROJECT_VERSION}] build files has been defined")
else()
  message("-- ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt must be the root config and not added as a subdirectory")
endif()
