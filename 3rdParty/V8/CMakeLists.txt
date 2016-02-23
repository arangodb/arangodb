# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release
      CACHE string
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif ()

project(V8)

macro(import_target tname tdep tinclude tpath)
  add_library(${tname} STATIC IMPORTED GLOBAL)
  add_dependencies(${tname} ${tdep})
  set_target_properties(${tname} PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${tinclude}"
    IMPORTED_LOCATION "${tpath}"
  )
endmacro()

set(V8_VERSION
  "4.3.61"
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
  FORCE
)

set(V8_DIR "${CMAKE_CURRENT_SOURCE_DIR}/V8-${V8_VERSION}")
set(V8_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/v8")

if ("${BITS}" STREQUAL "64")
  set(V8_PROC_ARCH "x64")
else ()
  message(ERROR "WHAT THE FUCK! FIXME")
  set(V8_PROC_ARCH "ia32")
endif () # TODO: ARM? Win32?

if (DEBUG)
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
else ()
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
endif ()

list(APPEND V8_GYP_ARGS
  ${V8_DIR}/build/gyp/gyp_main.py
    --generator-output=${V8_TARGET_DIR}
    build/all.gyp
    -Dv8_target_arch=${V8_PROC_ARCH}
    -S.${V8_TARGET_ARCH}
)

if (DEBUG)
  list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
endif ()

set(V8_BIN_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
set(ICU_BIN_DIR "${V8_BIN_DIR}")

set(V8_COMPILE_ARGS
  -C ${V8_TARGET_DIR}
  -f Makefile.${V8_TARGET_ARCH}
  BUILDTYPE=Release
  builddir=${V8_TARGET_DIR}/${V8_TARGET_ARCH}
  VERBOSE=1 V=1
  CC=${CMAKE_C_COMPILER}
  CXX=${CMAKE_CXX_COMPILER}
  PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
)
  
if (WIN32)
  # TODO: get this from the main configure paramentrisation
  # CMAKE_BUILD_TYPE:STRING=RelWithDebInfo
  # -G\ msvs_version=2013
  #  -Dtarget_arch=x64
  # -Dmode=release
  list(APPEND V8_GYP_ARGS -Dcomponent=static_library -Dlibrary=static_library)
else ()
  list(APPEND V8_GYP_ARGS --format make)

  if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR (CMAKE_SYSTEM_NAME MATCHES "Solaris.*") OR (CMAKE_SYSTEM_NAME MATCHES "SunOS.*"))
    set(V8_BIN_DIR "${V8_BIN_DIR}/obj.target/tools/gyp")
    set(ICU_BIN_DIR "${ICU_BIN_DIR}/obj.target/third_party/icu")
  endif ()

  # do not add -Dwerror='', this breaks under gyp
  
  list(APPEND V8_GYP_ARGS
    -Ibuild/standalone.gypi
    --depth=.
    -Dstandalone_static_library=1
    -Dlinux_use_bundled_gold=0
    -Dlinux_use_gold_flags=0
    -Dv8_no_strict_aliasing=1
  )

  if (APPLE AND CMAKE_COMPILER_IS_CLANG)
    list(APPEND V8_COMPILE_ARGS CXXFLAGS=-stdlib=libc++)
  endif ()
endif()

ExternalProject_Add(v8_build
  BUILD_IN_SOURCE TRUE
  SOURCE_DIR "${V8_DIR}"
  CONFIGURE_COMMAND ${PYTHON_EXECUTABLE} ${V8_GYP_ARGS}
  BUILD_COMMAND ${MAKE} ${V8_COMPILE_ARGS} 
  INSTALL_COMMAND "")

set(ICU_INCLUDE_DIR
  ${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/i18n;${V8_DIR}/third_party/icu/source/io
  CACHE INTERNAL
  "ICU: include path"
  FORCE
)

set(V8_INCLUDE_DIR
  ${V8_DIR}/include
  CACHE INTERNAL
  "${PROJECT_NAME}: include path"
  FORCE
)

set(ICU_BIN_DIR ${ICU_BIN_DIR} FORCE)
set(V8_BIN_DIR ${V8_BIN_DIR} FORCE)

import_target(v8_base v8_build
  "${V8_INCLUDE_DIR}"
  "${V8_BIN_DIR}/libv8_base.a"
)

import_target(v8_libbase v8_build
  "${V8_INCLUDE_DIR}"
  "${V8_BIN_DIR}/libv8_libbase.a"
)

import_target(v8_libplatform v8_build
  "${V8_INCLUDE_DIR}"
  "${V8_BIN_DIR}/libv8_libplatform.a"
)

import_target(v8_nosnapshot v8_build
  "${V8_INCLUDE_DIR}"
  "${V8_BIN_DIR}/libv8_nosnapshot.a"
)

import_target(v8_snapshot v8_build
  "${V8_INCLUDE_DIR}"
  "${V8_BIN_DIR}/libv8_snapshot.a"
)

import_target(icu_uc v8_build
  "${ICU_INCLUDE_DIR}"
  "${ICU_BIN_DIR}/libicuuc.a"
)

import_target(icu_data v8_build
  "${ICU_INCLUDE_DIR}"
  "${ICU_BIN_DIR}/libicudata.a"
)

import_target(icu_i18n v8_build
  "${ICU_INCLUDE_DIR}"
  "${ICU_BIN_DIR}/libicui18n.a"
)
