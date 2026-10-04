cmake_minimum_required(VERSION 4.0)
project(utils)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_library(utils
	source/string.c
	source/shader.c
	source/allocators/arena.c
	source/allocators/pool.c
)

add_library(module::utils ALIAS utils)
target_compile_definitions(utils PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
target_link_libraries(utils PUBLIC glad)

target_include_directories(utils PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(utils PRIVATE
		-Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
		-Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
		-Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
		-Wno-empty-translation-unit -fno-omit-frame-pointer -O0
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)

	target_link_options(utils PUBLIC
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/utils.pc.in ${CMAKE_CURRENT_BINARY_DIR}/utils.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/utils.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY include/utils DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
	TARGETS utils
	EXPORT utilsConfig
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
	EXPORT utilsConfig
	NAMESPACE module::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/utils
)
