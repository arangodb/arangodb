# - Find ICU (unicode/*.h, icu*.lib, icu*.a, icu*.so)
# This module defines
#  ICU_INCLUDE_DIR, directory containing headers
#  ICU_LIBRARY_DIR, directory containing ICU libraries
#  ICU_SHARED_LIBS, shared libraries required to use ICU, i.e. icu*.a/icu*.lib
#  ICU_STATIC_LIBS, static libraries required to use ICU, i.e. icu*.lib
#  ICU_SHARED_LIB_RESOURCES, shared libraries required to use ICU, i.e. icu*.so/icu*.dll
#  ICU_FOUND, whether ICU has been found

if (ICU_FOUND)
  return()
endif()

if ("${ICU_ROOT}" STREQUAL "")
  set(ICU_ROOT "$ENV{ICU_ROOT}")
  if (NOT "${ICU_ROOT}" STREQUAL "") 
    string(REPLACE "\"" "" ICU_ROOT ${ICU_ROOT})
  endif()
endif()

if (NOT "${ICU_ROOT}" STREQUAL "")
  set(ICU_SEARCH_HEADER_PATHS
    ${ICU_ROOT}
    ${ICU_ROOT}/common
    ${ICU_ROOT}/include
    ${ICU_ROOT}/include/common
    ${ICU_ROOT}/source/common
    ${ICU_ROOT}/icu/source/common
  )

  set(ICU_SEARCH_LIB_PATHS
    ${ICU_ROOT}/bin
    ${ICU_ROOT}/bin64
    ${ICU_ROOT}/lib
    ${ICU_ROOT}/lib64
  )

  set(ICU_SEARCH_SRC_PATHS
    ${ICU_ROOT}
    ${ICU_ROOT}/source
    ${ICU_ROOT}/icu/source
    ${ICU_ROOT}/src
    ${ICU_ROOT}/src/icu
    ${ICU_ROOT}/src/icu/source
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

  set(ICU_SEARCH_SRC_PATHS
      "/usr/src"
      "/usr/src/icu"
      "/usr/src/icu/source"
  )
endif()

find_path(ICU_INCLUDE_DIR
  unicode/uversion.h
  PATHS ${ICU_SEARCH_HEADER_PATHS}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

find_path(ICU_SRC_DIR_UCONV
  ucnv.c
  PATHS ${ICU_SEARCH_SRC_PATHS}
  PATH_SUFFIXES common
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

if (ICU_SRC_DIR_UCONV)
  get_filename_component(ICU_SRC_DIR_PARENT ${ICU_SRC_DIR_UCONV} DIRECTORY)

  find_file(ICU_SRC_DIR_CONFIGURE
    configure
    PATHS ${ICU_SRC_DIR_PARENT}
    NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
  )
endif()

# found the configure enabled source directory
if (ICU_INCLUDE_DIR AND ICU_SRC_DIR_UCONV AND ICU_SRC_DIR_CONFIGURE)
  set(ICU_FOUND TRUE)
  set(ICU_WORK_PATH "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-icu.dir")
  set(ICU_INCLUDE_PATH "${ICU_WORK_PATH}/build/include")
  set(ICU_LIBRARY_PATH "${ICU_WORK_PATH}/ebuild/lib")

  if (MSVC)
    set(ICU_SHARED_LIBRARY_PREFIX "")
    set(ICU_STATIC_LIBRARY_PREFIX "")
    set(ICU_SHARED_LIBRARY_SUFFIX ".lib")
    set(ICU_STATIC_LIBRARY_SUFFIX ".lib")
  elseif(APPLE)
    set(ICU_SHARED_LIBRARY_PREFIX "lib")
    set(ICU_STATIC_LIBRARY_PREFIX "lib")
    set(ICU_SHARED_LIBRARY_SUFFIX ".dylib")
    set(ICU_STATIC_LIBRARY_SUFFIX ".a")
  else()
    set(ICU_SHARED_LIBRARY_PREFIX "lib")
    set(ICU_STATIC_LIBRARY_PREFIX "lib")
    set(ICU_SHARED_LIBRARY_SUFFIX ".so")
    set(ICU_STATIC_LIBRARY_SUFFIX ".a")
  endif()

  # directory required to exist before using WORKING_DIRECTORY
  file(MAKE_DIRECTORY "${ICU_WORK_PATH}")

  add_custom_target(icu-build
    COMMAND test -d "${ICU_LIBRARY_PATH}" || "${ICU_SRC_DIR_CONFIGURE}" "--disable-samples" "--disable-tests" "--enable-static" "--srcdir=${ICU_SRC_DIR_PARENT}" "--prefix=${ICU_WORK_PATH}/build" "--exec-prefix=${ICU_WORK_PATH}/ebuild"
    COMMAND test -d "${ICU_LIBRARY_PATH}" || make -j 8 install
    WORKING_DIRECTORY "${ICU_WORK_PATH}"
    VERBATIM
  )

  # directory required to exist before using INTERFACE_INCLUDE_DIRECTORIES
  file(MAKE_DIRECTORY "${ICU_INCLUDE_PATH}")

  add_library(icudata-shared SHARED IMPORTED GLOBAL)
  add_dependencies(icudata-shared icu-build)
  set_target_properties(icudata-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_PATH}"
    IMPORTED_LOCATION "${ICU_LIBRARY_PATH}/${ICU_SHARED_LIBRARY_PREFIX}icudata${ICU_SHARED_LIBRARY_SUFFIX}"
  )

  add_library(icudata-static STATIC IMPORTED GLOBAL)
  add_dependencies(icudata-shared icu-build)
  set_target_properties(icudata-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_PATH}"
    IMPORTED_LOCATION "${ICU_LIBRARY_PATH}/${ICU_STATIC_LIBRARY_PREFIX}icudata${ICU_SHARED_LIBRARY_SUFFIX}"
  )

  add_library(icui18n-shared SHARED IMPORTED GLOBAL)
  add_dependencies(icui18n-shared icu-build)
  set_target_properties(icui18n-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_PATH}"
    IMPORTED_LOCATION "${ICU_LIBRARY_PATH}/${ICU_SHARED_LIBRARY_PREFIX}icui18n${ICU_SHARED_LIBRARY_SUFFIX}"
  )

  add_library(icui18n-static STATIC IMPORTED GLOBAL)
  add_dependencies(icui18n-shared icu-build)
  set_target_properties(icui18n-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_PATH}"
    IMPORTED_LOCATION "${ICU_LIBRARY_PATH}/${ICU_STATIC_LIBRARY_PREFIX}icui18n${ICU_SHARED_LIBRARY_SUFFIX}"
  )

  add_library(icuuc-shared SHARED IMPORTED GLOBAL)
  add_dependencies(icuuc-shared icu-build)
  set_target_properties(icuuc-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_PATH}"
    IMPORTED_LOCATION "${ICU_LIBRARY_PATH}/${ICU_SHARED_LIBRARY_PREFIX}icuuc${ICU_SHARED_LIBRARY_SUFFIX}"
  )

  add_library(icuuc-static STATIC IMPORTED GLOBAL)
  add_dependencies(icuuc-shared icu-build)
  set_target_properties(icuuc-shared PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_PATH}"
    IMPORTED_LOCATION "${ICU_LIBRARY_PATH}/${ICU_STATIC_LIBRARY_PREFIX}icuuc${ICU_SHARED_LIBRARY_SUFFIX}"
  )

  add_library(icu-shared SHARED IMPORTED GLOBAL)
  add_dependencies(icu-shared icudata-shared icui18n-shared icuuc-shared)
  set_property(TARGET icu-shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES # $<TARGET_PROPERTY:tgt,INCLUDE_DIRECTORIES> does not get expanded
    "${ICU_INCLUDE_PATH}"
  )
  set_property(TARGET icu-shared PROPERTY IMPORTED_LOCATION # $<TARGET_FILE:tgt> does not get expanded
    "${ICU_LIBRARY_PATH}/${ICU_SHARED_LIBRARY_PREFIX}icui18n${ICU_SHARED_LIBRARY_SUFFIX}"
  )
  set_property(TARGET icu-shared PROPERTY INTERFACE_LINK_LIBRARIES # additional IMPORTED_LOCATION value list
    "${ICU_LIBRARY_PATH}/${ICU_SHARED_LIBRARY_PREFIX}icuuc${ICU_SHARED_LIBRARY_SUFFIX}" # must be after icui18n
    "${ICU_LIBRARY_PATH}/${ICU_SHARED_LIBRARY_PREFIX}icudata${ICU_SHARED_LIBRARY_SUFFIX}" # must be after icuuc
  )

  add_library(icu-static STATIC IMPORTED GLOBAL)
  add_dependencies(icu-static icudata-static icui18n-static icuuc-static)
  set_property(TARGET icu-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES # $<TARGET_PROPERTY:tgt,INCLUDE_DIRECTORIES> does not get expanded
    "${ICU_INCLUDE_PATH}"
  )
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION # $<TARGET_FILE:tgt> does not get expanded
    "${ICU_LIBRARY_PATH}/${ICU_STATIC_LIBRARY_PREFIX}icui18n${ICU_STATIC_LIBRARY_SUFFIX}"
  )
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES # additional IMPORTED_LOCATION value list
    "${ICU_LIBRARY_PATH}/${ICU_STATIC_LIBRARY_PREFIX}icuuc${ICU_STATIC_LIBRARY_SUFFIX}" # must be after icui18n
    "${ICU_LIBRARY_PATH}/${ICU_STATIC_LIBRARY_PREFIX}icudata${ICU_STATIC_LIBRARY_SUFFIX}" # must be after icuuc
  )

  set(ICU_SHARED_LIBRARY_DT "$<TARGET_FILE:icudata-shared>")
  set(ICU_STATIC_LIBRARY_DT "$<TARGET_FILE:icudata-static>")
  set(ICU_SHARED_LIBRARY_IN "$<TARGET_FILE:icui18n-shared>")
  set(ICU_STATIC_LIBRARY_IN "$<TARGET_FILE:icui18n-static>")
  set(ICU_SHARED_LIBRARY_UC "$<TARGET_FILE:icuuc-shared>")
  set(ICU_STATIC_LIBRARY_UC "$<TARGET_FILE:icuuc-static>")

  list(APPEND ICU_SHARED_LIBS ${ICU_SHARED_LIBRARY_DT} ${ICU_SHARED_LIBRARY_IN} ${ICU_SHARED_LIBRARY_UC})

  if (MSVC)
    # ${ICU_STATIC_LIBRARY_DT} produces duplicate symbols if linked with MSVC
    list(APPEND ICU_STATIC_LIBS ${ICU_STATIC_LIBRARY_IN} ${ICU_STATIC_LIBRARY_UC})
  else()
    list(APPEND ICU_STATIC_LIBS ${ICU_STATIC_LIBRARY_DT} ${ICU_STATIC_LIBRARY_IN} ${ICU_STATIC_LIBRARY_UC})
  endif()

  return()
