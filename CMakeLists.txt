cmake_minimum_required(VERSION 4.0)
project(editor)

include(GNUInstallDirs)
set(BUILD_SHARED_LIBS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
   set(CMAKE_C_COMPILER_LAUNCHER "ccache")
   set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")
endif()

add_subdirectory(glfw)
add_subdirectory(harfbuzz)
add_subdirectory(unicode)
add_subdirectory(glad)
add_subdirectory(cglm)

set(CMAKE_C_STANDARD 23)
add_compile_options(-std=gnu2x)

add_executable(editor
   source/editor.c
   source/shader.c
   source/filesystem.c
   source/text.c
   source/layout.c
   source/renderer.c
   source/window.c
   source/fontmanager.c
   source/utils.c
)

if (TEXT_LINE_IMPLEMENTATION)
   add_definitions(-DTEXT_LINE_IMPLEMENTATION=1)
endif()

target_compile_definitions(editor PRIVATE GLFW_INCLUDE_NONE ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
target_include_directories(editor PUBLIC include)
target_link_libraries(editor PRIVATE harfbuzz-gpu harfbuzz glfw glad cglm unicode)

install(
   TARGETS editor
   EXPORT  editor
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
