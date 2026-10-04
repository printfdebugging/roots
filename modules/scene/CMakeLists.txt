cmake_minimum_required(VERSION 4.0)
project(scene)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_library(scene
	source/model.c
)

add_library(module::scene ALIAS scene)
target_compile_definitions(scene PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

target_link_libraries(scene PUBLIC
	vendor::cgltf
)

target_include_directories(scene PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(scene PRIVATE
		-Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
		-Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
		-Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
		-Wno-empty-translation-unit -fno-omit-frame-pointer -O0
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)

	target_link_options(scene PUBLIC
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scene.pc.in ${CMAKE_CURRENT_BINARY_DIR}/scene.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/scene.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY include/scene DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
	TARGETS scene
	EXPORT sceneConfig
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
	EXPORT sceneConfig
	NAMESPACE module::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/scene
)
