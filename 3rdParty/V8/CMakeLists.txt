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

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

project(V8)
set(V8_VERSION 4.3.61)

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

  if (LINUX OR SOLARIS)
    set(V8_BIN_DIR "${V8_BIN_DIR}/obj.target/tools/gyp")
    set(ICU_BIN_DIR "${ICU_BIN_DIR}/obj.target/third_party/icu")
  endif ()

  list(APPEND V8_GYP_ARGS
    -Ibuild/standalone.gypi
    --depth=.
    -Dstandalone_static_library=1
    -Dlinux_use_bundled_gold=0
    -Dlinux_use_gold_flags=0
    -Dv8_no_strict_aliasing=1
    -Dwerror=
  )

  if (APPLE AND CMAKE_COMPILER_IS_CLANG)
    list(APPEND V8_COMPILE_ARGS CXXFLAGS=-stdlib=libc++ LDFLAGS=-stdlib=libc++)
  endif ()
endif()

list(APPEND V8_COMPILE_ARGS icui18n icuuc icudata)
list(APPEND V8_COMPILE_ARGS v8)

ExternalProject_Add(v8_build
  BUILD_IN_SOURCE TRUE
  SOURCE_DIR "${V8_DIR}"
  CONFIGURE_COMMAND ${PYTHON_EXECUTABLE} ${V8_GYP_ARGS}
  BUILD_COMMAND ${MAKE} ${V8_COMPILE_ARGS} 
  INSTALL_COMMAND "")

################################################################################
## ICU EXPORTS
################################################################################

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

################################################################################
## V8 EXPORTS
################################################################################

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
