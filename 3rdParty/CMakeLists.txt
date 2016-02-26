# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

################################################################################
## ETCD
################################################################################

if (GO_FOUND)
  if (MSVC)
    set (ETCD_BUILD_COMMAND build.bat)
  else ()
    set (ETCD_BUILD_COMMAND ./build)
  endif ()

  ExternalProject_Add(etcd_build
    SOURCE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/etcd
    CONFIGURE_COMMAND
      ""
    BUILD_IN_SOURCE
      TRUE
    BUILD_COMMAND
      "${ETCD_BUILD_COMMAND}"
    INSTALL_COMMAND
      "cp"
        ${CMAKE_CURRENT_SOURCE_DIR}/etcd/bin/etcd
        ${CMAKE_BINARY_DIR}/bin/etcd-arango
  )
endif ()

################################################################################
## BOOST
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/boost)

################################################################################
## EV
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libev)

################################################################################
## OPENSSL
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/OpenSSL)

################################################################################
## V8 and ICU
################################################################################

option(USE_PRECOMPILED_V8 "use a precompiled V8" OFF)

if (USE_PRECOMPILED_V8)
  set(V8_DIR ${CMAKE_CURRENT_SOURCE_DIR}/V8-4.3.61)

  set(V8_PROC_ARCH "x64")
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")

  set(V8_TARGET_DIR "${V8_DIR}/out")
  set(V8_BIN_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}/obj.target/tools/gyp")
  set(ICU_BIN_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}/obj.target/third_party/icu")

  set(ICU_VERSION
    "54.1"
    CACHE INTERNAL
    "ICU: Version"
    FORCE
  )

  set(ICU_INCLUDE_DIR
    ${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/i18n;${V8_DIR}/third_party/icu/source/io
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

  set(V8_VERSION
    ${V8_VERSION}
    CACHE INTERNAL
    "${PROJECT_NAME}: Version"
    FORCE
  )

  set(V8_INCLUDE_DIR
    ${V8_DIR}/include
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
endif ()

################################################################################
## ZLIB
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib-1.2.8)
