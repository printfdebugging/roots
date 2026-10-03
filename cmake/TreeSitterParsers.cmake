option(TREE_SITTER_REUSE_ALLOCATOR "Make grammar scanners use the tree-sitter library's allocator" OFF)

set(TREE_SITTER_PARSER_CLANG_TIDY "" CACHE STRING "clang-tidy command for the grammar libraries, empty for none")
set(TREE_SITTER_PARSER_DEFINE_SYMBOL "" CACHE STRING "Symbol defined while building a shared grammar library")

function(add_tree_sitter_parser PARSER_DIRECTORY)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "NAME" "")
	if(NOT ARG_NAME)
		get_filename_component(ARG_NAME "${PARSER_DIRECTORY}" NAME)
	endif()

	file(GLOB sources CONFIGURE_DEPENDS "${PARSER_DIRECTORY}/src/parser.c" "${PARSER_DIRECTORY}/src/scanner.c")
	add_library(${ARG_NAME} ${sources})

	target_include_directories(${ARG_NAME}
		PRIVATE "${PARSER_DIRECTORY}/src"
		PUBLIC "${PARSER_DIRECTORY}/bindings/c"
	)

	target_compile_definitions(${ARG_NAME} PRIVATE
		$<$<BOOL:${TREE_SITTER_REUSE_ALLOCATOR}>:TREE_SITTER_REUSE_ALLOCATOR>
		$<$<CONFIG:Debug>:TREE_SITTER_DEBUG>
	)

	set_target_properties(${ARG_NAME} PROPERTIES
		POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
		DEFINE_SYMBOL "${TREE_SITTER_PARSER_DEFINE_SYMBOL}"
		C_CLANG_TIDY "${TREE_SITTER_PARSER_CLANG_TIDY}"
	)

	install(
		TARGETS ${ARG_NAME}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)

	configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tree-sitter-parser.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.pc" @ONLY)
	string(REGEX REPLACE "^tree-sitter-" "" language "${ARG_NAME}")
	install(DIRECTORY "${PARSER_DIRECTORY}/queries/" DESTINATION ${CMAKE_INSTALL_DATADIR}/roots/queries/${language})
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
	install(DIRECTORY "${PARSER_DIRECTORY}/bindings/c/tree_sitter" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endfunction()
