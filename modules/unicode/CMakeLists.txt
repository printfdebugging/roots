cmake_minimum_required(VERSION 4.0)
project(unicode
   VERSION         "1.0"
   DESCRIPTION     "A Unicode/UTF-8 processing library"
   HOMEPAGE_URL    "https://github.com/printfdebugging/unicode"
   LANGUAGES       "C"
)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_library(unicode source/unicode.c)
add_library(module::unicode ALIAS unicode)
target_compile_options(unicode PRIVATE -fPIC)

target_include_directories(unicode PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
   DIRECTORY include/unicode
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unicode.pc.in ${CMAKE_CURRENT_BINARY_DIR}/unicode.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/unicode.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(
   TARGETS unicode
   EXPORT unicodeConfig
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
   EXPORT unicodeConfig
   NAMESPACE module::
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/unicode
)

enable_unit_testing(unicode)
