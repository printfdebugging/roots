# source: https://github.com/nitrix/nui/blob/master/CMakeLists.txt
function(enable_unit_testing LIB)
	enable_testing()

	file(GLOB testfiles_fullpath "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")
	SET(testfiles_relative)

	foreach(name ${testfiles_fullpath})
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/tests/" "" name ${name})
		SET(testfiles_relative ${testfiles_relative} ${name})
	endforeach()

	create_test_sourcelist(testlist test_runner.c ${testfiles_relative})
	add_executable(test_runner test_runner.c ${testfiles_fullpath})
	target_link_libraries(test_runner ${LIB})

	foreach(name ${testfiles_relative})
		string(REPLACE ".c" "" name ${name})
		add_test(NAME ${name} COMMAND test_runner ${name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
	endforeach()
endfunction()
