cmake_minimum_required(VERSION 4.0)
project(editor)

include(GNUInstallDirs)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/harfbuzz)
add_subdirectory(vendor/unicode)
add_subdirectory(vendor/glad)
add_subdirectory(vendor/cglm)
add_subdirectory(vendor/stb)
add_subdirectory(vendor/lua)

set(CMAKE_C_STANDARD 23)
set(SOURCES main.c editor.c shader.c filesystem.c text.c renderer.c utils.c sanitizers.c)
list(TRANSFORM SOURCES PREPEND source/)
add_executable(editor "${SOURCES}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_compile_options(editor PRIVATE
      -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
      -Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
      -Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
      -Wno-empty-translation-unit -fno-omit-frame-pointer -O0
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )

   target_link_options(editor PUBLIC
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )

   set(CLANGD_COMPLETION_HEADER_INSERTION Never)
   set(CLANGD_DIAGNOSTICS_UNUSED_INCLUDES None)
   set(CLANGD_COMPILE_COMMANDS_FILE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

   configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/clangd.in
      ${CMAKE_CURRENT_SOURCE_DIR}/.clangd @ONLY
   )
endif()


target_compile_definitions(editor PRIVATE
   GLFW_INCLUDE_NONE
   TEXT_LINE_IMPLEMENTATION
   ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
   "$<$<CONFIG:Debug>:DEBUG>"
)

target_include_directories(editor PUBLIC
   include
)

target_link_libraries(editor PRIVATE
   harfbuzz-gpu harfbuzz glfw glad cglm
   stb lua::lua unicode
   $<$<BOOL:${WIN32}>:dwmapi>
)

install(
   DIRECTORY assets/
   DESTINATION ${CMAKE_INSTALL_DATADIR}/roots/assets
   COMPONENT dist
)

install(
   TARGETS editor
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT dist
)
