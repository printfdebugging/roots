cmake_minimum_required(VERSION 4.0)
project(font-manager)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_library(font-manager source/font-manager.c)
add_library(module::font-manager ALIAS font-manager)
target_compile_definitions(font-manager PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

target_include_directories(font-manager PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(font-manager PUBLIC
   module::utils
   module::unicode

   harfbuzz-gpu
   harfbuzz
   glad
   cglm
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_compile_options(font-manager PRIVATE
      -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
      -Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
      -Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
      -Wno-empty-translation-unit -fno-omit-frame-pointer -O0
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )

   target_link_options(font-manager PUBLIC
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/font-manager.pc.in ${CMAKE_CURRENT_BINARY_DIR}/font-manager.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/font-manager.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(
   DIRECTORY include/font-manager
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
   TARGETS font-manager
   EXPORT fontManagerConfig
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
   EXPORT fontManagerConfig
   NAMESPACE module::
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/font-manager
)
