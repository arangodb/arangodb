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
  set(ENDIANESS 'big')
else()
  set(ENDIANESS 'little')
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

file(RELATIVE_PATH V8_REL_TARGET_PATH ${V8_DIR} ${V8_TARGET_DIR})

list(APPEND V8_GYP_ARGS
  --generator-output=${V8_TARGET_DIR}
  #$ ${V8_REL_TARGET_PATH}
  -Dv8_target_arch=${V8_PROC_ARCH}
  -Dv8_host_arch=${V8_PROC_ARCH}
  -Dbyteorder=${ENDIANESS}
  # -Dicu_use_data_file_flag=1 not supportet anymore..
  -Dv8_enable_i18n_support=1
)

if (CROSS_COMPILING)
  list(APPEND V8_GYP_ARGS
  -Dhost_arch=${V8_PROC_ARCH}
  -DGYP_CROSSCOMPILE=1
  -DEXECUTABLE_PREFIX=${V8_PROC_ARCH}.
  )
  configure_file (
    "${CMAKE_SOURCE_DIR}/lib/V8/v8-mkpeephole.in"
    "${V8_TARGET_DIR}/${V8_PROC_ARCH}.release/${V8_PROC_ARCH}.mkpeephole"
    NEWLINE_STYLE UNIX
    @ONLY
  )
  configure_file (
    "${CMAKE_SOURCE_DIR}/lib/V8/v8-mksnapshot.in"
    "${V8_TARGET_DIR}/${V8_PROC_ARCH}.release/${V8_PROC_ARCH}.mksnapshot"
    NEWLINE_STYLE UNIX
    @ONLY
  )
endif()

################################################################################
## ICU EXPORTS
################################################################################

set(ICU_COMPACT_VERSION
  "54"
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
  ${V8_DIR}/include;${V8_DIR}
  CACHE INTERNAL
  "${PROJECT_NAME}: include path"
)



