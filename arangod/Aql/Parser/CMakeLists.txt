target_sources(arango_aql PRIVATE
  Parser.cpp
  ${BISON_AqlGrammar_OUTPUT_SOURCE}
  ${FLEX_AqlTokens_OUTPUTS})

# The AQL lexer and parser are generated into the build tree. The
# BISON_TARGET / FLEX_TARGET calls live in ../CMakeLists.txt — they must be in
# the same directory scope as the consuming target arango_aql, otherwise
# CMake's Ninja generator emits phantom build edges with no COMMAND attached.

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(${FLEX_AqlTokens_OUTPUTS}
    DIRECTORY ..
    PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set_source_files_properties(${BISON_AqlGrammar_OUTPUT_SOURCE}
    DIRECTORY ..
    PROPERTIES COMPILE_FLAGS -Wno-unused-but-set-variable)
endif ()
