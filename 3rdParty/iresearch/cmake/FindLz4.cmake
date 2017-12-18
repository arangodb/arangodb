# - Find LZ4 (lz4.h, liblz4.a, liblz4.so, and liblz4.so.1)
# This module defines
#  Lz4_INCLUDE_DIR, directory containing headers
#  Lz4_LIBRARY_DIR, directory containing lz4 libraries
#  Lz4_SHARED_LIB, path to liblz4.so/liblz4.dll
#  Lz4_STATIC_LIB, path to liblz4.lib
#  Lz4_FOUND, whether lz4 has been found

if ("${LZ4_ROOT}" STREQUAL "")
  set(LZ4_ROOT "$ENV{LZ4_ROOT}")
  if (NOT "${LZ4_ROOT}" STREQUAL "") 
    string(REPLACE "\"" "" LZ4_ROOT ${LZ4_ROOT})
  endif()
endif()

if (NOT "${LZ4_ROOT}" STREQUAL "")
  set(LZ4_SEARCH_HEADER_PATHS
    ${LZ4_ROOT}
    ${LZ4_ROOT}/include
    ${LZ4_ROOT}/include/lz4
    ${LZ4_ROOT}/include
    ${LZ4_ROOT}/lib
  )

  set(LZ4_SEARCH_LIB_PATHS
    ${LZ4_ROOT}
    ${LZ4_ROOT}/lib
  )

  set(LZ4_SEARCH_SRC_PATHS
    ${LZ4_ROOT}
    ${LZ4_ROOT}/lib
    ${LZ4_ROOT}/src
    ${LZ4_ROOT}/src/lz4
    ${LZ4_ROOT}/src/lz4/lib
  )
elseif (NOT MSVC)
  set(LZ4_SEARCH_HEADER_PATHS
      "/usr/include"
      "/usr/include/lz4"
      "/usr/include/x86_64-linux-gnu"
      "/usr/include/x86_64-linux-gnu/lz4"
  )

  set(LZ4_SEARCH_LIB_PATHS
      "/lib"
      "/lib/x86_64-linux-gnu"
      "/usr/lib"
      "/usr/lib/x86_64-linux-gnu"
  )

  set(LZ4_SEARCH_SRC_PATHS
      "/usr/src"
      "/usr/src/lz4"
      "/usr/src/lz4/lib"
  )
endif()

find_path(Lz4_INCLUDE_DIR
  lz4.h
  PATHS ${LZ4_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

find_path(Lz4_SRC_DIR_LZ4
  lz4.c
  PATHS ${LZ4_SEARCH_SRC_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

if (Lz4_SRC_DIR_LZ4)
  get_filename_component(Lz4_SRC_DIR_PARENT ${Lz4_SRC_DIR_LZ4} DIRECTORY)

  find_path(Lz4_SRC_DIR_CMAKE
    CMakeLists.txt
    PATHS ${Lz4_SRC_DIR_LZ4} ${Lz4_SRC_DIR_PARENT}/contrib/cmake_unofficial
    NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
  )
endif()

# found the cmake enabled source directory
if (Lz4_INCLUDE_DIR AND Lz4_SRC_DIR_LZ4 AND Lz4_SRC_DIR_CMAKE)
  set(Lz4_FOUND TRUE)

  # for lz4_shared must set LZ4_BUNDLED_MODE=OFF + BUILD_STATIC_LIBS=ON CACHE FORCE
  # for lz4_static must set BUILD_SHARED_LIBS=ON CACHE FORCE
  set(LZ4_BUNDLED_MODE OFF) # enable lz4_shared
  set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE) # enable lz4_static
  set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE) # enable lz4_shared
  set(LZ4_POSITION_INDEPENDENT_LIB ON)
  add_subdirectory(
    ${Lz4_SRC_DIR_CMAKE}
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-lz4.dir
    EXCLUDE_FROM_ALL # do not build unused targets
  )

  set(Lz4_LIBRARY_DIR ${LZ4_SEARCH_LIB_PATHS})
  set(Lz4_SHARED_LIB lz4_shared)
  set(Lz4_STATIC_LIB lz4_static)

  return()
endif()

include(Utils)

# set options for: shared
if (MSVC)
  set(LZ4_LIBRARY_PREFIX "")
  set(LZ4_LIBRARY_SUFFIX ".lib")
elseif(APPLE)
  set(LZ4_LIBRARY_PREFIX "lib")
  set(LZ4_LIBRARY_SUFFIX ".dylib")
else()
  set(LZ4_LIBRARY_PREFIX "lib")
  set(LZ4_LIBRARY_SUFFIX ".so")
endif()
set_find_library_options("${LZ4_LIBRARY_PREFIX}" "${LZ4_LIBRARY_SUFFIX}")

# find library
find_library(Lz4_SHARED_LIB
  NAMES lz4
  PATHS ${LZ4_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


# set options for: static
if (MSVC)
  set(LZ4_LIBRARY_PREFIX "")
  set(LZ4_LIBRARY_SUFFIX ".lib")
else()
  set(LZ4_LIBRARY_PREFIX "lib")
  set(LZ4_LIBRARY_SUFFIX ".a")
endif()
set_find_library_options("${LZ4_LIBRARY_PREFIX}" "${LZ4_LIBRARY_SUFFIX}")

# find library
find_library(Lz4_STATIC_LIB
  NAMES lz4
  PATHS ${LZ4_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


if (Lz4_INCLUDE_DIR AND Lz4_SHARED_LIB AND Lz4_STATIC_LIB)
  set(Lz4_FOUND TRUE)
  set(Lz4_LIBRARY_DIR
    "${LZ4_SEARCH_LIB_PATHS}"
    CACHE PATH
    "Directory containing lz4 libraries"
    FORCE
  )

  add_library(lz4_shared IMPORTED SHARED)
  set_target_properties(lz4_shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${Lz4_INCLUDE_DIR}"
    IMPORTED_LOCATION "${Lz4_SHARED_LIB}"
  )

  add_library(lz4_static IMPORTED STATIC)
  set_target_properties(lz4_static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${Lz4_INCLUDE_DIR}"
    IMPORTED_LOCATION "${Lz4_STATIC_LIB}"
  )
else ()
  set(Lz4_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lz4
  DEFAULT_MSG
  Lz4_INCLUDE_DIR
  Lz4_SHARED_LIB
  Lz4_STATIC_LIB
)
message("Lz4_INCLUDE_DIR: " ${Lz4_INCLUDE_DIR})
message("Lz4_LIBRARY_DIR: " ${Lz4_LIBRARY_DIR})
message("Lz4_SHARED_LIB: " ${Lz4_SHARED_LIB})
message("Lz4_STATIC_LIB: " ${Lz4_STATIC_LIB})

mark_as_advanced(
  Lz4_INCLUDE_DIR
  Lz4_LIBRARY_DIR
  Lz4_SHARED_LIB
  Lz4_STATIC_LIB
)
