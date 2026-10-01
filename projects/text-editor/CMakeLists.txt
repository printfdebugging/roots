
project(text-editor)

include(GNUInstallDirs)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CGLM_STATIC ON)


set(CMAKE_C_STANDARD 23)
set(SOURCES main.c editor.c shader.c filesystem.c text.c renderer.c utils.c sanitizers.c)
list(TRANSFORM SOURCES PREPEND source/)
add_executable(text-editor "${SOURCES}")

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
   harfbuzz-gpu
   harfbuzz
   glfw
   glad
   cglm
   stb
   lua::lua
   unicode
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
