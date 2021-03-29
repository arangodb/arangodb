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
set(GYP_FILES_DIR "${PROJECT_SOURCE_DIR}/gypfiles")

################################################################################
## ICU EXPORTS
################################################################################

set(ICU_COMPACT_VERSION
  "64.2"
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


SET(V8_RELATIVE_SOURCE_DIR "../${V8_SUB_DIR}")
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
  -DV8_ROOT=${V8_RELATIVE_SOURCE_DIR}
  #$ ${V8_REL_TARGET_PATH}
  -Dv8_target_arch=${V8_PROC_ARCH}
  -Dv8_host_arch=${V8_PROC_ARCH}
  -Dbyteorder=${ENDIANESS}
  -Dv8_enable_i18n_support=1
  -DV8_SRC_ROOT=${GYP_FILES_DIR}/
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

option(USE_DEBUG_V8 "compile V8 in DEBUG mode" OFF)
  
################################################################################
## WINDOWS with msbuild; Gyp now does TrackFileAccess too.
################################################################################

set(GYP_MAIN ${PROJECT_SOURCE_DIR}/gyp/gyp_main.py)

if (WIN32)
  MESSAGE(STATUS "Building V8 for Windows with MSBUILD")

  if (USE_CLCACHE_MODE)
    list(APPEND V8_GYP_ARGS "-Duse_clcache_mode=true")
  else ()
    list(APPEND V8_GYP_ARGS "-Duse_clcache_mode=false")
  endif()

  if (${CMAKE_GENERATOR} MATCHES "2013")
    # try to find 2013 in 'Visual Studio 12 2013 Win64'
    set(GYP_MSVS_VERSION "2013")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2013")
  elseif (${CMAKE_GENERATOR} MATCHES "2015")
    # try to find 2015 in 'Visual Studio 14 2015 Win64'
    set(GYP_MSVS_VERSION "2015")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2015")
  elseif (${CMAKE_GENERATOR} MATCHES "2017")
    # try to find 2017 in 'Visual Studio 15 2017 Win64'
    set(GYP_MSVS_VERSION "2017")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2017")
  elseif (${CMAKE_GENERATOR} MATCHES "2019")
    # try to find 2019 in 'Visual Studio 16 2019 Win64'
    set(GYP_MSVS_VERSION "2019")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2019")
  else ()
    set(GYP_MSVS_VERSION "2017")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2017 by default")
  endif()

  list(APPEND V8_GYP_ARGS "-Dmsvs_cygwin_shell=0")
  list(APPEND V8_GYP_ARGS "-Dv8_use_snapshot=true")

  if ("${BITS}" STREQUAL "64")
    set(MS_TARGET_PLATFORM x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
  endif ()

  list(APPEND V8_GYP_ARGS -Dtarget_arch=${V8_PROC_ARCH} -Dhost_arch=${V8_PROC_ARCH})
  list(APPEND V8_GYP_ARGS ../gypfiles/all.gyp)

  list(APPEND V8_TARGET_ARCHS "Release")
  
  if (USE_DEBUG_V8)
    list(APPEND V8_TARGET_ARCHS "Debug")
  endif()
  
  set(GYP_GENERATORS "msvs")
  set(DEPOT_TOOLS_WIN_TOOLCHAIN "0")
  set(GYP_MSVS_OVERRIDE_PATH "${CMAKE_CXX_COMPILER}")

  list(APPEND V8_GYP_ARGS
    -I../gypfiles/standalone.gypi
    -Dcomponent=static_library
    -Dlibrary=static_library
    -Dgenerator_output=${V8_TARGET_DIR}
    -Ggenerator_output=${V8_TARGET_DIR}
    --depth=.
    -Dwerror=
    -Dembed_script=
    -Dwarmup_script=
    -Dv8_enable_embedded_builtins=1
    -Dembedded_builtins_snapshot_src=
    -Dv8_enable_handle_zapping=0
    -Dv8_use_snapshot=1
    -DPYTHON_EXECUTABLE="${PYTHON_EXECUTABLE}"
    -Dnode_use_pch=false
    )

  list(APPEND V8_TARGETS
    torque
    run_torque
    generate_bytecode_builtins_list
    v8_libsampler
    v8_initializers
    v8_libplatform
    v8_nosnapshot
    mksnapshot
    v8_snapshot
    v8
    )

  add_custom_target(v8_build)
  set(V8_LIBS
    "v8_base_without_compiler;v8_compiler;v8_base_without_compiler;torque_base;v8_libplatform;v8_libsampler;v8_libbase;v8_init;v8_initializers;v8_nosnapshot"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  foreach (MS_TARGET_CONF ${V8_TARGET_ARCHS})
    string(TOLOWER ${MS_TARGET_CONF} MS_TARGET_CONF_LOWER)
    ExternalProject_Add(v8_build_${MS_TARGET_CONF_LOWER}
      BUILD_IN_SOURCE
      TRUE
      WORKING_DIRECTORY
      ${V8_DIR}/gypfiles
      SOURCE_DIR
      "${V8_DIR}"
      CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env GYP_MSVS_VERSION=${GYP_MSVS_VERSION} GYP_GENERATORS=${GYP_GENERATORS} GYP_MSVS_OVERRIDE_PATH=${GYP_MSVS_OVERRIDE_PATH} DEPOT_TOOLS_WIN_TOOLCHAIN=${DEPOT_TOOLS_WIN_TOOLCHAIN} "${PYTHON_EXECUTABLE}" ${GYP_MAIN} ${V8_GYP_ARGS} -Dmode=${MS_TARGET_CONF_LOWER}
      BUILD_COMMAND
      ""
      INSTALL_COMMAND
        ""
      )

    add_dependencies(v8_build v8_build_${MS_TARGET_CONF_LOWER})

    foreach(V8_ONE_TARGET ${V8_TARGETS})
      # cmake eats semicolons or quotes them or, or, or so we can't use /t:a;b;c
      ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_${V8_ONE_TARGET}_${MS_TARGET_CONF_LOWER}
        WORKING_DIRECTORY
          ${V8_TARGET_DIR}/../gypfiles
        COMMAND
          msbuild ${V8_ONE_TARGET}.vcxproj /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
#          msbuild All.vcxproj /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
        DEPENDEES
          configure
        DEPENDERS
          build
        )
    endforeach ()
  endforeach ()

  set(ICU_LIBRARY_DEBUG   "${CMAKE_CURRENT_BINARY_DIR}/${V8_SUB_DIR}/third_party/icu/Debug")
  set(ICU_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/${V8_SUB_DIR}/third_party/icu/Release")

  set(ICU_LIBS
    debug "${ICU_LIBRARY_DEBUG}/lib/icui18n.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icui18n.lib"

    debug "${ICU_LIBRARY_DEBUG}/lib/icuuc.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icuuc.lib"

    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
    )

  
  set(V8_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/gypfiles/Debug")
  set(V8_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/gypfiles//Release")


  message("V8_LIBRARY_DEBUG ${V8_LIBRARY_DEBUG} CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}")
#v8_libplatform.lib
#v8_base_without_compiler.lib
#v8_libbase.lib
#v8_libsampler.lib
#v8_compiler.lib
#v8_snapshot.lib
#v8_initializers.lib

  set(V8_LIBS
    debug "${V8_LIBRARY_DEBUG}/lib/v8_libplatform.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libplatform.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_base_without_compiler.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_without_compiler.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_libbase.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libbase.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_libsampler.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libsampler.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/torque_base.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/torque_base.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_compiler.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_compiler.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_initializers.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_initializers.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_snapshot.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_snapshot.lib"

#    debug "${V8_LIBRARY_DEBUG}/lib/v8_init.lib"
#    optimized "${V8_LIBRARY_RELEASE}/lib/v8_init.lib"

    debug "DbgHelp.lib"
    optimized "DbgHelp.lib"

    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
    )
  set(V8_INCLUDE_DIR
    debug "${V8_DIR}/include;${V8_DIR};${V8_LIBRARY_DEBUG}/obj/global_intermediate/"
    optimized "${V8_DIR}/include;${V8_DIR};${V8_LIBRARY_RELEASE}/obj/global_intermediate/"
    CACHE INTERNAL
    "${PROJECT_NAME}: include path"
    )
  set(V8_INTERNAL_INCLUDE_DIR
    debug "${V8_DIR}/src/builtins;${V8_LIBRARY_DEBUG}/obj/global_intermediate/generate-bytecode-output-root;${V8_LIBRARY_DEBUG}/obj/global_intermediate/torque-output-root"
    optimized "${V8_DIR}/src/builtins;${V8_LIBRARY_RELEASE}/obj/global_intermediate/generate-bytecode-output-root;${V8_LIBRARY_RELEASE}/obj/global_intermediate/torque-output-root"
    CACHE INTERNAL
    "${PROJECT_NAME}: V8 private include path"
    )

  set(V8_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/gypfiles/Debug")
  set(V8_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/gypfiles//Release")

else ()


################################################################################
## UNIX
################################################################################

  set(V8_CFLAGS   "$ENV{V8_CFLAGS}")
  set(V8_CXXFLAGS "$ENV{V8_CXXFLAGS}")
  set(V8_LDFLAGS  "$ENV{V8_LDFLAGS}")

  # build V8 without debug information
  if (USE_MINIMAL_DEBUGINFO)
    set(V8_CFLAGS   "${V8_CFLAGS} -g0")
    set(V8_CXXFLAGS "${V8_CXXFLAGS} -g0")
  endif()

  list(APPEND V8_GYP_ARGS "-Dv8_use_snapshot=true")

  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(V8_CFLAGS   "${V8_CFLAGS} -fno-delete-null-pointer-checks")
    set(V8_CXXFLAGS "${V8_CXXFLAGS} -fno-delete-null-pointer-checks")
  endif()
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set(V8_CFLAGS "${V8_CFLAGS} -Wno-suggest-override -Wno-implicit-const-int-float-conversion -Wno-non-virtual-dtor -Wno-final-dtor-non-final-class -Wno-implicit-const-int-float-conversion")
      set(V8_CXXFLAGS "${V8_CXXFLAGS} -Wno-suggest-override -Wno-implicit-const-int-float-conversion -Wno-non-virtual-dtor -Wno-final-dtor-non-final-class -Wno-implicit-const-int-float-conversion")
  endif()

  if (USE_DEBUG_V8)
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
  else ()
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
  endif ()

  if (USE_DEBUG_V8)
    list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
  endif ()

  set(V8_LIBRARY_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}/")
  set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}")

  list(APPEND V8_GYP_ARGS ../gypfiles/all.gyp)
  list(APPEND V8_GYP_ARGS --format make)
  list(APPEND V8_GYP_ARGS -S.${V8_TARGET_ARCH})

  set(V8_INTERNAL_INCLUDE_DIR "${V8_LIBRARY_DIR}/obj/gen/generate-bytecode-output-root;${V8_LIBRARY_DIR}/obj/gen/torque-output-root")
  if (LINUX)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/gypfiles")
    set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.target/third_party/icu")
  elseif (APPLE)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/")
    set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}/")
  endif ()

  list(APPEND V8_GYP_ARGS
    -I../gypfiles/standalone.gypi
    -Dmac_deployment_target=${CMAKE_OSX_DEPLOYMENT_TARGET}
    --depth=.
    -Dstandalone_static_library=1
    -Dlinux_use_bundled_gold=0
    -Dlinux_use_gold_flags=0
    -Dv8_no_strict_aliasing=1
    -Dwerror=
    -Dclang=0
    -Dhost_clang=
    -Dv8_embed_script=
    -Dembed_script=
    -Dwarmup_script=
    -Dv8_enable_embedded_builtins=0
    -Dembedded_builtins_snapshot_src=
    -Dv8_enable_handle_zapping=0
    -Dv8_use_snapshot=1 )

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

  list(APPEND V8_COMPILE_TARGETS icui18n icuuc)
  list(APPEND V8_COMPILE_TARGETS v8_libbase
    v8_libplatform
    v8_base
    v8_libsampler
    v8_init
    v8_initializers
    v8_nosnapshot
    mksnapshot
    v8_snapshot
    )
message(${V8_GYP_ARGS})
  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE
      TRUE
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BUILD_COMMAND
      $(MAKE) ${V8_COMPILE_TARGETS} ${V8_COMPILE_ARGS}
    STEP_TARGETS
      v8_libbase v8_libplatform icui18n icuuc v8_base v8_libsampler v8_init v8_initializers v8_nosnapshot mksnapshot v8_snapshot
    INSTALL_COMMAND
      "")

  list(APPEND LINK_DIRECTORIES "${V8_LIBRARY_DIR}")

  set(V8_LIBS
    "v8_base_without_compiler;v8_compiler;v8_base_without_compiler;torque_base;v8_libplatform;v8_libsampler;v8_libbase;v8_init;;v8_initializers;v8_snapshot"
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
  set(V8_INCLUDE_DIR
    "${V8_DIR}/include;${V8_DIR};${V8_TARGET_DIR}/${V8_TARGET_ARCH}/obj/gen"
    CACHE INTERNAL
    "${PROJECT_NAME}: include path"
    )
endif()

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


set(V8_INCLUDE_DIR ${V8_INCLUDE_DIR} PARENT_SCOPE)
set(V8_INTERNAL_INCLUDE_DIR ${V8_INTERNAL_INCLUDE_DIR} PARENT_SCOPE)
