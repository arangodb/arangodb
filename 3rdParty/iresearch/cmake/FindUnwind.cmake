# - Find Unwind (libunwind.h, libunwind.a, libunwind.so, libunwind.lib, libunwind.dll)
# This module defines
#  Unwind_INCLUDE_DIR, directory containing headers
#  Unwind_LIBRARY_DIR, directory containing unwind libraries
#  Unwind_SHARED_LIBS, path to libunwind*.so/libunwind*.lib
#  Unwind_STATIC_LIBS, path to libunwind*.a/libunwind*.lib
#  Unwind_SHARED_LIB_RESOURCES, shared libraries required to use Unwind, i.e. libunwind*.so/libunwind*.dll
#  Unwind_FOUND, whether unwind has been found

if ("${UNWIND_ROOT}" STREQUAL "")
  set(UNWIND_ROOT "$ENV{UNWIND_ROOT}")
  if (NOT "${UNWIND_ROOT}" STREQUAL "")
    string(REPLACE "\"" "" UNWIND_ROOT ${UNWIND_ROOT})
  endif()
endif()

if (NOT "${UNWIND_ROOT}" STREQUAL "")
  set(UNWIND_SEARCH_HEADER_PATHS
    ${UNWIND_ROOT}/include
  )

  set(UNWIND_SEARCH_LIB_PATHS
    ${UNWIND_ROOT}/lib
    ${UNWIND_ROOT}/src/.libs
  )
elseif (NOT MSVC)
  set(UNWIND_SEARCH_HEADER_PATHS
      "/usr/include"
      "/usr/include/x86_64-linux-gnu"
  )

  set(UNWIND_SEARCH_LIB_PATHS
      "/lib"
      "/lib/x86_64-linux-gnu"
      "/usr/lib"
      "/usr/lib/x86_64-linux-gnu"
  )
endif()

