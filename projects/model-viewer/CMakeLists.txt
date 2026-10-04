cmake_minimum_required(VERSION 4.0)
project(model-viewer)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 23)

add_executable(model-viewer
	source/main.c
	source/viewer.c
	source/sanitizers.c
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(model-viewer PRIVATE
		-Wall -Wextra -Wundef -Wshadow -Wpointer-arith -Wcast-align
		-Wstrict-prototypes -Wwrite-strings -Wswitch-enum -Wconversion -Wunreachable-code
		-Wpedantic -Wno-override-init -Werror=return-type -Wno-double-promotion
		-Wno-empty-translation-unit -fno-omit-frame-pointer -O0
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)

	target_link_options(model-viewer PUBLIC
		$<$<NOT:$<PLATFORM_ID:Windows>>:-fsanitize=undefined,address>
	)
endif()


target_compile_definitions(model-viewer PRIVATE
	GLFW_INCLUDE_NONE
	TEXT_LINE_IMPLEMENTATION
	ASSETS_DIR="${CMAKE_CURRENT_LIST_DIR}/assets/"
	SOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}/"
	"$<$<CONFIG:Debug>:DEBUG>"
)

target_include_directories(model-viewer PUBLIC include)

target_link_libraries(model-viewer PRIVATE
	module::platform
)

install(
	DIRECTORY assets/
	DESTINATION ${CMAKE_INSTALL_DATADIR}/model-viewer
)

install(
	TARGETS model-viewer
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
