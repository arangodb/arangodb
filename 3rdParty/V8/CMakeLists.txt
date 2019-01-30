# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

project(V8)

# create target directory

set(V8_DIR "${PROJECT_SOURCE_DIR}/${V8_SUB_DIR}")
set(V8_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/${V8_SUB_DIR}")

file(MAKE_DIRECTORY ${V8_TARGET_DIR})

# detect endianess, and translate to gyp/v8 proper string

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)

if (${IS_BIGENDIAN})
  set(ENDIANESS big)
else()
  set(ENDIANESS little)
endif()

set(GN gn CACHE PATH "where to find generate-Ninja")
#v8_imminent_deprecation_warnings
#v8_use_snapshot
list(APPEND GN_ARGS_LIST
  is_component_build=false
  v8_monolithic=true
  v8_use_external_startup_data=false
  v8_enable_i18n_support=true
  use_custom_libcxx=false
  is_debug=false
# is_debug=true
 v8_enable_v8_checks=true
#   host_byteorder=${ENDIANESS}
  )

if (WIN32)
  list(APPEND GN_ARGS_LIST
    is_clang=true
    "clang_base_path=\"C:/Program Files/LLVM\""
    "visual_studio_version=2017"
#    "visual_studio_path=\"${CMAKE_GENERATOR_INSTANCE}\""
    "visual_studio_path=\"${MSVC_REDIST_DIR}../../../\""
    "wdk_path=\"${MSVC_REDIST_DIR}\""
    "visual_studio_runtime_dirs=\"${MSVC_REDIST_DIR}\""
    )
else ()
  list(APPEND GN_ARGS_LIST
    is_clang=false
    use_sysroot=false
    )
endif()
# detect 32bit or 64bit

if ("${CMAKE_TARGET_ARCHITECTURES}" STREQUAL "armv7")
  set(V8_PROC_ARCH "arm")
  set(QEMU_ARCH "qemu-arm")
  list(APPEND V8_GYP_ARGS -Darm_version=7 -Darm_fpu=default -Darm_float_abi=default)
elseif("${CMAKE_TARGET_ARCHITECTURES}" STREQUAL "aarch64")
  set(V8_PROC_ARCH "arm64")
  set(QEMU_ARCH "qemu-aarch64")
  list(APPEND V8_GYP_ARGS -Darm_fpu=default -Darm_float_abi=default)
else ()
  if ("${BITS}" STREQUAL "64")
    set(V8_PROC_ARCH "x64")
  else ()
    set(V8_PROC_ARCH "ia32")
  endif ()
endif()


# target architecture

if (CROSS_COMPILING)
  list(APPEND V8_GYP_ARGS
  -Dhost_arch=${V8_PROC_ARCH}
  -DGYP_CROSSCOMPILE=1
  -DEXECUTABLE_PREFIX=${V8_PROC_ARCH}.
  )
endif()

################################################################################
## ICU EXPORTS
################################################################################

set(ICU_COMPACT_VERSION
  "63"
  CACHE INTERNAL
  "ICU: compact Version"
)

set(ICU_VERSION
  "${ICU_COMPACT_VERSION}.1"
  CACHE INTERNAL
  "ICU: Version"
  )

set(ICU_BASEDIR ${V8_DIR}/third_party/icu)

set(ICU_INCLUDE_DIR
  ${ICU_BASEDIR}/source/common;${ICU_BASEDIR}/source/i18n;${ICU_BASEDIR}/source/io
  CACHE INTERNAL
  "ICU: include path"
  FORCE
)

set(V8_VERSION
  ${V8_VERSION}
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
)

set(V8_INCLUDE_DIR
  "${V8_DIR}/include;${V8_DIR};${V8_TARGET_DIR}/Release/gen"
  CACHE INTERNAL
  "${PROJECT_NAME}: include path"
)


option(USE_DEBUG_V8 "compile V8 in DEBUG mode" OFF)

if (USE_DEBUG_V8)
  set(NINJA_TARGET "optDebug") # one of Debug|Release|Optdebug
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
  list(APPEND GN_ARGS_LIST v8_enable_backtrace=true)
else ()
  set(NINJA_TARGET "Release") # one of Debug|Release|Optdebug
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
endif ()

