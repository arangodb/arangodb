# - Find GTest (CMakeLists.txt gtest.h gtest.cc libgtest.a libgtest_main.a)

# This module defines
#  GTEST_INCLUDE_DIR, directory containing headers
#  GTEST_STATIC_LIBS, path to libbfd*.a/libbfd*.lib or cmake target
#  GTEST_FOUND, whether ftest has been found
if (GTEST_FOUND)
return()
endif()

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
    EXCLUDE_FROM_ALL # do not build unused targets
  )

  if (MSVC)
    # when compiling or linking against GTEST on MSVC2017 the following
    # definition is required: /D _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING

    target_compile_options(gtest
      PRIVATE "/D _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING"
    )

    target_compile_options(gtest_main
      PRIVATE "/D _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING"
    )
  endif()

  set(GTEST_LIBRARIES gtest)
  set(GTEST_MAIN_LIBRARIES gtest_main)
  set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})  
  set(GTEST_STATIC_LIBS ${GTEST_BOTH_LIBRARIES})  

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

set(GTEST_STATIC_LIBS ${GTEST_BOTH_LIBRARIES})
