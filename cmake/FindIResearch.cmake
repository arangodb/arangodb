# ArangoDB-IResearch integration
#
# Copyright (c) 2017 by EMC Corporation, All Rights Reserved
#
# This software contains the intellectual property of EMC Corporation or is licensed to
# EMC Corporation from third parties. Use of this software and the intellectual property
# contained therein is expressly limited to the terms and conditions of the License
# Agreement under which it is provided by or on behalf of EMC.

# - Find iResearch (iresearch.dll core/*/*.hpp)
# This module defines
#  IRESEARCH_BUILD_DIR, directory to the iresearch build path
#  IRESEARCH_CXX_FLAGS, flags to use with CXX compilers
#  IRESEARCH_FOUND, weather iResearch has been found (for find_package(...))
#  IRESEARCH_INCLUDE, directory containing headers
#  IRESEARCH_ROOT, root directory of iResearch

if ("${IRESEARCH_ROOT}" STREQUAL "")
  set(IRESEARCH_ROOT "$ENV{IRESEARCH_ROOT}")
  if ("${IRESEARCH_ROOT}" STREQUAL "")
    set(IRESEARCH_ROOT "${CMAKE_SOURCE_DIR}/3rdParty/iresearch")
  endif()
  if (NOT "${IRESEARCH_ROOT}" STREQUAL "")
    string(REPLACE "\"" "" IRESEARCH_ROOT ${IRESEARCH_ROOT})
  endif()
endif()

if (NOT "${IRESEARCH_ROOT}" STREQUAL "")
  string(REPLACE "\\" "/" IRESEARCH_ROOT ${IRESEARCH_ROOT})
endif()

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/3rdParty/iresearch/README.md" AND NOT EXISTS "${IRESEARCH_ROOT}/README.md")
  execute_process(
    COMMAND git submodule update --init --remote --recursive -- "3rdParty/iresearch"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

set(IRESEARCH_BUILD_DIR "${CMAKE_BINARY_DIR}/3rdParty/iresearch")
set(IRESEARCH_CXX_FLAGS " ") # has to be a non-empty string
#set(IRESEARCH_CXX_FLAGS "-DIRESEARCH_DLL") Arango now links statically against iResearch

if (MSVC)
  # do not use min/max '#define' from Microsoft since iResearch declares methods
  set(IRESEARCH_CXX_FLAGS ${IRESEARCH_CXX_FLAGS} "-DNOMINMAX")
endif ()

set(IRESEARCH_FOUND TRUE) # always found since using git submodule as fallback

unset(IRESEARCH_INCLUDE)
list(APPEND IRESEARCH_INCLUDE
  "${IRESEARCH_ROOT}/core"
  "${IRESEARCH_ROOT}/external"
  "${IRESEARCH_BUILD_DIR}/core"
)


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IRESEARCH
  DEFAULT_MSG
  IRESEARCH_BUILD_DIR
  IRESEARCH_CXX_FLAGS
  IRESEARCH_INCLUDE
  IRESEARCH_ROOT
)
message("IRESEARCH_BUILD_DIR: " ${IRESEARCH_BUILD_DIR})
message("IRESEARCH_INCLUDE: " ${IRESEARCH_INCLUDE})
message("IRESEARCH_ROOT: " ${IRESEARCH_ROOT})

mark_as_advanced(
  IRESEARCH_BUILD_DIR
  IRESEARCH_CXX_FLAGS
  IRESEARCH_INCLUDE
  IRESEARCH_ROOT
)
