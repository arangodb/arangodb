target_sources(arango_aql PRIVATE
  Parser.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp)

# The AQL lexer and parser are generated into the build tree. The
# add_custom_command() calls live in ../CMakeLists.txt — they must be in the
# same directory scope as the consuming target arango_aql, otherwise CMake's
# Ninja generator emits phantom build edges with no COMMAND attached.

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp
    DIRECTORY ..
    PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp
    DIRECTORY ..
    PROPERTIES COMPILE_FLAGS -Wno-unused-but-set-variable)
endif ()
