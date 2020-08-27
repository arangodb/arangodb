# - Find S2
# This module defines
#  S2_INCLUDE_DIR, directory containing headers
#  S2_LIBRARY_DIR, directory containing s2 libraries
#  S2_SHARED_LIB, path to libs2.so/libs2.dll
#  S2_STATIC_LIB, path to libs2.lib
#  S2_FOUND, whether s2 has been found

if ("${S2_ROOT}" STREQUAL "")
  set(S2_ROOT "$ENV{S2_ROOT}")
  if (NOT "${S2_ROOT}" STREQUAL "") 
    string(REPLACE "\"" "" S2_ROOT ${S2_ROOT})
  endif()
endif()

if (NOT "${S2_ROOT}" STREQUAL "")
  set(S2_SEARCH_HEADER_PATHS
    ${S2_ROOT}
    ${S2_ROOT}/src
    ${S2_ROOT}/src/s2
  )

  set(S2_SEARCH_LIB_PATHS
    ${S2_ROOT}
    ${S2_ROOT}/lib
  )

  set(S2_SEARCH_SRC_PATHS
    ${S2_ROOT}
    ${S2_ROOT}/src
    ${S2_ROOT}/src/s2
  )
elseif (NOT MSVC)
  set(S2_SEARCH_HEADER_PATHS
    "/usr/include"
    "/usr/include/s2"
    "/usr/include/x86_64-linux-gnu"
    "/usr/include/x86_64-linux-gnu/s2"
  )

  set(S2_SEARCH_LIB_PATHS
    "/lib"
    "/lib/x86_64-linux-gnu"
    "/usr/lib"
    "/usr/lib/x86_64-linux-gnu"
  )

  set(S2_SEARCH_SRC_PATHS
    "/usr/src"
    "/usr/src/s2"
  )
endif()

find_path(S2_INCLUDE_DIR
  s2/s1angle.h
  PATHS ${S2_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

find_path(S2_SRC_DIR_S2
  s1angle.cc
  PATHS ${S2_SEARCH_SRC_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

if (S2_SRC_DIR_S2)
  get_filename_component(S2_SRC_DIR_PARENT ${S2_SRC_DIR_S2} DIRECTORY)

  find_path(S2_SRC_DIR_CMAKE
    CMakeLists.txt
    PATHS ${S2_ROOT}
    NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
  )
endif()

# found the cmake enabled source directory
if (S2_INCLUDE_DIR AND S2_SRC_DIR_S2 AND S2_SRC_DIR_CMAKE)
  set(S2_FOUND TRUE)

  set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
  set(CMAKE_USE_OPENSSL ON CACHE BOOL "" FORCE)
  set(WITH_GLOG OFF CACHE BOOL "" FORCE)
  set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(INSTALL_HEADERS OFF CACHE BOOL "" FORCE)

  add_subdirectory(
    ${S2_SRC_DIR_CMAKE}
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-s2.dir
    EXCLUDE_FROM_ALL # do not build unused targets
  )

  set(S2_LIBRARY_DIR ${S2_SEARCH_LIB_PATHS})
  set(S2_STATIC_LIB s2)

  return()
endif()

include(Utils)

# set options for: shared
if (MSVC)
  set(S2_LIBRARY_PREFIX "")
  set(S2_LIBRARY_SUFFIX ".lib")
elseif(APPLE)
  set(S2_LIBRARY_PREFIX "lib")
  set(S2_LIBRARY_SUFFIX ".dylib")
else()
  set(S2_LIBRARY_PREFIX "lib")
  set(S2_LIBRARY_SUFFIX ".so")
endif()
set_find_library_options("${S2_LIBRARY_PREFIX}" "${S2_LIBRARY_SUFFIX}")

# find library
find_library(S2_SHARED_LIB
  NAMES s2
  PATHS ${S2_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()

# set options for: static
if (MSVC)
  set(S2_LIBRARY_PREFIX "")
  set(S2_LIBRARY_SUFFIX ".lib")
else()
  set(S2_LIBRARY_PREFIX "lib")
  set(S2_LIBRARY_SUFFIX ".a")
endif()
set_find_library_options("${S2_LIBRARY_PREFIX}" "${S2_LIBRARY_SUFFIX}")

# find library
find_library(S2_STATIC_LIB
  NAMES s2
  PATHS ${S2_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()

if (S2_INCLUDE_DIR AND S2_SHARED_LIB AND S2_STATIC_LIB)
  set(S2_FOUND TRUE)
  set(S2_LIBRARY_DIR
    "${S2_SEARCH_LIB_PATHS}"
    CACHE PATH
    "Directory containing s2 libraries"
    FORCE
  )

  add_library(s2_shared IMPORTED SHARED)
  set_target_properties(s2_shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${S2_INCLUDE_DIR}"
    IMPORTED_LOCATION "${S2_SHARED_LIB}"
  )

  add_library(s2_static IMPORTED STATIC)
  set_target_properties(s2_static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${S2_INCLUDE_DIR}"
    IMPORTED_LOCATION "${S2_STATIC_LIB}"
  )
else ()
  set(S2_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(S2Geometry
  DEFAULT_MSG
  S2_INCLUDE_DIR
  S2_SHARED_LIB
  S2_STATIC_LIB
)
message("S2_INCLUDE_DIR: " ${S2_INCLUDE_DIR})
message("S2_LIBRARY_DIR: " ${S2_LIBRARY_DIR})
message("S2_SHARED_LIB: " ${S2_SHARED_LIB})
message("S2_STATIC_LIB: " ${S2_STATIC_LIB})

mark_as_advanced(
  S2_INCLUDE_DIR
  S2_LIBRARY_DIR
  S2_SHARED_LIB
  S2_STATIC_LIB
)