################################################################################
## WINDOWS & NINJA
################################################################################
if (WIN32 AND ${CMAKE_GENERATOR} MATCHES "Ninja")
  message(STATUS "using generator NINJA on Windows! Still WIP be patient!")
  set(GYP_MAIN ${V8_DIR}/gypfiles/gyp_v8)

  # try to find 2013 in 'Visual Studio 12 2013 Win64'
  #if (${CMAKE_GENERATOR} MATCHES "2013") 
  #  list(APPEND V8_GYP_ARGS "-G msvs_version=2013")
  #endif()

  # try to find 2015 in 'Visual Studio 14 2015 Win64'
  #if (${CMAKE_GENERATOR} MATCHES "2015") 
    #    list(APPEND V8_GYP_ARGS "-G msvs_version=2015")
  list(APPEND V8_GYP_ARGS --format ninja)
  #endif()

  if ("${BITS}" STREQUAL "64")
    set(MS_TARGET_PLATFORM x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
  endif ()

  list(APPEND V8_GYP_ARGS -Dtarget_arch=${V8_PROC_ARCH} -Dhost_arch=${V8_PROC_ARCH})
  list(APPEND V8_GYP_ARGS ${V8_DIR}/gypfiles/all.gyp)

  if ("${V8_TARGET_ARCHS}" STREQUAL "")
    set(V8_TARGET_ARCHS "Debug;Release" CACHE string "V8 targets to build")
  endif()
  list(APPEND V8_GYP_ARGS
    -Dcomponent=static_library
    -Dlibrary=static_library
    #--depth=2
    #-Dv8_use_external_startup_data=1
    #-Dicu_gyp_path=../third_party/icu/icu.gyp
    #-Dv8_optimized_debug=0
    #-Dos_posix=0
    #-Dclang=0
    #-Dwant_separate_host_toolset=0
  )

  add_custom_target(v8_build_ninja)

  set(NINJA_BUILD_DIR "${V8_TARGET_DIR}/${NINJA_TARGET}")

  ExternalProject_Add(v8_build_ninja
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BINARY_DIR
      ${NINJA_BUILD_DIR}
    BUILD_COMMAND
      ${CMAKE_BUILD_TOOL} v8
    STEP_TARGETS
      v8_base v8_libplatform v8_nosnapshot v8_libsampler v8_libbase icui18n icuuc
    INSTALL_COMMAND
      "")

  list(APPEND LINK_DIRECTORIES "${NINJA_BUILD_DIR}/obj.target/src/")

  set(V8_LIBS
    "v8_base;v8_libplatform;v8_nosnapshot;v8_libsampler;v8_libbase"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  list(APPEND LINK_DIRECTORIES "${NINJA_BUILD_DIR}/obj/third_party/icu")

  set(ICU_LIBS
    "icui18n;icuuc"
    CACHE INTERNAL
    "ICU: Libraries"
  )

  set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)

elseif (${CMAKE_GENERATOR} MATCHES "Ninja")
  message(STATUS "using generator NINJA!")

  option(USE_DEBUG_V8 "compile V8 in DEBUG mode" OFF)

  if (USE_DEBUG_V8)
    set(NINJA_TARGET "optDebug") # one of Debug|Release|Optdebug
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
  else ()
    set(NINJA_TARGET "Release") # one of Debug|Release|Optdebug
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
  endif ()

  if (USE_DEBUG_V8)
    list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
  endif ()

  set(V8_LIBRARY_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
  set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}")

  set(GYP_MAIN "${V8_DIR}/gypfiles/gyp_v8")

  list(APPEND V8_GYP_ARGS ${V8_DIR}/gypfiles/all.gyp)
  list(APPEND V8_GYP_ARGS --format ninja)
  list(APPEND V8_GYP_ARGS -S.${V8_TARGET_ARCH})

  if (LINUX)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/obj.target/src")
    set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.target/third_party/icu")
  elseif (SOLARIS)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/obj.host/src")
    set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.host/third_party/icu")
  endif ()

  # Gyp wants to use its own clang if we don't set this:
  list(APPEND V8_GYP_ARGS
    -Dclang=0)

  list(APPEND V8_GYP_ARGS
    -I${V8_DIR}/gypfiles/standalone.gypi
    --depth=.
    -Dstandalone_static_library=1
    -Dlinux_use_bundled_gold=0
    -Dlinux_use_gold_flags=0
    -Dv8_no_strict_aliasing=1
    -Dwerror=
  )

  set(NINJA_BUILD_DIR "${V8_TARGET_DIR}/${NINJA_TARGET}")

  ExternalProject_Add(v8_build
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BINARY_DIR
      ${NINJA_BUILD_DIR}
    BUILD_COMMAND
      ${CMAKE_BUILD_TOOL} v8
    STEP_TARGETS
      v8_base v8_libplatform v8_nosnapshot v8_libsampler v8_libbase icui18n icuuc
    INSTALL_COMMAND
      "")

  list(APPEND LINK_DIRECTORIES "${NINJA_BUILD_DIR}/obj/src")

  set(V8_LIBS
    "v8_base;v8_libplatform;v8_nosnapshot;v8_libsampler;v8_libbase"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  list(APPEND LINK_DIRECTORIES "${NINJA_BUILD_DIR}/obj/third_party/icu")

  set(ICU_LIBS
    "icui18n;icuuc"
    CACHE INTERNAL
    "ICU: Libraries"
  )

  set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)

################################################################################
## WINDOWS with msbuild
################################################################################

elseif (WIN32)
  MESSAGE(STATUS "Building V8 for Windows with MSBUILD")
  set(GYP_MAIN gypfiles/gyp_v8)

  # try to find 2013 in 'Visual Studio 12 2013 Win64'
  if (${CMAKE_GENERATOR} MATCHES "2013") 
    list(APPEND V8_GYP_ARGS "-Gmsvs_version=2013")
  endif()

  # try to find 2015 in 'Visual Studio 14 2015 Win64'
  if (${CMAKE_GENERATOR} MATCHES "2015") 
    list(APPEND V8_GYP_ARGS "-Gmsvs_version=2015")
  endif()
  
  list(APPEND V8_GYP_ARGS "-GGYP_GENERATORS=msvs")
  list(APPEND V8_GYP_ARGS "-Dmsvs_cygwin_shell=0")
  list(APPEND V8_GYP_ARGS "-Dv8_use_snapshot=false")
  
  if ("${BITS}" STREQUAL "64")
    set(MS_TARGET_PLATFORM x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
  endif ()

  list(APPEND V8_GYP_ARGS -Dtarget_arch=${V8_PROC_ARCH} -Dhost_arch=${V8_PROC_ARCH})
  list(APPEND V8_GYP_ARGS gypfiles/all.gyp)

  if ("${V8_TARGET_ARCHS}" STREQUAL "")
    set(V8_TARGET_ARCHS "Debug;Release" CACHE string "V8 targets to build")
  endif()
  set(ENV{GYP_GENERATORS} msvs)
  set(ENV{GYP_MSVS_VERSION} 2015)
  set(ENV{DEPOT_TOOLS_WIN_TOOLCHAIN} 0)
  list(APPEND V8_GYP_ARGS
    -Dcomponent=static_library
    -Dlibrary=static_library
    -Dgenerator_output=${V8_TARGET_DIR}
    -Ggenerator_output=${V8_TARGET_DIR}
    #--depth=2
    #-Dv8_use_external_startup_data=1
    #-Dicu_gyp_path=../third_party/icu/icu.gyp
    #-Dv8_optimized_debug=0
    #-Dos_posix=0
    #-Dclang=0
    #-Dwant_separate_host_toolset=0
  )

  add_custom_target(v8_build)

  foreach (MS_TARGET_CONF ${V8_TARGET_ARCHS})
    string(TOLOWER ${MS_TARGET_CONF} MS_TARGET_CONF_LOWER)

    ExternalProject_Add(v8_build_${MS_TARGET_CONF_LOWER}
      BUILD_IN_SOURCE
        TRUE
      WORKING_DIRECTORY
        ${V8_DIR}
      SOURCE_DIR
        "${V8_DIR}"
      CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env GYP_GENERATORS=msvs GYP_MSVS_VERSION=2015 DEPOT_TOOLS_WIN_TOOLCHAIN=0 ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS} -Dmode=${MS_TARGET_CONF_LOWER}
      BUILD_COMMAND
        ""
      INSTALL_COMMAND
        ""
    )

    add_dependencies(v8_build v8_build_${MS_TARGET_CONF_LOWER})

    # and now ICU...

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} icu_build_icui18n
      WORKING_DIRECTORY
        ${V8_TARGET_DIR}/third_party/icu
      COMMAND
        msbuild icu.sln /t:icui18n /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} icu_build_icuuc
      WORKING_DIRECTORY
        ${V8_TARGET_DIR}/third_party/icu
      COMMAND
        msbuild icu.sln /t:icuuc /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    # cmake eats semicolons or quotes them or, or, or so we can't use /t:a;b;c
    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8
      WORKING_DIRECTORY
        ${V8_TARGET_DIR}/gypfiles
      COMMAND
        msbuild all.sln /t:v8 /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8_libbase
      WORKING_DIRECTORY
        ${V8_TARGET_DIR}/gypfiles
      COMMAND
        msbuild all.sln /t:v8_libbase /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8_libplatform
      WORKING_DIRECTORY
        ${V8_TARGET_DIR}/gypfiles
      COMMAND
        msbuild all.sln /t:v8_libplatform /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8_libsampler
      WORKING_DIRECTORY
        ${V8_TARGET_DIR}/gypfiles
      COMMAND
        msbuild all.sln /t:v8_libsampler /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )
  endforeach ()

  set(ICU_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}}/../build/Debug")
  set(ICU_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}}/../build/Release")

  set(ICU_LIBS
    debug "${ICU_LIBRARY_DEBUG}/lib/icui18n.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icui18n.lib"

    debug "${ICU_LIBRARY_DEBUG}/lib/icuuc.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icuuc.lib"

    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
    )
  set(V8_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/build/Debug")
  set(V8_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/build/Release")

  set(V8_LIBS
    debug "${V8_LIBRARY_DEBUG}/lib/v8_base_0.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_0.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_base_1.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_1.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_base_2.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_2.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_base_3.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_3.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_libplatform.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libplatform.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_nosnapshot.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_nosnapshot.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_libsampler.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libsampler.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_libbase.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libbase.lib"

    debug "DbgHelp.lib"
    optimized "DbgHelp.lib"

    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

else ()

################################################################################
## UNIX
################################################################################

  option(USE_DEBUG_V8 "compile V8 in DEBUG mode" OFF)

  set(V8_CFLAGS   "$ENV{V8_CFLAGS}")
  set(V8_CXXFLAGS "$ENV{V8_CXXFLAGS}")
  set(V8_LDFLAGS  "$ENV{V8_LDFLAGS}")

  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(V8_CFLAGS   "${V8_CFLAGS} -fno-delete-null-pointer-checks")
    set(V8_CXXFLAGS "${V8_CXXFLAGS} -fno-delete-null-pointer-checks")
  endif()

  if (USE_DEBUG_V8)
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
  else ()
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
  endif ()

  if (USE_DEBUG_V8)
    list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
  endif ()

  set(V8_LIBRARY_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
  set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}")

  set(GYP_MAIN ${V8_DIR}/tools/gyp/gyp_main.py)

  list(APPEND V8_GYP_ARGS gypfiles/all.gyp)
  list(APPEND V8_GYP_ARGS --format make)
  list(APPEND V8_GYP_ARGS -S.${V8_TARGET_ARCH})

  if (LINUX)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/obj.target/src")
    set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.target/third_party/icu")
  elseif (SOLARIS)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/obj.host/src")
    set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.host/third_party/icu")
  endif ()

  list(APPEND V8_GYP_ARGS
    -Igypfiles/standalone.gypi
    -Dmac_deployment_target=${CMAKE_OSX_DEPLOYMENT_TARGET}
    --depth=.
    -Dstandalone_static_library=1
    -Dlinux_use_bundled_gold=0
    -Dlinux_use_gold_flags=0
    -Dv8_no_strict_aliasing=1
    -Dwerror=
    -Dv8_use_snapshot=true )

  if (APPLE AND CMAKE_COMPILER_IS_CLANG)
    set(V8_CXXFLAGS "${V8_CXXFLAGS} -stdlib=libc++")
    set(V8_LDFLAGS  "${V8_LDFLAGS} -stdlib=libc++")
  endif ()

  if (USE_DEBUG_V8)
    set(V8_BUILDTYPE Debug)
  else ()
    set(V8_BUILDTYPE Release)
  endif ()

  set(V8_COMPILE_ARGS
    -C ${V8_TARGET_DIR}
    -f Makefile.${V8_TARGET_ARCH}
    BUILDTYPE=${V8_BUILDTYPE}
    builddir=${V8_TARGET_DIR}/${V8_TARGET_ARCH}
    CC=${CMAKE_C_COMPILER}
    CXX=${CMAKE_CXX_COMPILER}
    CFLAGS=${V8_CFLAGS}
    CXXFLAGS=${V8_CXXFLAGS}
    LDFLAGS=${V8_LDFLAGS}
  )

  list(APPEND V8_COMPILE_ARGS icui18n icuuc)
  list(APPEND V8_COMPILE_ARGS v8)

  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE
      TRUE
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BUILD_COMMAND
      $(MAKE) ${V8_COMPILE_ARGS}
    STEP_TARGETS
      v8_base v8_libplatform v8_nosnapshot v8_libsampler v8_libbase icui18n icuuc
    INSTALL_COMMAND
      "")

  list(APPEND LINK_DIRECTORIES "${V8_LIBRARY_DIR}")

  set(V8_LIBS
    "v8_base;v8_libplatform;v8_nosnapshot;v8_libsampler;v8_libbase"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  list(APPEND LINK_DIRECTORIES "${ICU_LIBRARY_DIR}")

  set(ICU_LIBS
    "icui18n;icuuc"
    CACHE INTERNAL
    "ICU: Libraries"
  )

  set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
endif()

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
