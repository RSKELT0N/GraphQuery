cmake_minimum_required(VERSION 3.9)

include(GNUInstallDirs)

add_library(dlg
        SHARED
        dlg.cpp)

target_include_directories(dlg
        PUBLIC
        ${GRAPH_QUERY_ROOT_DIR}/)

set_target_properties(dlg
        PROPERTIES
        VERSION ${PROJECT_VERSION})

set_target_properties(dlg
        PROPERTIES
        SOVERSION ${PROJECT_VERSION_MAJOR})

set_target_properties(dlg
        PROPERTIES
        PUBLIC_HEADER dlg.h)

install(TARGETS dlg
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})