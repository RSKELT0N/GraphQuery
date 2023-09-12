# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

find_package(OpenGL REQUIRED)

# Setting project definitions.
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui/)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB
	SOURCES
	${SOURCE_DIR}/*.cpp
	${SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

# Add files for compilation.
add_library(imgui
		STATIC
		${SOURCES})

# Link the following librariies
target_link_libraries(imgui
		PUBLIC
		libglew_static
		glfw
		OpenGL::GL)

# Add program include directories
target_include_directories(imgui
		PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/imgui/
		${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
