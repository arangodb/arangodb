# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.0)

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
file(MAKE_DIRECTORY ${V8_TARGET_DIR})

# Detect endianess, and translate to gyp/v8 proper string.
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)
if (${IS_BIGENDIAN})
  set(ENDIANESS 'big')
else()
  set(ENDIANESS 'little')
endif()

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
    --generator-output=${V8_TARGET_DIR}
    -Dv8_target_arch=${V8_PROC_ARCH}
    -Dbyteorder=${ENDIANESS}
    -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
)

if (DEBUG)
#  list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
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
  )


if (WIN32)
  set(GYP_MAIN ${V8_DIR}/build/gyp_v8)
  # try to find 2013 in 'Visual Studio 12 2013 Win64'
  if (${CMAKE_GENERATOR} MATCHES "2013") 
    list(APPEND V8_GYP_ARGS -G\ msvs_version=2013)
  endif()
  if (${CMAKE_GENERATOR} MATCHES "Win64")
    set(MS_TARGET_PLATFORM x64)
    list(APPEND V8_GYP_ARGS -Dtarget_arch=x64 -Dhost_arch=x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
    list(APPEND V8_GYP_ARGS -Dtarget_arch=ia32 -Dhost_arch=ia32)
  endif()
  list(APPEND V8_GYP_ARGS ${V8_DIR}/build/all.gyp)
  
      # TODO: get this from the main configure paramentrisation
  # CMAKE_BUILD_TYPE:STRING=RelWithDebInfo
  list(APPEND V8_GYP_ARGS
    -Dcomponent=static_library
    -Dlibrary=static_library
    -Dmode=${WIN_GYP_RELEASE_TYPE}
    #--depth=2
    #-Dv8_use_external_startup_data=1
    #-Dicu_gyp_path=../third_party/icu/icu.gyp
    #-Dv8_optimized_debug=0
    #-Dos_posix=0
    #-Dclang=0
    #-Dwant_separate_host_toolset=0
    #-Dwant_separate_host_toolset=0
    )
  set(MSBUILD TRUE)    
else ()
  set(GYP_MAIN ${V8_DIR}/build/gyp/gyp_main.py)

  list(APPEND V8_GYP_ARGS build/all.gyp)
  list(APPEND V8_GYP_ARGS --format make)
  list(APPEND V8_GYP_ARGS -S.${V8_TARGET_ARCH})

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

get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach ()
#--------------------------------------------------------------------------------
MESSAGE(${V8_GYP_ARGS})
if (MSBUILD)
  set(V8_TARGETS "v8\;v8_libbase\;v8_libplatform")
  set(ICU_TARGETS "icudata\;icui18n\;icuuc")
  message("TARGETS: ${V8_TARGETS}")
  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE TRUE
    WORKING_DIRECTORY ${V8_DIR}/build
    SOURCE_DIR "${V8_DIR}"
    CONFIGURE_COMMAND ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BUILD_COMMAND ""
    INSTALL_COMMAND "")


  # and now ICU...
  ExternalProject_Add_Step(v8_build icu_build_icudata
    WORKING_DIRECTORY ${V8_DIR}/third_party/icu/
    COMMAND msbuild icu.sln /t:icudata /p:Configuration=Debug /p:Platform=${MS_TARGET_PLATFORM}
    DEPENDEES configure
    DEPENDERS build)

  ExternalProject_Add_Step(v8_build icu_build_icui18n
    WORKING_DIRECTORY ${V8_DIR}/third_party/icu/
    COMMAND msbuild icu.sln /t:icui18n /p:Configuration=Debug /p:Platform=${MS_TARGET_PLATFORM}
    DEPENDEES configure
    DEPENDERS build)

  ExternalProject_Add_Step(v8_build icu_build_icuuc
    WORKING_DIRECTORY ${V8_DIR}/third_party/icu/
    COMMAND msbuild icu.sln /t:icuuc /p:Configuration=Debug /p:Platform=${MS_TARGET_PLATFORM}
    DEPENDEES configure
    DEPENDERS build)

  # cmake eats semicolons or quotes them or, or, or so we can't use /t:a;b;c
  ExternalProject_Add_Step(v8_build v8_build_v8
    WORKING_DIRECTORY ${V8_DIR}/build
    COMMAND msbuild all.sln /t:v8 /p:Configuration=Debug /p:Platform=${MS_TARGET_PLATFORM}
    DEPENDEES configure
    DEPENDERS build)

  ExternalProject_Add_Step(v8_build v8_build_v8_libbase
    WORKING_DIRECTORY ${V8_DIR}/build
    COMMAND msbuild all.sln /t:v8_libbase /p:Configuration=Debug /p:Platform=${MS_TARGET_PLATFORM}
    DEPENDEES configure
    DEPENDERS build)

  ExternalProject_Add_Step(v8_build v8_build_v8_libplatform
    WORKING_DIRECTORY ${V8_DIR}/build
    COMMAND msbuild all.sln /t:v8_libplatform /p:Configuration=Debug /p:Platform=${MS_TARGET_PLATFORM}
    DEPENDEES configure
    DEPENDERS build)

else()
  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE TRUE
    SOURCE_DIR "${V8_DIR}"
    CONFIGURE_COMMAND ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BUILD_COMMAND $(MAKE) ${V8_COMPILE_ARGS}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND "")
endif()
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

if (MSBUILD)
  set(ICU_BIN_DIR "${V8_DIR}/build/${CMAKE_BUILD_TYPE_INIT}/")
  set(ICU_LIBS
    "${ICU_BIN_DIR}/lib/icui18n.lib;${ICU_BIN_DIR}/lib/icuuc.lib"
    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
    )
else()
  set(ICU_LIBS
    "${ICU_BIN_DIR}/libicui18n.a;${ICU_BIN_DIR}/libicuuc.a;${ICU_BIN_DIR}/libicudata.a"
    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
    )
endif()
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

if (MSBUILD)
  set(V8_BIN_DIR "${V8_DIR}/build/${CMAKE_BUILD_TYPE_INIT}/")
  set(V8_LIBS
    "${V8_BIN_DIR}/lib/v8_base.lib;${V8_BIN_DIR}/lib/v8_libbase.lib;${V8_BIN_DIR}/lib/v8_libplatform.lib;${V8_BIN_DIR}/lib/v8_nosnapshot.lib"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
    FORCE
    )
else()
  set(V8_LIBS
    "${V8_BIN_DIR}/libv8_base.a;${V8_BIN_DIR}/libv8_libbase.a;${V8_BIN_DIR}/libv8_libplatform.a;${V8_BIN_DIR}/libv8_nosnapshot.a"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
    FORCE
    )
endif()
