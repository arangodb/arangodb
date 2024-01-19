# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.21)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

project(V8)

set(CMAKE_CXX_STANDARD 20)

set(V8_DIR "${PROJECT_SOURCE_DIR}/${V8_SUB_DIR}")
set(V8_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/${V8_SUB_DIR}")
set(GYP_FILES_DIR "${PROJECT_SOURCE_DIR}/gypfiles")

################################################################################
## ICU EXPORTS
################################################################################

set(ICU_COMPACT_VERSION
  "73"
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


set(V8_RELATIVE_SOURCE_DIR "../${V8_SUB_DIR}")

# create target directory
file(MAKE_DIRECTORY ${V8_TARGET_DIR})

# detect endianess, and translate to gyp/v8 proper string
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)

if (${IS_BIGENDIAN})
  set(ENDIANESS 'big')
else()
  set(ENDIANESS 'little')
endif()

# detect 32bit or 64bit and target architecture
if("${CMAKE_TARGET_ARCHITECTURES}" STREQUAL "aarch64")
  set(V8_PROC_ARCH "arm64")
  list(APPEND V8_GYP_ARGS -Darm_fpu=default -Darm_float_abi=default)
else ()
  if ("${BITS}" STREQUAL "64")
    set(V8_PROC_ARCH "x64")
  else ()
    set(V8_PROC_ARCH "ia32")
  endif ()
endif()


file(RELATIVE_PATH V8_REL_TARGET_PATH ${V8_DIR} ${V8_TARGET_DIR})

list(APPEND V8_GYP_ARGS
  --generator-output=${V8_TARGET_DIR}
  -DV8_ROOT=${V8_RELATIVE_SOURCE_DIR}
  -DABSL_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/3rdParty/abseil-cpp
  -Dicu_path=${V8_RELATIVE_SOURCE_DIR}/third_party/icu/
  -Dv8_target_arch=${V8_PROC_ARCH}
  -Dtarget_arch=${V8_PROC_ARCH}
  -Dv8_host_arch=${V8_PROC_ARCH}
  -Dhost_arch=${V8_PROC_ARCH}
  -Dbyteorder=${ENDIANESS}
  -Dv8_enable_i18n_support=1
  -DV8_SRC_ROOT=${GYP_FILES_DIR}/
)

option(USE_DEBUG_V8 "compile V8 in DEBUG mode" OFF)
  
set(GYP_MAIN ${PROJECT_SOURCE_DIR}/gyp/gyp_main.py)

################################################################################
## WINDOWS with msbuild; Gyp now does TrackFileAccess too.
################################################################################

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
  elseif (${CMAKE_GENERATOR} MATCHES "2022")
    # try to find 2022 in 'Visual Studio 17 2022 Win64'
    set(GYP_MSVS_VERSION "2022")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2022")
  else ()
    set(GYP_MSVS_VERSION "2017")
    MESSAGE(STATUS "setting GYP_MSVS_VERSION to 2017 by default")
  endif()

  list(APPEND V8_GYP_ARGS "-Dmsvs_cygwin_shell=0")

  if ("${BITS}" STREQUAL "64")
    set(MS_TARGET_PLATFORM x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
  endif ()

  list(APPEND V8_GYP_ARGS -Dtarget_arch=${V8_PROC_ARCH} -Dhost_arch=${V8_PROC_ARCH})
  list(APPEND V8_GYP_ARGS ../gypfiles/v8.gyp)

  list(APPEND V8_TARGET_ARCHS "Release")
  
  if (USE_DEBUG_V8)
    list(APPEND V8_TARGET_ARCHS "Debug")
  endif()
  
  set(GYP_GENERATORS "msvs")
  set(GYP_MSVS_OVERRIDE_PATH "${CMAKE_CXX_COMPILER}")

  list(APPEND V8_GYP_ARGS
    -Dgenerator_output=${V8_TARGET_DIR}
    -Ggenerator_output=${V8_TARGET_DIR}
    -DINSPECTOR_ROOT=${V8_DIR}
    --depth=.
    # whether or not to use dynamic runtime libraries
    -Dforce_dynamic_crt=0
    # false on Windows, otherwise sets some compiler options for RTTI etc. 
    # when building ICU.
    -Dos_posix=0
    # don't fail build upon compile warnings
    -Dwerror=
    -Dv8_embed_script=
    -Dembedded_builtins_snapshot_src=
    # disable zapping of v8 Handles. unsure what it is exactly, but obviously
    # we don't rely on this for any functionality.
    -Dv8_enable_handle_zapping=0
    # we don't want to rely on zlib in the V8 build because it could cause
    # ambiguities with our own bundled version of zlib. V8 only needs zlib
    # for the serialization of some debug/inspector data, which we do not
    # support anyway
    -Dv8_use_zlib=0
    # web assembly turned off to reduce the feature scope, and because we
    # have seen crashes inside some web assembly execution code
    -Dv8_enable_webassembly=0
    # allow for some optimizations in debug builds (we build in release mode
    # by default). otherwise v8 becomes unbearably slow in debug mode
    -Dv8_optimized_debug=1
    # allow to load ICU data dynamically, so that data doesn't have to be
    # compiled into every executable.
    -Dicu_use_data_file_flag=1
    # don't spend time on link-time optimization
    -Denable_lto=0
    # the following two are important so that we build static libraries
    -Dcomponent=static_library
    -Dlibrary=static_library
    # build full ICU data file
    -Dicu_small=false
    # version number is included in some file paths
    -Dicu_ver_major=73
    -Dicu_data_in=${CMAKE_BINARY_DIR}/bin/icudt73l.dat
    -Dicu_path=${V8_RELATIVE_SOURCE_DIR}/third_party/icu/
    # little endian
    -Dicu_endianness=l
    -Dpython=${PYTHON_EXECUTABLE}
  )

  list(APPEND V8_TARGETS v8_libbase v8_libplatform v8_snapshot)

  add_custom_target(v8_build)
  set(V8_LIBS
    "torque_base;v8_base_without_compiler;v8_compiler;v8_init;v8_initializers;v8_turboshaft;v8_libbase;v8_libplatform;v8_snapshot"
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
        ${CMAKE_COMMAND} -E env GYP_MSVS_VERSION=${GYP_MSVS_VERSION} GYP_GENERATORS=${GYP_GENERATORS} GYP_MSVS_OVERRIDE_PATH=${GYP_MSVS_OVERRIDE_PATH} "${PYTHON_EXECUTABLE}" ${GYP_MAIN} ${V8_GYP_ARGS} -Dmode=${MS_TARGET_CONF_LOWER} 
      BUILD_COMMAND
        ""
      INSTALL_COMMAND
        ""
    )

    add_dependencies(v8_build v8_build_${MS_TARGET_CONF_LOWER})

    foreach (V8_ONE_TARGET ${V8_TARGETS})
      # cmake eats semicolons or quotes them or, or, or so we can't use /t:a;b;c
      ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_${V8_ONE_TARGET}_${MS_TARGET_CONF_LOWER}
        WORKING_DIRECTORY
          ${V8_TARGET_DIR}/../gypfiles
        COMMAND
          msbuild ${V8_ONE_TARGET}.vcxproj /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
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

    debug "${ICU_LIBRARY_DEBUG}/lib/icuucx.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icuucx.lib"
    
    debug "${ICU_LIBRARY_DEBUG}/lib/icustubdata.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icustubdata.lib"

    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
  )
  
  set(V8_LIBRARY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/gypfiles/Debug")
  set(V8_LIBRARY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/gypfiles/Release")

  message("V8_LIBRARY_DEBUG ${V8_LIBRARY_DEBUG} CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}")
  set(V8_LIBS
    debug "${V8_LIBRARY_DEBUG}/lib/v8_libplatform.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libplatform.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_base_without_compiler.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base_without_compiler.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_libbase.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_libbase.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/torque_base.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/torque_base.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_compiler.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_compiler.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_initializers.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_initializers.lib"
    
    debug "${V8_LIBRARY_DEBUG}/lib/v8_init.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_init.lib"

    debug "${V8_LIBRARY_DEBUG}/lib/v8_snapshot.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_snapshot.lib"
    
    debug "${V8_LIBRARY_DEBUG}/lib/v8_turboshaft.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_turboshaft.lib"

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

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(V8_CFLAGS   "${CMAKE_C_FLAGS}   ${CMAKE_C_FLAGS_DEBUG}")
    set(V8_CXXFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
  elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(V8_CFLAGS   "${CMAKE_C_FLAGS}   ${CMAKE_C_FLAGS_MINSIZEREL}")
    set(V8_CXXFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}")
  elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(V8_CFLAGS   "${CMAKE_C_FLAGS}   ${CMAKE_C_FLAGS_RELEASE}")
    set(V8_CXXFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
  elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(V8_CFLAGS   "${CMAKE_C_FLAGS}   ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    set(V8_CXXFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endif ()
  set(V8_CFLAGS     "${V8_CFLAGS}   $ENV{V8_CFLAGS}")
  # note: c++17 is required here to make ASan/TSan builds still pass
  set(V8_CXXFLAGS   "${V8_CXXFLAGS} $ENV{V8_CXXFLAGS} -std=c++17")

  # TODO: find a better solution than forcefully removing all sanitizer
  # options.
  if (TRUE)
    # remove sanitizer options. if these options are not removed, linking
    # will currently fail.
    string(REGEX REPLACE "-f(no-)?sanitize=[^ ]+" "" V8_CFLAGS ${V8_CFLAGS})
    string(REGEX REPLACE "-f(no-)?sanitize=[^ ]+" "" V8_CXXFLAGS ${V8_CXXFLAGS})
    string(REGEX REPLACE "-f(no-)?sanitize-[^ ]+" "" V8_CFLAGS ${V8_CFLAGS})
    string(REGEX REPLACE "-f(no-)?sanitize-[^ ]+" "" V8_CXXFLAGS ${V8_CXXFLAGS})
  endif ()
  # remove -Werror from compile options if it is set, we do not want to turn
  # any warning in the compilation of V8 into an error
  string(REGEX REPLACE "-Werror( |$)" "" V8_CFLAGS ${V8_CXXFLAGS})
  string(REGEX REPLACE "-Werror( |$)" "" V8_CXXFLAGS ${V8_CXXFLAGS})

  set(V8_LDFLAGS    "${CMAKE_EXE_LINKER_FLAGS} $ENV{V8_LDFLAGS}")

  # for V8 we minimal debug information is more than enough
  set(V8_CFLAGS   "${V8_CFLAGS} -g1 -gno-column-info -gz")
  set(V8_CXXFLAGS "${V8_CXXFLAGS} -g1 -gno-column-info -gz")
  
  add_c_flags_if_supported(V8_CFLAGS -Wno-suggest-override -Wno-implicit-const-int-float-conversion -Wno-non-virtual-dtor -Wno-final-dtor-non-final-class -Wno-implicit-const-int-float-conversion)
  add_cxx_flags_if_supported(V8_CXXFLAGS -Wno-suggest-override -Wno-implicit-const-int-float-conversion -Wno-non-virtual-dtor -Wno-final-dtor-non-final-class -Wno-implicit-const-int-float-conversion)

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

  list(APPEND V8_GYP_ARGS ../gypfiles/v8.gyp)
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
    -DINSPECTOR_ROOT=${V8_RELATIVE_SOURCE_DIR}
    # needed to make things compile fine. c++20 did not succeed initially.
    # may need to be revisited later
    -Dclang_cxx_language_standard=c++17
    -Dmac_deployment_target=${CMAKE_OSX_DEPLOYMENT_TARGET}
    --depth=.
    # whether or not to use dynamic runtime libraries. only used on Windows
    -Dforce_dynamic_crt=0
    # true here, sets some compiler options for RTTI etc. when building ICU
    -Dos_posix=1
    # build a standalone static library. used for some gyp generators (make),
    # but not on Windows
    -Dstandalone_static_library=1
    # sets -fno-strict-aliasing. unclear if still needed
    -Dv8_no_strict_aliasing=1
    # don't fail build upon compile warnings
    -Dwerror=
    # unclear if this is still needed
    -Dclang=0
    -Dv8_embed_script=
    -Dembedded_builtins_snapshot_src=
    # disable zapping of v8 Handles. unsure what it is exactly, but obviously
    # we don't rely on this for any functionality.
    -Dv8_enable_handle_zapping=0
    # we don't want to rely on zlib in the V8 build because it could cause
    # ambiguities with our own bundled version of zlib. V8 only needs zlib
    # for the serialization of some debug/inspector data, which we do not
    # support anyway
    -Dv8_use_zlib=0
    # web assembly turned off to reduce the feature scope, and because we
    # have seen crashes inside some web assembly execution code
    -Dv8_enable_webassembly=0
    # allow for some optimizations in debug builds (we build in release mode
    # by default). otherwise v8 becomes unbearably slow in debug mode
    -Dv8_optimized_debug=1
    # allow to load ICU data dynamically, so that data doesn't have to be
    # compiled into every executable.
    -Dicu_use_data_file_flag=1
    # don't spend time on link-time optimization
    -Denable_lto=0
    # the following two are important so that we build static libraries
    -Dcomponent=static_library
    -Dlibrary=static_library
    # build full ICU data file
    -Dicu_small=false
    # version number is included in some file paths
    -Dicu_ver_major=73
    -Dicu_data_in=${CMAKE_BINARY_DIR}/bin/icudt73l.dat
    -Dicu_path=${V8_RELATIVE_SOURCE_DIR}/third_party/icu/
    # little endian
    -Dicu_endianness=l
    -Dpython=${PYTHON_EXECUTABLE}
  )

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

  # only here for debugging. enable when needed
  # message("V8 cc: ${CMAKE_C_COMPILER}")
  # message("V8 cxx: ${CMAKE_CXX_COMPILER}")
  # message("V8 gyp args: ${V8_GYP_ARGS}")
  # message("V8 cflags: ${V8_CFLAGS}")
  # message("V8 cxxflags: ${V8_CXXFLAGS}")
  # message("V8 compile args: ${V8_COMPILE_ARGS}")
  # message("V8 ldflags: ${V8_LDFLAGS}")

  list(APPEND V8_COMPILE_TARGETS icui18n icuuc icustubdata v8_libbase v8_libplatform v8_snapshot)
  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE
      TRUE
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env CFLAGS=${V8_CFLAGS} CXXFLAGS=${V8_CXXFLAGS} cflags_c=${V8_CFLAGS} cflags_cc=${V8_CXXFLAGS} "${PYTHON_EXECUTABLE}" ${GYP_MAIN} ${V8_GYP_ARGS} 
    BUILD_COMMAND
      $(MAKE) ${V8_COMPILE_TARGETS} ${V8_COMPILE_ARGS}
    STEP_TARGETS
      icui18n icuuc v8_libbase v8_libplatform v8_snapshot
    INSTALL_COMMAND
      ""
  )

  list(APPEND LINK_DIRECTORIES "${V8_LIBRARY_DIR}")

  set(V8_LIBS
    "torque_base;v8_base_without_compiler;v8_compiler;v8_init;v8_initializers;v8_turboshaft;v8_libbase;v8_libplatform;v8_snapshot"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  list(APPEND LINK_DIRECTORIES "${ICU_LIBRARY_DIR}")

  set(ICU_LIBS
    "icui18n;icuucx;icustubdata"
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
