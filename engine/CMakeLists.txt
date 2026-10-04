cmake_minimum_required(VERSION 4.0)
project(engine)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)



add_library(engine
	source/font-manager/font-manager.c
	source/toolkit/toolkit.c
	source/types/containers.c
	source/unicode/unicode.c

	source/allocators/arena.c
	source/allocators/pool.c

	source/utils/shader.c
	source/utils/string.c

	source/platform/window.c
	source/platform/memory.c
)

add_library(roots::engine ALIAS engine)
target_compile_definitions(engine PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

target_include_directories(engine PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(engine PUBLIC
	stb
	glad
	glfw
	cglm
	harfbuzz-gpu
	harfbuzz

	$<$<BOOL:${WIN32}>:dwmapi>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(engine PRIVATE
		-Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
		-Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
		-Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
		-Wno-empty-translation-unit -fno-omit-frame-pointer -O0
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)

	target_link_options(engine PUBLIC
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/engine.pc.in ${CMAKE_CURRENT_BINARY_DIR}/engine.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/engine.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY include/engine DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
	TARGETS engine
	EXPORT engineConfig
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
	EXPORT engineConfig
	NAMESPACE roots::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/engine
)

enable_unit_testing(roots::engine)
