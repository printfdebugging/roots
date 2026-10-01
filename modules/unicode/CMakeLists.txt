cmake_minimum_required(VERSION 4.0)
project(unicode
   VERSION         "1.0"
   DESCRIPTION     "A Unicode/UTF-8 processing library"
   HOMEPAGE_URL    "https://github.com/printfdebugging/unicode"
   LANGUAGES       "C"
)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(UNICODE_LIB_NAME unicode)
add_compile_options(-std=gnu2x)

add_library(unicode source/unicode.c)
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
   NAMESPACE unicode::
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/unicode
)

# note: this should be a funcion shared by all the modules :)
# source: https://github.com/nitrix/nui/blob/master/CMakeLists.txt
enable_testing()

file(GLOB testfiles_fullpath "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")
SET(testfiles_relative)

foreach(name ${testfiles_fullpath})
   string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/tests/" "" name ${name})
   SET(testfiles_relative ${testfiles_relative} ${name})
endforeach()

create_test_sourcelist(testlist test_runner.c ${testfiles_relative})
add_executable(test_runner test_runner.c ${testfiles_fullpath})
target_link_libraries(test_runner unicode)

foreach(name ${testfiles_relative})
   string(REPLACE ".c" "" name ${name})
   add_test(NAME ${name} COMMAND test_runner ${name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endforeach()