set(V8_LIBRARY_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(V8_CFLAGS   "${V8_CFLAGS} -fno-delete-null-pointer-checks")
  set(V8_CXXFLAGS "${V8_CXXFLAGS} -fno-delete-null-pointer-checks")
endif()

if (WIN32)
  if ("${BITS}" STREQUAL "64")
    set(MS_TARGET_PLATFORM x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
  endif ()
endif()

set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/Release/obj/")
set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/Release/obj/third_party/icu")

if (APPLE AND CMAKE_COMPILER_IS_CLANG)
  set(V8_CXXFLAGS "${V8_CXXFLAGS} -stdlib=libc++")
  set(V8_LDFLAGS  "${V8_LDFLAGS} -stdlib=libc++")
endif ()

# list(APPEND GN_ARGS_LIST
if (${CMAKE_GENERATOR} MATCHES "Ninja")
  set(NINJA ${CMAKE_BUILD_TOOL})
else()
  set(NINJA "ninja") #lean on path for now. TODO
endif()
set(NINJA_BUILD_DIR "${V8_TARGET_DIR}/${NINJA_TARGET}")

set(V8_LIBS
  "v8_monolith"
  CACHE INTERNAL
  "${PROJECT_NAME}: Libraries"
  )

list(JOIN GN_ARGS_LIST " " GN_ARGS)
message("GN_ARGS ${GN_ARGS}")
ExternalProject_Add(v8_build
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${GN} gen ${NINJA_BUILD_DIR} "--root=${V8_DIR}" "--args=${GN_ARGS}"
    BINARY_DIR
      ${NINJA_BUILD_DIR}
    BUILD_COMMAND
      ${NINJA} ${V8_LIBS}
    STEP_TARGETS
      ${V8_LIBS} icui18n icuuc
    INSTALL_COMMAND
      "")

list(APPEND LINK_DIRECTORIES "${NINJA_BUILD_DIR}/obj/")


list(APPEND LINK_DIRECTORIES "${NINJA_BUILD_DIR}/obj/third_party/icu")

set(ICU_LIBS
  "icui18n;icuuc"
  CACHE INTERNAL
  "ICU: Libraries"
  )

set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
set(ICU_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/../build/Debug")
set(ICU_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/../build/Release")

#set(ICU_LIBS
#  debug "${ICU_LIBRARY_DEBUG}/lib/icui18n.lib"
#  optimized "${ICU_LIBRARY_RELEASE}/lib/icui18n.lib"
#
#  debug "${ICU_LIBRARY_DEBUG}/lib/icuuc.lib"
#  optimized "${ICU_LIBRARY_RELEASE}/lib/icuuc.lib"
#
#  CACHE INTERNAL
#  "ICU: Libraries"
#  FORCE
#  )
#set(V8_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/build/Debug")
#set(V8_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/build/Release")
#
#set(V8_LIBS
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_base_0.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_0.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_base_1.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_1.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_base_2.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_2.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_base_3.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_3.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_libplatform.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_libplatform.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_nosnapshot.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_nosnapshot.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_libsampler.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_libsampler.lib"
#
#  debug "${V8_LIBRARY_DEBUG}/lib/v8_libbase.lib"
#  optimized "${V8_LIBRARY_RELEASE}/lib/v8_libbase.lib"
#
#  debug "DbgHelp.lib"
#  optimized "DbgHelp.lib"
#
#  CACHE INTERNAL
#  "${PROJECT_NAME}: Libraries"
#  )



if (USE_DEBUG_V8)
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
else ()
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
endif ()

set(V8_LIBRARY_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}")

set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/obj.target/src")
set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.target/third_party/icu")


if (USE_DEBUG_V8)
  set(V8_BUILDTYPE Debug)
else ()
  set(V8_BUILDTYPE Release)
endif ()

list(APPEND LINK_DIRECTORIES "${V8_LIBRARY_DIR}")

list(APPEND LINK_DIRECTORIES "${ICU_LIBRARY_DIR}")

set(ICU_LIBS
  "icui18n;icuuc"
  CACHE INTERNAL
  "ICU: Libraries"
  )

set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)

set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}" PARENT_SCOPE)
set(ICU_DT "${ICU_BASEDIR}/common/icudtl.dat")
set(ICU_DT ${ICU_DT} PARENT_SCOPE)
set(ICU_DT_DEST "icudtl.dat" )
set(ICU_DT_DEST ${ICU_DT_DEST} PARENT_SCOPE)


if (MSVC)
  add_custom_command(
    TARGET v8_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ICU_DT} ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/${ICU_DT_DEST})

  add_custom_command(
    TARGET v8_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ICU_DT} ${CMAKE_BINARY_DIR}/tests/$<CONFIG>/${ICU_DT_DEST})
else()
  add_custom_command(
    TARGET v8_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ICU_DT} ${CMAKE_BINARY_DIR}/bin/${ICU_DT_DEST})

  add_custom_command(
    TARGET v8_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ICU_DT} ${CMAKE_BINARY_DIR}/tests/${ICU_DT_DEST})
endif()
if (NOT WIN32)
  add_custom_target(nonthinV8
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../Installation/archive-de-thinner.sh ${V8_REL_TARGET_PATH}
    COMMENT de-thins V8 .a files)
endif()
