cmake_minimum_required(VERSION 3.9)

add_library(dlg SHARED)

target_include_directories(
        dlg
        PUBLIC
        ${GRAPH_QUERY_ROOT_DIR}/)

target_sources(
        dlg
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/dlg.cpp)

target_link_libraries(dlg
        PRIVATE
        fmt
        imgui)

# Copy the shared library into the output/peripherals directory
set(output_directory ${PROJECT_SOURCE_DIR}/lib/models)
add_custom_command(
        TARGET dlg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_directory}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dlg> ${output_directory})