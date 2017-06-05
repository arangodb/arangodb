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

if ("${LZ4_ROOT_SUFFIX}" STREQUAL "")
  set(LZ4_ROOT_SUFFIX "$ENV{LZ4_ROOT_SUFFIX}")
  if (NOT "${LZ4_ROOT_SUFFIX}" STREQUAL "")
    string(REPLACE "\"" "" LZ4_ROOT_SUFFIX ${LZ4_ROOT_SUFFIX})
  endif()
endif()

set(LZ4_SEARCH_HEADER_PATHS
  ${LZ4_ROOT}/include
)

set(LZ4_SEARCH_LIB_PATH
  ${LZ4_ROOT}/lib
)

if(NOT MSVC)
  set(UNIX_DEFAULT_INCLUDE "/usr/include")
endif()
find_path(Lz4_INCLUDE_DIR 
  lz4.h 
  PATHS ${LZ4_SEARCH_HEADER_PATHS} ${UNIX_DEFAULT_INCLUDE}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

include(Utils)
if(NOT MSVC)
    set(UNIX_DEFAULT_LIB "/usr/lib")
endif()


# set options for: shared
if (MSVC)
  set(LZ4_LIBRARY_PREFIX "")
  set(LZ4_LIBRARY_SUFFIX ".lib")
else()
  set(LZ4_LIBRARY_PREFIX "lib")
  set(LZ4_LIBRARY_SUFFIX ".so")
endif()
set_find_library_options("${LZ4_LIBRARY_PREFIX}" "${LZ4_LIBRARY_SUFFIX}")

# find library
find_library(Lz4_SHARED_LIB
  NAMES lz4
  PATHS ${LZ4_SEARCH_LIB_PATH} ${UNIX_DEFAULT_LIB}
  PATH_SUFFIXES ${LZ4_ROOT_SUFFIX}
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
  PATHS ${LZ4_SEARCH_LIB_PATH} ${UNIX_DEFAULT_LIB}
  PATH_SUFFIXES ${LZ4_ROOT_SUFFIX}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


if (Lz4_INCLUDE_DIR AND Lz4_SHARED_LIB AND Lz4_STATIC_LIB)
  set(Lz4_FOUND TRUE)
  set(Lz4_LIBRARY_DIR
    "${LZ4_SEARCH_LIB_PATH}"
    CACHE PATH
    "Directory containing lz4 libraries"
    FORCE
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
