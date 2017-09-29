# - Find ICU (unicode/*.h, icu*.lib, icu*.a, icu*.so)
# This module defines
#  ICU_INCLUDE_DIR, directory containing headers
#  ICU_LIBRARY_DIR, directory containing ICU libraries
#  ICU_SHARED_LIBS, shared libraries required to use ICU, i.e. icu*.a/icu*.lib
#  ICU_STATIC_LIBS, static libraries required to use ICU, i.e. icu*.lib
#  ICU_SHARED_LIB_RESOURCES, shared libraries required to use ICU, i.e. icu*.so/icu*.dll
#  ICU_FOUND, whether ICU has been found

if ("${ICU_ROOT}" STREQUAL "")
  set(ICU_ROOT "$ENV{ICU_ROOT}")
  if (NOT "${ICU_ROOT}" STREQUAL "") 
    string(REPLACE "\"" "" ICU_ROOT ${ICU_ROOT})
  endif()
endif()

if (NOT "${ICU_ROOT}" STREQUAL "")
  set(ICU_SEARCH_HEADER_PATHS
    ${ICU_ROOT}/include
  )

  set(ICU_SEARCH_LIB_PATHS
    ${ICU_ROOT}/bin
    ${ICU_ROOT}/bin64
    ${ICU_ROOT}/lib
    ${ICU_ROOT}/lib64
  )
elseif (NOT MSVC)
  set(ICU_SEARCH_HEADER_PATHS
      "/usr/include"
      "/usr/include/x86_64-linux-gnu"
  )

  set(ICU_SEARCH_LIB_PATHS
      "/lib"
      "/lib/x86_64-linux-gnu"
      "/usr/lib"
      "/usr/lib/x86_64-linux-gnu"
  )
endif()

