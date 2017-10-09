# - Find Iconv (iconv.h, libiconv.a, libiconv.so)
# This module defines
#  ICONV_INCLUDE_DIR, directory containing headers
#  ICONV_LIBRARY_DIR, directory containing iconv libraries
#  ICONV_SHARED_LIB, path to libiconv.so/libiconv.dll
#  ICONV_STATIC_LIB, path to libiconv.lib
#  ICONV_FOUND, whether iconv has been found

if ("${ICONV_ROOT}" STREQUAL "")
  set(ICONV_ROOT "$ENV{ICONV_ROOT}")
  if (NOT "${ICONV_ROOT}" STREQUAL "")
    string(REPLACE "\"" "" ICONV_ROOT ${ICONV_ROOT})
  endif()
endif()

if (NOT "${ICONV_ROOT}" STREQUAL "")
  set(ICONV_SEARCH_HEADER_PATHS
    ${ICONV_ROOT}
    ${ICONV_ROOT}/include
    ${ICONV_ROOT}/include/iconv
  )

  set(ICONV_SEARCH_LIB_PATHS
    ${ICONV_ROOT}
    ${ICONV_ROOT}/lib
    ${ICONV_ROOT}/lib/.libs
  )

  set(ICONV_SEARCH_SRC_PATHS
    ${ICONV_ROOT}
    ${ICONV_ROOT}/lib
    ${ICONV_ROOT}/src
    ${ICONV_ROOT}/src/iconv
    ${ICONV_ROOT}/src/iconv/lib
  )
elseif (NOT MSVC)
  set(ICONV_SEARCH_HEADER_PATHS
      "/usr/include"
      "/usr/include/iconv"
      "/usr/include/x86_64-linux-gnu"
      "/usr/include/x86_64-linux-gnu/iconv"
  )

  set(ICONV_SEARCH_LIB_PATHS
      "/lib"
      "/lib/x86_64-linux-gnu"
      "/usr/lib"
      "/usr/lib/x86_64-linux-gnu"
  )

  set(ICONV_SEARCH_SRC_PATHS
      "/usr/src"
      "/usr/src/iconv"
      "/usr/src/iconv/lib"
  )
endif()

find_path(ICONV_INCLUDE_DIR
  iconv.h
  PATHS ${ICONV_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

include(Utils)

# set options for: shared
if (MSVC)
  set(ICONV_LIBRARY_PREFIX "")
  set(ICONV_LIBRARY_SUFFIX ".lib")
elseif(APPLE)
  set(ICONV_LIBRARY_PREFIX "lib")
  set(ICONV_LIBRARY_SUFFIX ".dylib")
else()
  set(ICONV_LIBRARY_PREFIX "lib")
  set(ICONV_LIBRARY_SUFFIX ".so")
endif()
set_find_library_options("${ICONV_LIBRARY_PREFIX}" "${ICONV_LIBRARY_SUFFIX}")

# find library
find_library(ICONV_SHARED_LIB
  NAMES iconv
  PATHS ${ICONV_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


# set options for: static
if (MSVC)
  set(ICONV_LIBRARY_PREFIX "")
  set(ICONV_LIBRARY_SUFFIX ".lib")
else()
  set(ICONV_LIBRARY_PREFIX "lib")
  set(ICONV_LIBRARY_SUFFIX ".a")
endif()
set_find_library_options("${ICONV_LIBRARY_PREFIX}" "${ICONV_LIBRARY_SUFFIX}")

# find library
find_library(ICONV_STATIC_LIB
  NAMES iconv
  PATHS ${ICONV_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


if (ICONV_INCLUDE_DIR AND ICONV_SHARED_LIB AND ICONV_STATIC_LIB)
  set(ICONV_FOUND TRUE)

  add_library(iconv_shared IMPORTED SHARED)
  set_target_properties(iconv_shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICONV_INCLUDE_DIR}"
    IMPORTED_LOCATION "${ICONV_SHARED_LIB}"
  )

  add_library(iconv_static IMPORTED STATIC)
  set_target_properties(iconv_static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICONV_INCLUDE_DIR}"
    IMPORTED_LOCATION "${ICONV_STATIC_LIB}"
  )

  set(ICONV_LIBRARY_DIR
    "${ICONV_SEARCH_LIB_PATHS}"
    CACHE PATH
    "Directory containing iconv libraries"
    FORCE
  )

  # build a list of shared libraries (staticRT)
  foreach(ELEMENT ${ICONV_SHARED_LIBS})
    get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
    string(REGEX MATCH "^(.*)\\.(dylib|lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

    if(NOT ELEMENT_MATCHES)
      continue()
    endif()

    get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
    file(GLOB ELEMENT_LIB
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so.*"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so.*"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.dylib"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.dylib"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.dylib.*"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.dylib.*"    )

    if(ELEMENT_LIB)
      list(APPEND ICONV_SHARED_LIB_RESOURCES ${ELEMENT_LIB})
    endif()
  endforeach()
else ()
  set(ICONV_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ICONV
  DEFAULT_MSG
  ICONV_INCLUDE_DIR
  ICONV_SHARED_LIB
  ICONV_STATIC_LIB
)
message("ICONV_INCLUDE_DIR: " ${ICONV_INCLUDE_DIR})
message("ICONV_LIBRARY_DIR: " ${ICONV_LIBRARY_DIR})
message("ICONV_SHARED_LIB: " ${ICONV_SHARED_LIB})
message("ICONV_STATIC_LIB: " ${ICONV_STATIC_LIB})

mark_as_advanced(
  ICONV_INCLUDE_DIR
  ICONV_LIBRARY_DIR
  ICONV_SHARED_LIB
  ICONV_STATIC_LIB
)

