target_sources(arango_aql PRIVATE
  Parser.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp)

################################################################################
## BISON/FLEX
################################################################################

# Generate the AQL lexer and parser into the build tree. flex and bison are
# required unconditionally; see the top-level CMakeLists.txt for the
# find_package calls.

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${FLEX_EXECUTABLE} -L
    -o ${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tokens.ll
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/tokens.ll
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/grammar.hpp
  VERBATIM
)

add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/grammar.hpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_SOURCE_DIR}/utils/bison-c.sh
    ${BISON_EXECUTABLE}
    ${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grammar.y
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/grammar.y
  VERBATIM
)

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
