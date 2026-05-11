target_sources(arango_aql PRIVATE
  Parser.cpp
  grammar.cpp
  tokens.cpp)

################################################################################
## BISON/FLEX
################################################################################

#note that we check - in the generated FLEX / BISON files, therefore they are
#generate inside the source tree

if (USE_MAINTAINER_MODE AND NOT MSVC)
  set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM "On")

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tokens.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${FLEX_EXECUTABLE} -L -otokens.cpp tokens.ll
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/tokens.ll
    VERBATIM
  )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/grammar.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_SOURCE_DIR}/utils/bison-c.sh
      ${BISON_EXECUTABLE} grammar.cpp grammar.y
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/grammar.y
    VERBATIM
  )
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(tokens.cpp
    DIRECTORY ..
    PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set_source_files_properties(grammar.cpp
    DIRECTORY ..
    PROPERTIES COMPILE_FLAGS -Wno-unused-but-set-variable)
endif ()

