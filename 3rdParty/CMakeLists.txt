# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

################################################################################
## V8 and ICU
################################################################################

option(USE_PRECOMPILED_V8 "use a precompiled V8" OFF)


set(V8_SUB_DIR "v5.7.0.0")
set(V8_VERSION
  "5.7.0.0"
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
  FORCE
  )
set(V8_VERSION ${V8_VERSION} PARENT_SCOPE)
if (USE_PRECOMPILED_V8)

  set(V8_DIR ${PROJECT_SOURCE_DIR}/build/3rdParty/V8/${V8_SUB_DIR})
  set(V8_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdParty/V8/${V8_SUB_DIR})

  set(V8_PROC_ARCH "x64")
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")

  set(V8_TARGET_DIR "${V8_DIR}/${V8_TARGET_ARCH}/obj.target")
  set(V8_BIN_DIR "${V8_TARGET_DIR}/src/")
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
    "${ICU_BIN_DIR}/libicui18n.a;${ICU_BIN_DIR}/libicuuc.a"
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
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib-1.2.11)

################################################################################
## cURL
################################################################################

set(ENABLE_MANUAL OFF CACHE type BOOL)
set(BUILD_CURL_EXE OFF CACHE type BOOL)
set(BUILD_TESTING OFF CACHE type BOOL)
set(CURL_STATICLIB ON CACHE type BOOL)
set(HTTP_ONLY ON CACHE type BOOL)
set(CMAKE_USE_LIBSSH2 OFF CACHE type BOOL)
set(CMAKE_USE_OPENSSL ON CACHE type BOOL)
# mop: super important...if this is off curl will not handle request timeouts < 1000ms
set(ENABLE_THREADED_RESOLVER ON CACHE type BOOL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/curl/curl-7.50.3)

################################################################################
## SNAPPY
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/snappy/google-snappy-d53de18)

################################################################################
## LINK_DIRECTORIES
################################################################################

set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
