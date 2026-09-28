find_program(CARGO cargo REQUIRED)

set(TREE_SITTER_CLI_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
set(TREE_SITTER_CLI_BINARY "${CMAKE_BINARY_DIR}/bin/tree-sitter${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_target(tree-sitter-cli
   COMMAND "${CMAKE_COMMAND}" -E
                env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
                "${CARGO}" build 
                    --release 
                    --locked 
                    --manifest-path "${CMAKE_SOURCE_DIR}/vendor/tree-sitter/Cargo.toml" 
                    --package tree-sitter-cli 
                    --target-dir "${TREE_SITTER_CLI_TARGET_DIR}"

   COMMAND "${CMAKE_COMMAND}" -E 
                make_directory "${CMAKE_BINARY_DIR}/bin"

   COMMAND "${CMAKE_COMMAND}" -E 
                copy_if_different
                    "${TREE_SITTER_CLI_TARGET_DIR}/release/tree-sitter${CMAKE_EXECUTABLE_SUFFIX}"
                    "${TREE_SITTER_CLI_BINARY}"

   BYPRODUCTS "${TREE_SITTER_CLI_BINARY}"
   COMMENT "Building the tree-sitter command line tool with cargo"
   USES_TERMINAL
   VERBATIM
)