find_path(Unwind_INCLUDE_DIR
  libunwind.h
  PATHS ${UNWIND_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

include(Utils)

# set options for: shared
if (MSVC)
  set(UNWIND_LIBRARY_PREFIX "")
  set(UNWIND_LIBRARY_SUFFIX ".lib")
else()
  set(UNWIND_LIBRARY_PREFIX "lib")
  set(UNWIND_LIBRARY_SUFFIX ".so")
endif()
set_find_library_options("${UNWIND_LIBRARY_PREFIX}" "${UNWIND_LIBRARY_SUFFIX}")

# find library
find_library(UNWIND_SHARED_LIBRARY_CORE
  NAMES unwind
  PATHS ${UNWIND_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(UNWIND_SHARED_LIBRARY_LZMA
  NAMES lzma
  PATHS ${UNWIND_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(UNWIND_SHARED_LIBRARY_PLATFORM
  NAMES unwind-x86_64
  PATHS ${UNWIND_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


# set options for: static
if (MSVC)
  set(UNWIND_LIBRARY_PREFIX "")
  set(UNWIND_LIBRARY_SUFFIX ".lib")
else()
  set(UNWIND_LIBRARY_PREFIX "lib")
  set(UNWIND_LIBRARY_SUFFIX ".a")
endif()
set_find_library_options("${UNWIND_LIBRARY_PREFIX}" "${UNWIND_LIBRARY_SUFFIX}")

# find library
find_library(UNWIND_STATIC_LIBRARY_CORE
  NAMES unwind
  PATHS ${UNWIND_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(UNWIND_STATIC_LIBRARY_LZMA
  NAMES lzma
  PATHS ${UNWIND_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(UNWIND_STATIC_LIBRARY_PLATFORM
  NAMES unwind-x86_64
  PATHS ${UNWIND_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


if (Unwind_INCLUDE_DIR
    AND UNWIND_SHARED_LIBRARY_CORE
    AND UNWIND_SHARED_LIBRARY_PLATFORM
    AND UNWIND_STATIC_LIBRARY_CORE
    AND UNWIND_STATIC_LIBRARY_PLATFORM
)
  set(Unwind_FOUND TRUE)
  list(APPEND Unwind_SHARED_LIBS ${UNWIND_SHARED_LIBRARY_CORE} ${UNWIND_SHARED_LIBRARY_PLATFORM})
  list(APPEND Unwind_STATIC_LIBS ${UNWIND_STATIC_LIBRARY_CORE} ${UNWIND_STATIC_LIBRARY_PLATFORM})

  if(UNWIND_SHARED_LIBRARY_LZMA)
    list(APPEND Unwind_SHARED_LIBS ${UNWIND_SHARED_LIBRARY_LZMA})
  endif()

  if(UNWIND_STATIC_LIBRARY_LZMA)
    list(APPEND Unwind_STATIC_LIBS ${UNWIND_STATIC_LIBRARY_LZMA})
  endif()

  add_library(unwind-shared IMPORTED SHARED)
  set_target_properties(unwind-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${UNWIND_INCLUDE_DIR}"
    IMPORTED_LOCATION "${Unwind_SHARED_LIBS}"
  )

  add_library(unwind-static IMPORTED STATIC)
  set_target_properties(unwind-static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${UNWIND_INCLUDE_DIR}"
    IMPORTED_LOCATION "${Unwind_STATIC_LIBS}"
  )

  set(Unwind_LIBRARY_DIR
    "${UNWIND_SEARCH_LIB_PATHS}"
    CACHE PATH
    "Directory containing unwind libraries"
    FORCE
  )

  # build a list of shared libraries (staticRT)
  foreach(ELEMENT ${Unwind_SHARED_LIBS})
    get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
    string(REGEX MATCH "^(.*)\\.(lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

    if(NOT ELEMENT_MATCHES)
      continue()
    endif()

    get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
    file(GLOB ELEMENT_LIB
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so"
      "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so.*"
      "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so.*"
    )

    if(ELEMENT_LIB)
      list(APPEND Unwind_SHARED_LIB_RESOURCES ${ELEMENT_LIB})
    endif()
  endforeach()
else ()
  set(Unwind_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Unwind
  DEFAULT_MSG
  Unwind_INCLUDE_DIR
  Unwind_SHARED_LIBS
  Unwind_STATIC_LIBS
  UNWIND_SHARED_LIBRARY_CORE
  UNWIND_STATIC_LIBRARY_CORE
  UNWIND_SHARED_LIBRARY_PLATFORM
  UNWIND_STATIC_LIBRARY_PLATFORM
)
message("Unwind_INCLUDE_DIR: " ${Unwind_INCLUDE_DIR})
message("Unwind_LIBRARY_DIR: " ${Unwind_LIBRARY_DIR})
message("Unwind_SHARED_LIBS: " ${Unwind_SHARED_LIBS})
message("Unwind_STATIC_LIBS: " ${Unwind_STATIC_LIBS})
message("Unwind_SHARED_LIB_RESOURCES: " ${Unwind_SHARED_LIB_RESOURCES})

if(NOT UNWIND_SHARED_LIBRARY_LZMA)
  message("LZMA shared library not found. Required if during linking the following errors are seen: undefined reference to `lzma_...")
endif()

if(NOT UNWIND_STATIC_LIBRARY_LZMA)
  message("LZMA static library not found. Required if during linking the following errors are seen: undefined reference to `lzma_...")
endif()

mark_as_advanced(
  Unwind_INCLUDE_DIR
  Unwind_LIBRARY_DIR
  Unwind_SHARED_LIBS
  Unwind_STATIC_LIBS
  UNWIND_SHARED_LIBRARY_CORE
  UNWIND_STATIC_LIBRARY_CORE
  UNWIND_SHARED_LIBRARY_LZMA
  UNWIND_STATIC_LIBRARY_LZMA
  UNWIND_SHARED_LIBRARY_PLATFORM
  UNWIND_STATIC_LIBRARY_PLATFORM
)
