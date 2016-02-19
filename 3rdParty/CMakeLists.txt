# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

macro(import_target tname tdep tinclude tpath)
  add_library(${tname} STATIC IMPORTED GLOBAL)
  add_dependencies(${tname} ${tdep})
  set_target_properties(${tname} PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${tinclude}"
    IMPORTED_LOCATION "${tpath}"
  )
endmacro()

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
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/etcd
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND "${ETCD_BUILD_COMMAND}"
    INSTALL_COMMAND ""
  )
endif ()

################################################################################
## ZLIB
################################################################################

if (UNIX)
  ExternalProject_Add(zlib_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib-1.2.7
    BUILD_IN_SOURCE 1
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zlib
    CONFIGURE_COMMAND CC=${CMAKE_C_COMPILER} ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/zlib --static
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
  )  
endif ()

import_target(zlib zlib_build
  "${CMAKE_CURRENT_SOURCE_DIR}/libev"
  "${CMAKE_CURRENT_SOURCE_DIR}/libev/.libs/libev.a"
)

################################################################################
## EV
################################################################################

if (UNIX)
  ExternalProject_Add(ev_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libev
    BUILD_IN_SOURCE 1
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libev
    CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/libev --enable-static --disable-shared ${THIRDP_BUILDFLAGS}
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
  )  
endif ()

import_target(ev ev_build
  "${CMAKE_CURRENT_SOURCE_DIR}/libev"
  "${CMAKE_CURRENT_SOURCE_DIR}/libev/.libs/libev.a"
)

################################################################################
## V8 and ICU
################################################################################

set(V8_DIR "${CMAKE_CURRENT_SOURCE_DIR}/V8-4.3.61")
# doesn't work?
set(V8_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/v8")
#set(V8_TARGET_DIR "out")


if ("${HOST_BITS}" STREQUAL "64")
  set(TARGET_ARCH "x64")
else()
  message(ERROR "WHAT THE FUCK!")
  set(TARGET_ARCH "ia32")
endif() # TODO: ARM? Win32?

if(DEBUG)
  list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
  set(V8_TARGET_ARCH "${TARGET_ARCH}.debug")
else()
  set(V8_TARGET_ARCH "${TARGET_ARCH}.release")
endif()


list(APPEND V8_GYP_ARGS ${V8_DIR}/build/gyp/gyp_main.py --generator-output=${V8_TARGET_DIR} build/all.gyp)

  
if(WIN32)
  # TODO: get this from the main configure paramentrisation
  # CMAKE_BUILD_TYPE:STRING=RelWithDebInfo
  # -G\ msvs_version=2013
  #  -Dtarget_arch=x64
  # -Dmode=release
  list(APPEND V8_GYP_ARGS -Dcomponent=static_library -Dlibrary=static_library)
else()
  # -Dwerror='' breaks shit... rather...   -Dwerror= 
  list(APPEND V8_GYP_ARGS -Ibuild/standalone.gypi --depth=. -Dstandalone_static_library=1  -Dlinux_use_bundled_gold=0 -Dlinux_use_gold_flags=0-Dv8_no_strict_aliasing=1)
endif()


list(APPEND V8_GYP_ARGS -Dv8_target_arch=${TARGET_ARCH} -S.${V8_TARGET_ARCH})

if (UNIX)
  #set(V8_PLATFORM "native")
  set(V8_BIN_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
  set(ICU_BIN_DIR "${V8_BIN_DIR}")
  if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR (CMAKE_SYSTEM_NAME MATCHES "Solaris.*") OR (CMAKE_SYSTEM_NAME MATCHES "SunOS.*"))
    set(V8_BIN_DIR "${V8_BIN_DIR}/obj.target/tools/gyp")
    set(ICU_BIN_DIR "${ICU_BIN_DIR}/obj.target/third_party/icu")
  endif ()
else ()
  message(ERROR "Not implemented for your OS")
endif ()



include(ExternalProject)
set (V8_BUILD_COMMAND $(MAKE) -f Makefile-v8 strictaliasing=off
  snapshot=off werror=no "${V8_TARGET_ARCH}" CC="${CMAKE_C_COMPILER}"
  CXX="${CMAKE_CXX_COMPILER}"
  CXXFLAGS=${CMAKE_CXX_FLAGS})
if (APPLE AND CMAKE_COMPILER_IS_CLANG)
  set(V8_BUILD_COMMAND ${V8_BUILD_COMMAND} LDFLAGS=-stdlib=libc++ CXXFLAGS=-stdlib=libc++)
endif ()

set (V8_COMPILE_ARGS -C ${V8_TARGET_DIR} -f Makefile.${V8_TARGET_ARCH} BUILDTYPE=Release builddir=${V8_TARGET_DIR}/${V8_TARGET_ARCH} VERBOSE=1 V=1 CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})

#set(RUN_V8_GYP "PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE};\ ${PYTHON_EXECUTABLE}")
set(RUN_V8_GYP "${PYTHON_EXECUTABLE}")

#--------------------------------------------------------------------------------
get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach ()
#--------------------------------------------------------------------------------

if (NOT USE_PRECOMPILED_V8)
  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE TRUE
    SOURCE_DIR "${V8_DIR}"
    CONFIGURE_COMMAND ${RUN_V8_GYP} ${V8_GYP_ARGS}
    BUILD_COMMAND ${MAKE} ${V8_COMPILE_ARGS}
    INSTALL_COMMAND "")
endif ()

import_target(v8_base v8_build
  "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_base.a"
)

import_target(v8_libbase v8_build
  "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_libbase.a"
)

import_target(v8_libplatform v8_build
  "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_libplatform.a"
)

import_target(v8_nosnapshot v8_build
  "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_nosnapshot.a"
)

import_target(v8_snapshot v8_build
  "${V8_DIR}/include"
  "${V8_BIN_DIR}/libv8_snapshot.a"
)

import_target(icu_uc v8_build
  "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
  "${ICU_BIN_DIR}/libicuuc.a"
)

import_target(icu_data v8_build
  "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
  "${ICU_BIN_DIR}/libicudata.a"
)

import_target(icu_i18n v8_build
  "${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/io;${V8_DIR}/third_party/icu/source/i18n"
  "${ICU_BIN_DIR}/libicui18n.a"
)
