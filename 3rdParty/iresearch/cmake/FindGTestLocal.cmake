# - Find GTest (CMakeLists.txt gtest.h gtest.cc libgtest.a libgtest_main.a)

# This module defines
#  GTEST_INCLUDE_DIR, directory containing headers
#  GTEST_SHARED_LIBS, path to libbfd*.so/libbfd*.lib or cmake target
#  GTEST_STATIC_LIBS, path to libbfd*.a/libbfd*.lib or cmake target
#  GTEST_SHARED_LIB_RESOURCES, shared libraries required to use GTest, i.e. libgtest.so/libgtest.dll
#  GTEST_FOUND, whether ftest has been found

if ("${GTEST_ROOT}" STREQUAL "")
  set(GTEST_ROOT "$ENV{GTEST_ROOT}")
  if (NOT "${GTEST_ROOT}" STREQUAL "")
    string(REPLACE "\"" "" GTEST_ROOT ${GTEST_ROOT})
  endif()
endif()

if (NOT "${GTEST_ROOT}" STREQUAL "")
  set(GTEST_SEARCH_HEADER_PATHS
    ${GTEST_ROOT}
    ${GTEST_ROOT}/include
    ${GTEST_ROOT}/include/gtest
  )

  set(GTEST_SEARCH_LIB_PATHS
    ${GTEST_ROOT}
    ${GTEST_ROOT}/lib
  )

  set(GTEST_SEARCH_SRC_PATHS
    ${GTEST_ROOT}
    ${GTEST_ROOT}/src
    ${GTEST_ROOT}/src/gtest
    ${GTEST_ROOT}/src/gtest/src
  )
elseif (NOT MSVC)
  set(GTEST_SEARCH_HEADER_PATHS
      "/usr/include"
      "/usr/include/gtest"
      "/usr/include/x86_64-linux-gnu"
      "/usr/include/x86_64-linux-gnu/gtest"
  )

  set(GTEST_SEARCH_LIB_PATHS
      "/lib"
      "/lib/x86_64-linux-gnu"
      "/usr/lib"
      "/usr/lib/x86_64-linux-gnu"
  )

  set(GTEST_SEARCH_SRC_PATHS
      "/usr/src"
      "/usr/src/gtest"
      "/usr/src/gtest/src"
  )
endif()

find_path(GTEST_INCLUDE_DIR
  gtest/gtest.h
  PATHS ${GTEST_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

find_path(GTEST_SRC_DIR_GTEST
  gtest.cc
  PATHS ${GTEST_SEARCH_SRC_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

if (GTEST_SRC_DIR_GTEST)
  get_filename_component(GTEST_SRC_DIR_PARENT ${GTEST_SRC_DIR_GTEST} DIRECTORY)

  find_path(GTEST_SRC_DIR_CMAKE
    CMakeLists.txt
    PATHS ${GTEST_SRC_DIR_GTEST} ${GTEST_SRC_DIR_PARENT}
    NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
  )
endif()

# found the cmake enabled source directory
if (GTEST_INCLUDE_DIR AND GTEST_SRC_DIR_GTEST AND GTEST_SRC_DIR_CMAKE)
  set(GTEST_FOUND TRUE)

  add_subdirectory(
    ${GTEST_SRC_DIR_CMAKE}
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-gtest.dir
  )

  # prepend source directory to files since can't switch to source directory
  # during add_library(...)
  foreach(ELEMENT "$<TARGET_PROPERTY:gtest,SOURCES>" "$<TARGET_PROPERTY:gtest_main,SOURCES>")
    list(APPEND IResearch_gtest_source "${GTEST_SRC_DIR_CMAKE}/${ELEMENT}")
  endforeach()

  add_library(${IResearch_TARGET_NAME}-gtest-shared
    SHARED
    ${IResearch_gtest_source}
  )

  target_include_directories(${IResearch_TARGET_NAME}-gtest-shared
    PRIVATE ${GTEST_INCLUDE_DIR}
    PRIVATE ${GTEST_SRC_DIR_CMAKE}
  )

  set_target_properties(${IResearch_TARGET_NAME}-gtest-shared
    PROPERTIES
    OUTPUT_NAME libgtest
    DEBUG_POSTFIX d
  )

  set(GTEST_LIBRARIES gtest)
  set(GTEST_MAIN_LIBRARIES gtest_main)
  set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
  set(GTEST_SHARED_LIBS ${IResearch_TARGET_NAME}-gtest-shared)
  set(GTEST_STATIC_LIBS ${GTEST_BOTH_LIBRARIES})
  set(GTEST_SHARED_LIB_RESOURCES
      $<TARGET_FILE:${IResearch_TARGET_NAME}-gtest-shared>
      $<TARGET_PDB_FILE:${IResearch_TARGET_NAME}-gtest-shared>
  )

  return()
endif()

if ("${GTEST_ROOT}" STREQUAL ""
    AND NOT MSVC
    AND EXISTS "/usr/include/gtest/gtest.h"
)
  set(GTEST_ROOT "/usr")
endif()

find_package(GTest
  REQUIRED
)

# shared libraries not built for GTest by default, so nothing to set
set(GTEST_STATIC_LIBS ${GTEST_BOTH_LIBRARIES})
