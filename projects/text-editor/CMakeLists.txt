cmake_minimum_required(VERSION 4.0)
project(text-editor)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_executable(text-editor
   source/main.c
   source/editor.c
   source/text.c
   source/renderer.c
   source/sanitizers.c
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_compile_options(text-editor PRIVATE
      -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
      -Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
      -Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
      -Wno-empty-translation-unit -fno-omit-frame-pointer -O0
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )

   target_link_options(text-editor PUBLIC
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )
endif()


target_compile_definitions(text-editor PRIVATE
   GLFW_INCLUDE_NONE
   TEXT_LINE_IMPLEMENTATION
   ASSETS_DIR="${CMAKE_CURRENT_LIST_DIR}/assets/"
   SOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}/"
   "$<$<CONFIG:Debug>:DEBUG>"
)

target_include_directories(text-editor PUBLIC include)

target_link_libraries(text-editor PRIVATE
   module::utils
   module::font-manager
   module::unicode
   module::platform

   glfw
   glad
   cglm
   stb
   lua::lua
   tree-sitter
   tree-sitter-c
   tree-sitter-lua
   libuv::libuv
   $<$<BOOL:${WIN32}>:dwmapi>
)

install(
   DIRECTORY assets/
   DESTINATION ${CMAKE_INSTALL_DATADIR}/roots
   COMPONENT dist
)

install(
   TARGETS text-editor
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT dist
)
