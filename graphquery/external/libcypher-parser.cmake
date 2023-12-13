# cmake sub-directory for src specification
cmake_minimum_required(VERSION 3.17)

find_package(OpenGL REQUIRED)

# Retrieve source files, under SOURCE_FOLDER
file(GLOB
	SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp/imgui_stdlib.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imnodes/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imgui-filebrowser/imfilebrowser.h)

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
		${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/
		${CMAKE_CURRENT_SOURCE_DIR}/imnode/
		${CMAKE_CURRENT_SOURCE_DIR}/imnodes/
		${CMAKE_CURRENT_SOURCE_DIR}/imgui-filebrowser/
		${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp/)
