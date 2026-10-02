cmake_minimum_required(VERSION 4.0)
project(platform)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_library(platform
   source/window.c
)

add_library(module::platform ALIAS platform)
target_compile_definitions(platform PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

target_link_libraries(platform PUBLIC
   stb
   glad
   glfw
   module::utils
)

target_include_directories(platform PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   target_compile_options(platform PRIVATE
      -Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
      -Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
      -Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
      -Wno-empty-translation-unit -fno-omit-frame-pointer -O0
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )

   target_link_options(platform PUBLIC
      $<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
   )
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/platform.pc.in ${CMAKE_CURRENT_BINARY_DIR}/platform.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/platform.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(
   DIRECTORY
      include/platform
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
   TARGETS platform
   EXPORT platformConfig
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
   EXPORT platformConfig
   NAMESPACE module::
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/platform
)