endif()

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

  add_library(icu-shared SHARED IMPORTED GLOBAL)
  set_property(TARGET icu-shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES # $<TARGET_PROPERTY:tgt,INCLUDE_DIRECTORIES> does not get expanded
    "${ICU_INCLUDE_DIR}"
  )
  set_property(TARGET icu-shared PROPERTY IMPORTED_LOCATION # $<TARGET_FILE:tgt> does not get expanded
    "${ICU_SHARED_LIBRARY_IN}"
  )
  set_property(TARGET icu-shared PROPERTY INTERFACE_LINK_LIBRARIES # additional IMPORTED_LOCATION value list
    "${ICU_SHARED_LIBRARY_UC}" # must be after icui18n
    "${ICU_SHARED_LIBRARY_DT}" # must be after icuuc
  )

  add_library(icu-static STATIC IMPORTED GLOBAL)
  set_property(TARGET icu-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES # $<TARGET_PROPERTY:tgt,INCLUDE_DIRECTORIES> does not get expanded
    "${ICU_INCLUDE_DIR}"
  )
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION # $<TARGET_FILE:tgt> does not get expanded
    "${ICU_STATIC_LIBRARY_IN}"
  )
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES # additional IMPORTED_LOCATION value list
    "${ICU_STATIC_LIBRARY_UC}" # must be after icui18n
    "${ICU_STATIC_LIBRARY_DT}" # must be after icuuc
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
