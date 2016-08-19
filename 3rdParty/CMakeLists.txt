# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

################################################################################
## EV
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libev)

################################################################################
## V8 and ICU
################################################################################

option(USE_PRECOMPILED_V8 "use a precompiled V8" OFF)

if (USE_PRECOMPILED_V8)
  set(V8_VERSION
    "V8-5.0.71.39"
    CACHE INTERNAL
    "${PROJECT_NAME}: Version"
    FORCE
  )

  set(V8_DIR ${PROJECT_SOURCE_DIR}/build/3rdParty/V8/v8)
  set(V8_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdParty/V8/${V8_VERSION})

  set(V8_PROC_ARCH "x64")
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")

  set(V8_TARGET_DIR "${V8_DIR}/${V8_TARGET_ARCH}/obj.target")
  set(V8_BIN_DIR "${V8_TARGET_DIR}/tools/gyp")
  set(ICU_BIN_DIR "${V8_TARGET_DIR}/third_party/icu")

  set(ICU_VERSION
    "54.1"
    CACHE INTERNAL
    "ICU: Version"
    FORCE
  )

  set(ICU_INCLUDE_DIR
    ${V8_SRC_DIR}/third_party/icu/source/common;${V8_SRC_DIR}/third_party/icu/source/i18n;${V8_SRC_DIR}/third_party/icu/source/io
    CACHE INTERNAL
    "ICU: include path"
    FORCE
  )

  set(ICU_LIBS
    "${ICU_BIN_DIR}/libicui18n.a;${ICU_BIN_DIR}/libicuuc.a;${ICU_BIN_DIR}/libicudata.a"
    ${libev_BINARY_DIR}/libev.a
    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
  )

  set(V8_INCLUDE_DIR
    ${V8_SRC_DIR};${V8_SRC_DIR}/include
    CACHE INTERNAL
    "${PROJECT_NAME}: include path"
    FORCE
  )

  set(V8_LIBS
    "${V8_BIN_DIR}/libv8_base.a;${V8_BIN_DIR}/libv8_libbase.a;${V8_BIN_DIR}/libv8_libplatform.a;${V8_BIN_DIR}/libv8_nosnapshot.a"
    ${libev_BINARY_DIR}/libev.a
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
    FORCE
  )
else ()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/V8)
  set(ICU_DT "${ICU_DT}" PARENT_SCOPE)
  set(ICU_DT_DEST "${ICU_DT_DEST}" PARENT_SCOPE)
endif ()

################################################################################
## ZLIB
################################################################################

set(SKIP_INSTALL_ALL ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib-1.2.8)

################################################################################
## ROCKSDB
################################################################################

if (USE_ROCKSDB)
  if (${JEMALLOC_FOUND})
#   set(USE_JEMALLOC_DEFAULT 1)
  endif()
  set(SKIP_INSTALL_ALL ON)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/snappy/google-snappy-d53de18)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/rocksdb)
endif ()

################################################################################
## LINK_DIRECTORIES
################################################################################

set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
