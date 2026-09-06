cmake_minimum_required(VERSION 4.0)
project(editor)

include(GNUInstallDirs)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CGLM_STATIC ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
   set(CMAKE_C_COMPILER_LAUNCHER "ccache")
   set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")
endif()

add_subdirectory(libs/glfw)
add_subdirectory(libs/harfbuzz)
add_subdirectory(libs/unicode)
add_subdirectory(libs/glad)
add_subdirectory(libs/cglm)
add_subdirectory(libs/stb)

set(CMAKE_C_STANDARD 23)
add_compile_options(-std=gnu2x)

add_executable(editor
   source/main.c
   source/editor.c
   source/shader.c
   source/filesystem.c
   source/text.c
   source/renderer.c
   source/window.c
   source/buffer.c
   source/fontmanager.c
   source/utils.c
   source/sanitizers.c
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_compile_options(editor PRIVATE
      -Wall
      -Wextra
      -Wundef
      -Wshadow
      -Wpointer-arith
      -Wcast-align
      -Wstrict-prototypes
      -Wwrite-strings
      -Wswitch-enum
      -Wconversion
      -Wunreachable-code
      -Wpedantic
      -Wno-override-init
      -Werror=return-type
      -Wno-double-promotion
      -fno-omit-frame-pointer
      -O0
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )

   target_link_options(editor PUBLIC
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )
endif()

add_definitions(
   -DTEXT_LINE_IMPLEMENTATION=1
)

target_compile_definitions(editor PRIVATE
   GLFW_INCLUDE_NONE
   ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
)

target_include_directories(editor PUBLIC
   include
)

target_link_libraries(editor PRIVATE
   harfbuzz-gpu
   harfbuzz
   glfw
   glad
   cglm
   stb
   unicode
   $<$<BOOL:${WIN32}>:dwmapi>
)

install(
   TARGETS editor
   EXPORT  editor
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
