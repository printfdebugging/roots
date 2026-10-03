cmake_minimum_required(VERSION 4.0)
project(toolkit)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_library(toolkit
	source/toolkit.c
)

add_library(module::toolkit ALIAS toolkit)
target_compile_definitions(toolkit PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

target_link_libraries(toolkit PUBLIC
	module::font-manager
)

target_include_directories(toolkit PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(toolkit PRIVATE
		-Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
		-Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
		-Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
		-Wno-empty-translation-unit -fno-omit-frame-pointer -O0
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)

	target_link_options(toolkit PUBLIC
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/toolkit.pc.in ${CMAKE_CURRENT_BINARY_DIR}/toolkit.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/toolkit.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY include/toolkit DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
	TARGETS toolkit
	EXPORT toolkitConfig
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
	EXPORT toolkitConfig
	NAMESPACE module::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/toolkit
)