find_path(ICU_INCLUDE_DIR
  unicode/uversion.h
  PATHS ${ICU_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

include(Utils)


# set options for: shared
if (MSVC)
  set(ICU_LIBRARY_PREFIX "")
  set(ICU_LIBRARY_SUFFIX ".lib")
elseif(APPLE)
  set(ICU_LIBRARY_PREFIX "lib")
  set(ICU_LIBRARY_SUFFIX ".dylib")
else()
  set(ICU_LIBRARY_PREFIX "lib")
  set(ICU_LIBRARY_SUFFIX ".so")
endif()
set_find_library_options("${ICU_LIBRARY_PREFIX}" "${ICU_LIBRARY_SUFFIX}")

# find libraries
find_library(ICU_SHARED_LIBRARY_DT
  NAMES icudt icudata
  PATHS ${ICU_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(ICU_SHARED_LIBRARY_IN
  NAMES icuin icui18n
  PATHS ${ICU_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(ICU_SHARED_LIBRARY_UC
  NAMES icuuc
  PATHS ${ICU_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


# set options for: static
if (MSVC)
  set(ICU_LIBRARY_PREFIX "")
  set(ICU_LIBRARY_SUFFIX ".lib")
else()
  set(ICU_LIBRARY_PREFIX "lib")
  set(ICU_LIBRARY_SUFFIX ".a")
endif()
set_find_library_options("${ICU_LIBRARY_PREFIX}" "${ICU_LIBRARY_SUFFIX}")

# find libraries
find_library(ICU_STATIC_LIBRARY_DT
  NAMES icudt icudata
  PATHS ${ICU_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(ICU_STATIC_LIBRARY_IN
  NAMES icuin icui18n
  PATHS ${ICU_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)
find_library(ICU_STATIC_LIBRARY_UC
  NAMES icuuc
  PATHS ${ICU_SEARCH_LIB_PATHS}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


if (ICU_INCLUDE_DIR
    AND ICU_SHARED_LIBRARY_DT
    AND ICU_STATIC_LIBRARY_DT
    AND ICU_SHARED_LIBRARY_IN
    AND ICU_STATIC_LIBRARY_IN
    AND ICU_SHARED_LIBRARY_UC
    AND ICU_STATIC_LIBRARY_UC
)
  set(ICU_FOUND TRUE)
  list(APPEND ICU_SHARED_LIBS ${ICU_SHARED_LIBRARY_DT} ${ICU_SHARED_LIBRARY_IN} ${ICU_SHARED_LIBRARY_UC})

  if (MSVC)
    # ${ICU_STATIC_LIBRARY_DT} produces duplicate symbols if linked with MSVC
    list(APPEND ICU_STATIC_LIBS ${ICU_STATIC_LIBRARY_IN} ${ICU_STATIC_LIBRARY_UC})
  else()
    list(APPEND ICU_STATIC_LIBS ${ICU_STATIC_LIBRARY_DT} ${ICU_STATIC_LIBRARY_IN} ${ICU_STATIC_LIBRARY_UC})
  endif()

  add_library(icu-shared IMPORTED SHARED)
  set_target_properties(icu-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIR}"
    IMPORTED_LOCATION ${ICU_SHARED_LIBRARY_DT} ${ICU_SHARED_LIBRARY_IN} ${ICU_SHARED_LIBRARY_UC}
  )

  add_library(icu-static IMPORTED STATIC)
  set_target_properties(icu-static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIR}"
    IMPORTED_LOCATION ${ICU_STATIC_LIBRARY_DT} ${ICU_STATIC_LIBRARY_IN} ${ICU_STATIC_LIBRARY_UC}
  )

  set(ICU_LIBRARY_DIR
    "${ICU_SEARCH_LIB_PATHS}"
    CACHE PATH
    "Directory containing ICU libraries"
    FORCE
  )

  # build a list of shared libraries (staticRT)
  foreach(ELEMENT ${ICU_SHARED_LIBS})
    get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
    string(REGEX MATCH "^(.*)\\.(dylib|lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

    if(NOT ELEMENT_MATCHES)
      continue()
    endif()

    get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
    get_filename_component(ELEMENT_PARENT_DIRECTORY ${ELEMENT_DIRECTORY} DIRECTORY)

    foreach(ELEMENT_LIB_DIRECTORY "${ELEMENT_DIRECTORY}" "${ELEMENT_PARENT_DIRECTORY}/bin" "${ELEMENT_PARENT_DIRECTORY}/bin64" "${ELEMENT_PARENT_DIRECTORY}/lib" "${ELEMENT_PARENT_DIRECTORY}/lib64")
      file(GLOB ELEMENT_LIB
        "${ELEMENT_LIB_DIRECTORY}/${CMAKE_MATCH_1}.so"
        "${ELEMENT_LIB_DIRECTORY}/lib${CMAKE_MATCH_1}.so"
        "${ELEMENT_LIB_DIRECTORY}/${CMAKE_MATCH_1}.so.*"
        "${ELEMENT_LIB_DIRECTORY}/lib${CMAKE_MATCH_1}.so.*"
        "${ELEMENT_LIB_DIRECTORY}/${CMAKE_MATCH_1}.dylib"
        "${ELEMENT_LIB_DIRECTORY}/lib${CMAKE_MATCH_1}.dylib"
        "${ELEMENT_LIB_DIRECTORY}/${CMAKE_MATCH_1}.dylib.*"
        "${ELEMENT_LIB_DIRECTORY}/lib${CMAKE_MATCH_1}.dylib.*"
        "${ELEMENT_LIB_DIRECTORY}/${CMAKE_MATCH_1}[0-9][0-9].dll"
        "${ELEMENT_LIB_DIRECTORY}/lib${CMAKE_MATCH_1}[0-9][0-9].dll"
        "${ELEMENT_LIB_DIRECTORY}/${CMAKE_MATCH_1}[0-9][0-9].pdb"
        "${ELEMENT_LIB_DIRECTORY}/lib${CMAKE_MATCH_1}[0-9][0-9].pdb"
      )

      if(ELEMENT_LIB)
        list(APPEND ICU_SHARED_LIB_RESOURCES ${ELEMENT_LIB})
      endif()
    endforeach()
  endforeach()

  list(REMOVE_DUPLICATES ICU_SHARED_LIB_RESOURCES)
else ()
  set(ICU_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ICU
  DEFAULT_MSG
  ICU_INCLUDE_DIR
  ICU_SHARED_LIBS
  ICU_STATIC_LIBS
  ICU_SHARED_LIBRARY_DT
  ICU_STATIC_LIBRARY_DT
  ICU_SHARED_LIBRARY_IN
  ICU_STATIC_LIBRARY_IN
  ICU_SHARED_LIBRARY_UC
  ICU_STATIC_LIBRARY_UC
)
message("ICU_INCLUDE_DIR: " ${ICU_INCLUDE_DIR})
message("ICU_LIBRARY_DIR: " ${ICU_LIBRARY_DIR})
message("ICU_SHARED_LIBS: " ${ICU_SHARED_LIBS})
message("ICU_STATIC_LIBS: " ${ICU_STATIC_LIBS})
message("ICU_SHARED_LIB_RESOURCES: " ${ICU_SHARED_LIB_RESOURCES})

mark_as_advanced(
  ICU_INCLUDE_DIR
  ICU_LIBRARY_DIR
  ICU_SHARED_LIBS
  ICU_STATIC_LIBS
  ICU_SHARED_LIBRARY_DT
  ICU_STATIC_LIBRARY_DT
  ICU_SHARED_LIBRARY_IN
  ICU_STATIC_LIBRARY_IN
  ICU_SHARED_LIBRARY_UC
  ICU_STATIC_LIBRARY_UC
)
