# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# General
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.2)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

if (POLICY CMP0037)
  cmake_policy(SET CMP0037 NEW)
endif ()

if (POLICY CMP0017)
  cmake_policy(SET CMP0017 NEW)
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release
      CACHE string
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE
  )
endif ()

if (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"
      OR CMAKE_BUILD_TYPE STREQUAL "Release"
      OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"
      OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"
      OR CMAKE_BUILD_TYPE STREQUAL "None"))

  message(FATAL_ERROR "expecting CMAKE_BUILD_TYPE: None Debug Release RelWithDebInfo MinSizeRel, got ${CMAKE_BUILD_TYPE}.")
endif ()

string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UPPER)

if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "deployment target for MacOSX; adjust to your sysem")
endif ()

# required for clang completion in editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# where to find CMAKE modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

option(SKIP_PACKAGING "" OFF)

# be verbose about flags used
option(VERBOSE "be verbose about flags used" OFF)

# use standalone boost asio
add_definitions("-DARANGODB_STANDALONE_ASIO=1")
add_definitions("-DFUERTE_STANDALONE_ASIO=1")

# ------------------------------------------------------------------------------
# VERSION information
# ------------------------------------------------------------------------------

# stable release:   MAJOR.MINOR.PATCH
# hot fix:          MAJOR.MINOR.PATCH-FIXNUMBER
# unstable release: MAJOR.MINOR.PATCH-TYPE.NUMBER
# devel:            MAJOR.MINOR.devel
#
# These are mapped to the following variables:
#
# ARANGODB_VERSION_MAJOR = MAJOR
# ARANGODB_VERSION_MINOR = MINOR
# ARANGODB_VERSION_PATCH = PATCH
#
# for pre-releases, otherwise empty:
#
# ARANGODB_VERSION_PRELEASE_TYPE   = TYPE
# ARANGODB_VERSION_PRELEASE_NUMBER = NUMBER
#
# This will set the following variables
#
# ARANGODB_DEBIAN_UPSTREAM
# ARANGODB_DEBIAN_REVISION
#
# ARANGODB_RPM_UPSTREAM
# ARANGODB_RPM_REVISION

set(ARANGODB_VERSION_MAJOR "3")
set(ARANGODB_VERSION_MINOR "4")

# when building the nightly ARANGODB_VERSION_PATCH will be set
if (NOT DEFINED ARANGODB_VERSION_PATCH)
  set(ARANGODB_VERSION_PATCH "8")
  set(ARANGODB_VERSION_RELEASE_TYPE "preview")
  set(ARANGODB_VERSION_RELEASE_NUMBER "1")
else()
  unset (ARANGODB_VERSION_RELEASE_TYPE) # do not remove space
  unset (ARANGODB_VERSION_RELEASE_NUMBER) # do not remove space
endif()

# unset TYPE and NUMBER in case they are empty
if (DEFINED ARANGODB_VERSION_RELEASE_TYPE)
  if (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "")
    unset (ARANGODB_VERSION_RELEASE_TYPE) # do not remove space
    unset (ARANGODB_VERSION_RELEASE_NUMBER) # do not remove space
  endif()
else()
  unset (ARANGODB_VERSION_RELEASE_NUMBER) # do not remove space
endif()

if (DEFINED ARANGODB_VERSION_RELEASE_NUMBER)
  if (ARANGODB_VERSION_RELEASE_NUMBER STREQUAL "")
    unset (ARANGODB_VERSION_RELEASE_NUMBER) # do not remove space
  endif()
endif()

# semantic version
set(ARANGODB_PLAIN_VERSION "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.${ARANGODB_VERSION_PATCH}")

if (DEFINED ARANGODB_VERSION_RELEASE_TYPE)
  if (DEFINED ARANGODB_VERSION_RELEASE_NUMBER)
    set(ARANGODB_VERSION "${ARANGODB_PLAIN_VERSION}-${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}")
  else()
    set(ARANGODB_VERSION "${ARANGODB_PLAIN_VERSION}-${ARANGODB_VERSION_RELEASE_TYPE}")
  endif()
else()
  set(ARANGODB_VERSION "${ARANGODB_PLAIN_VERSION}")
endif()
set(ARANGODB_JS_VERSION "js")

message(STATUS "ARANGODB PLAIN VERSION: ${ARANGODB_PLAIN_VERSION}")
message(STATUS "ARANGODB VERSION: ${ARANGODB_VERSION}")
message(STATUS "ARANGODB JS VERSION: ${ARANGODB_JS_VERSION}")

################################################################################
# DEBIAN, RPM, MacOS version
################################################################################

if (DEFINED ARANGODB_VERSION_RELEASE_TYPE)

  # unstable version
  if (   ARANGODB_VERSION_RELEASE_TYPE STREQUAL "alpha"
      OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "beta"
      OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "milestone"
      OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "preview"
      OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "rc")

    if (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "alpha")
      set(ARANGODB_RPM_REVISION_NUMBER 100)
    elseif (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "beta")
      set(ARANGODB_RPM_REVISION_NUMBER 200)
    elseif (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "milestone")
      set(ARANGODB_RPM_REVISION_NUMBER 300)
    elseif (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "preview")
      set(ARANGODB_RPM_REVISION_NUMBER 400)
    elseif (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "rc")
      set(ARANGODB_RPM_REVISION_NUMBER 500)
    endif()

    MATH(EXPR ARANGODB_RPM_REVISION_NUMBER "${ARANGODB_RPM_REVISION_NUMBER}+${ARANGODB_VERSION_RELEASE_NUMBER}")

    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}~${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_RPM_REVISION "0.${ARANGODB_RPM_REVISION_NUMBER}")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DARWIN_REVISION "${ARANGODB_VERSION_RELEASE_TYPE}.${ARANGODB_VERSION_RELEASE_NUMBER}")

  # hot-fix
  else()
    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}.${ARANGODB_VERSION_RELEASE_TYPE}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_RPM_REVISION "1.${ARANGODB_VERSION_RELEASE_TYPE}")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}.${ARANGODB_VERSION_RELEASE_TYPE}")
    set(ARANGODB_DARWIN_REVISION "")
  endif()
else()

  # devel
  if (ARANGODB_VERSION_PATCH STREQUAL "devel")
    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.0~~${ARANGODB_VERSION_PATCH}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.0")
    set(ARANGODB_RPM_REVISION "0.1")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DARWIN_REVISION "")

  # nightly
  elseif (ARANGODB_VERSION_PATCH STREQUAL "nightly")
    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.0")
    set(ARANGODB_RPM_REVISION "0.2")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DARWIN_REVISION "")

  # stable version
  else()
    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_RPM_REVISION "1.0")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DARWIN_REVISION "")
  endif()
endif()

message(STATUS "DEBIAN VERSION: ${ARANGODB_DEBIAN_UPSTREAM} / ${ARANGODB_DEBIAN_REVISION}")
message(STATUS "RPM VERSION:    ${ARANGODB_RPM_UPSTREAM} / ${ARANGODB_RPM_REVISION}")
message(STATUS "DARWIN VERSION: ${ARANGODB_DARWIN_UPSTREAM} / ${ARANGODB_DARWIN_REVISION}")


################################################################################
# Windows version
################################################################################

# PATCH needs to be numeric for the windows rc file
set(ARANGODB_WINDOWS_UPSTREAM "${ARANGODB_VERSION}")

if (ARANGODB_VERSION_PATCH GREATER -1)
  set(ARANGODB_WINDOWS_PATCH "${ARANGODB_VERSION_PATCH}")
else()
  set(ARANGODB_WINDOWS_PATCH 1337)
endif()

message(STATUS "WINDOWS VERSION: ${ARANGODB_WINDOWS_UPSTREAM}")

################################################################################
# SNAP version
################################################################################

set(ARANGODB_SNAP_REVISION "1")

message(STATUS "SNAP REVISION: ${ARANGODB_SNAP_REVISION}")

# ------------------------------------------------------------------------------
# 
# ------------------------------------------------------------------------------

if (WIN32)
  # path containing the OpenSSL find-module and libraries
  set(WIN_FIND_OPENSSL_PATH "C:/OpenSSL-ArangoDB")
  if (EXISTS "${WIN_FIND_OPENSSL_PATH}")
	list(APPEND CMAKE_MODULE_PATH ${WIN_FIND_OPENSSL_PATH})
  endif()
  project(arangodb3 LANGUAGES CXX C VERSION ${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR})
else ()
  project(arangodb3 LANGUAGES CXX C ASM VERSION ${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR})
endif ()

# Static executables:
option(STATIC_EXECUTABLES "produce static executables" OFF)
if (STATIC_EXECUTABLES)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# enable enterprise features
set(ENTERPRISE_INCLUDE_DIR "enterprise")
option(USE_ENTERPRISE "enable enterprise build" OFF)

if (USE_ENTERPRISE)
  add_definitions("-DUSE_ENTERPRISE=1")
  add_subdirectory(enterprise)
endif ()

option(DEBUG_SYNC_REPLICATION "Enable debug code to enable synchronous replication debugging" OFF)
if (DEBUG_SYNC_REPLICATION)
  add_definitions("-DDEBUG_SYNC_REPLICATION=1")
endif ()

# for the packages
set(ARANGODB_PACKAGE_VENDOR  "ArangoDB GmbH")
set(ARANGODB_PACKAGE_CONTACT "info@arangodb.com")
set(ARANGODB_DISPLAY_NAME    "ArangoDB")
set(ARANGODB_URL_INFO_ABOUT  "https://www.arangodb.com")
set(ARANGODB_HELP_LINK       "https://docs.arangodb.com/${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}/")
set(ARANGODB_CONTACT         "hackers@arangodb.com")
set(ARANGODB_FRIENDLY_STRING "ArangoDB - the native multi-model NoSQL database")

# MSVC
set(ARANGO_BENCH_FRIENDLY_STRING   "arangobench - stress test program")
set(ARANGO_DUMP_FRIENDLY_STRING    "arangodump - export")
set(ARANGO_RESTORE_FRIENDLY_STRING "arangrestore - importer")
set(ARANGO_EXPORT_FRIENDLY_STRING  "arangoexport - datae xporter")
set(ARANGO_IMPORT_FRIENDLY_STRING  "arangoimport - TSV/CSV/JSON importer")
set(ARANGOSH_FRIENDLY_STRING       "arangosh - commandline client")
set(ARANGO_VPACK_FRIENDLY_STRING   "arangovpack - vpack printer")

# libraries
set(LIB_ARANGO        arango)
set(LIB_ARANGO_GEO    arango_geo)
set(LIB_ARANGO_V8     arango_v8)

# binaries
set(BIN_ARANGOBENCH   arangobench)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOEXPORT  arangoexport)
set(BIN_ARANGOIMPORT  arangoimport)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)
set(BIN_ARANGOVPACK   arangovpack)

# test binaries
set(TEST_BASICS_SUITE basics_suite)
set(TEST_CACHE_SUITE  cache_suite)
set(TEST_GEO_SUITE    geo_suite)
set(CLEAN_AUTOGENERATED_FILES)
set(PACKAGES_LIST)
set(COPY_PACKAGES_LIST)
set(CLEAN_PACKAGES_LIST)
set(INSTALL_CONFIGFILES_LIST)

# ------------------------------------------------------------------------------
# update files containing VERSION information
# ------------------------------------------------------------------------------

if (${CMAKE_MAJOR_VERSION} EQUAL 2)
  set(ARANGODB_BUILD_DATE "YYYY-MM-DD HH:MM:SS")
else ()
  string(TIMESTAMP ARANGODB_BUILD_DATE "%Y-%m-%d %H:%M:%S")
endif ()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build.h"
  NEWLINE_STYLE UNIX
)

if (NOT DEFINED GENERATE_BUILD_DATE OR GENERATE_BUILD_DATE)
  set(GENERATE_BUILD_DATE ON CACHE INTERNAL "whether we should generate the build date")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build-date.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build-date.h"
    NEWLINE_STYLE UNIX
    )
else()
  set(GENERATE_BUILD_DATE OFF CACHE INTERNAL "whether we should generate the build date")
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/VERSION.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
  NEWLINE_STYLE UNIX
)

################################################################################
## Find the git revision
################################################################################

find_program (GIT_EXE git)
if (DEFINED GIT_EXE AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}

    COMMAND ${GIT_EXE} describe --all --tags --long --dirty=-dirty
    OUTPUT_VARIABLE GIT_OUTPUT)

  # this may fail on shallow clones that only knows about a limited number of commits.
  # if there is an older merged revision the head, it may not be available to git.
  if (NOT GIT_OUTPUT)
      set(ARANGODB_BUILD_REPOSITORY "GIT FAILED TO RETRIEVE THE VERSION - SHALLOW CLONE?")
      set(HAVE_ARANGODB_BUILD_REPOSITORY "1")
    else()
      string(STRIP ${GIT_OUTPUT} REPOSITORY_VERSION)
      set(ARANGODB_BUILD_REPOSITORY ${REPOSITORY_VERSION})
      set(HAVE_ARANGODB_BUILD_REPOSITORY "1")
    endif()
else ()
  set(ARANGODB_BUILD_REPOSITORY "")
  set(HAVE_ARANGODB_BUILD_REPOSITORY "0")
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build-repository.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build-repository.h"
  NEWLINE_STYLE UNIX
)

if (VERBOSE)
  message(STATUS "ARANGODB_BUILD_REPOSITORY=\"${ARANGODB_BUILD_REPOSITORY}\"")
endif ()

################################################################################
## OPERATION SYSTEM
################################################################################

option(HOMEBREW
  "whether to install for homebrew"
  Off
)

if (WIN32)
  set(WINDOWS TRUE)
  set(MSBUILD TRUE)
  add_definitions(-D_USE_MATH_DEFINES -DNOMINMAX -D_WIN32_WINNT=_WIN32_WINNT_VISTA -DINCL_EXTRA_HTON_FUNCTIONS -DPSAPI_VERSION=1)
elseif (UNIX AND NOT APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(LINUX TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*")
    set(FREEBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
    set(NETBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(OPENBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES ".*GNU.*")
    set(GNU TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES ".*BSDI.*")
    set(BSDI TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "DragonFly.*|FreeBSD")
    set(FREEBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "SYSV5.*")
    set(SYSV5 TRUE)
  elseif ((CMAKE_SYSTEM_NAME MATCHES "Solaris.*") OR (CMAKE_SYSTEM_NAME MATCHES "SunOS.*"))
    set(SOLARIS TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "HP-UX.*")
    set(HPUX TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "AIX.*")
    set(AIX TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "Minix.*")
    set(MINIX TRUE)
  endif ()
elseif (APPLE)
  if (CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*")
    set(DARWIN TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
    set(DARWIN TRUE)
  endif ()
  find_program(BREW brew)
endif ()

# ------------------------------------------------------------------------------
# user options
# ------------------------------------------------------------------------------

if (WINDOWS)
  SET(USE_JEMALLOC OFF)
else ()
  option(
    USE_JEMALLOC
    "use jemalloc memory allocator"
    ON
  )
endif ()

if (CROSS_COMPILING)
# curently off, need additional params to configure like --hoast=triple <params>
  SET(USE_JEMALLOC OFF)
endif()

################################################################################
## EXTERNAL PROGRAMS
################################################################################

if (SOLARIS)
  set(MAKE gmake)
else ()
  set(MAKE make)
endif ()

find_package(PythonInterp 2 EXACT REQUIRED)
get_filename_component(PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE}" REALPATH)

set($ENV{PYTHON_EXECUTABLE} ${PYTHON_EXECUTABLE})

# FIXME the build containers seem to have a
# /usr/bin/ch(mod|own) to prevent the search
# to find those files the NO_DEFAULT_PATH
# argument is passed
if (NOT WINDOWS)
  find_program(
    CHMOD_EXECUTABLE chmod
    PATHS "/bin/" "/usr/bin/"
    NO_DEFAULT_PATH
  )
  message(STATUS "chmod found in ${CHMOD_EXECUTABLE}")
  find_program(
    CHOWN_EXECUTABLE chown
    PATHS "/bin" "/usr/bin"
    NO_DEFAULT_PATH
  )
  message(STATUS "chown found in ${CHOWN_EXECUTABLE}")
endif()

################################################################################
## ARCHITECTURE
################################################################################

if (SOLARIS OR NOT CMAKE_SIZEOF_VOID_P)
  set(CMAKE_SIZEOF_VOID_P 8)
  set(CMAKE_CXX_SIZEOF_DATA_PTR 8)
endif ()

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
add_definitions("-DARANGODB_BITS=${BITS}")

################################################################################
## COMPILER FEATURES
################################################################################

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_COMPILER_IS_CLANG 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  set(CMAKE_COMPILER_IS_CLANG 1)
endif ()

if (WINDOWS)
  set(BASE_FLAGS     "/D WIN32 /D _WINDOWS /W3 /MP"      CACHE STRING "base flags")
  set(BASE_C_FLAGS   ""                         	 CACHE STRING "base C flags")
  set(BASE_CXX_FLAGS "/GR /EHsc"                	 CACHE STRING "base C++flags")
else ()
  set(BASE_FLAGS     ""                                  CACHE STRING "base flags")
  set(BASE_C_FLAGS   "${CMAKE_C_FLAGS}   $ENV{CFLAGS}"   CACHE STRING "base C flags")
  set(BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{CXXFLAGS}" CACHE STRING "base C++ flags")
  set(BASE_LD_FLAGS                     "$ENV{LDFLAGS}"  CACHE STRING "base linker flags")
  set(BASE_LIBS                         "$ENV{LIBS}"     CACHE STRING "base libraries")
endif ()

if (SOLARIS)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

  set(CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
    /lib;/lib64;/lib64;/usr/lib;/usr/lib64;/usr/lib64
  )

  list(APPEND SYSTEM_LIBRARIES nsl socket)

  # force 64bit compile
  set(BASE_FLAGS "-I /opt/csw/include -D_REENTRANT -m64 ${BASE_FLAGS}")
endif ()

if (CMAKE_COMPILER_IS_GNUCC)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.4")
    message(FATAL_ERROR "ArangoDB requires g++ 5.4 or newer.")
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "8.9.9")
    message(FATAL_ERROR "ArangoDB doesn't support g++ 9.0 yet.")
  endif()
  set(BASE_C_FLAGS "${BASE_C_FLAGS}")
endif ()

if (CMAKE_COMPILER_IS_GNUCC OR APPLE)
  set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -std=c++11")
endif ()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  if (MSVC_VERSION VERSION_LESS 1915)
    message(FATAL_ERROR "ArangoDB Requires at least MSVC 2017 update 15.8!")
  endif()

  # MSVC2018.1 - MSVC2018.7 does not correctly support alignas()
  # MSVC2018.8 requires the following define
  add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)
endif()

if (CMAKE_COMPILER_IS_CLANG)
  if (APPLE)
    set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -stdlib=libc++")
    add_definitions("-Wno-deprecated-declarations")
  else ()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.3")
      message(FATAL_ERROR "ArangoDB requires clang 3.3 or newer.")
    endif()
    list(APPEND BASE_LIBS atomic)
  endif ()
endif ()

# need c++11
# XXX this should really be set on a per target level using cmake compile_features capabilties
set(CMAKE_CXX_STANDARD 11)

# need threads
find_package(Threads REQUIRED)

if (MSVC)
  configure_file("Installation/Windows/vcproj.user/arangod.vcxproj.user" ${CMAKE_BINARY_DIR})
  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
  add_definitions("-DFD_SETSIZE=8192")
  add_definitions("-DU_STATIC_IMPLEMENTATION=1")

  # https://blogs.msdn.microsoft.com/vcblog/2016/04/14/stl-fixes-in-vs-2015-update-2/
  # https://connect.microsoft.com/VisualStudio/feedback/details/1892487
  # http://lists.boost.org/boost-users/2016/04/85968.php
  add_definitions("-D_ENABLE_ATOMIC_ALIGNMENT_FIX")

  set(MSVC_LIBS Shlwapi.lib;crypt32.lib;WINMM.LIB;Ws2_32.lib;Psapi.lib)

  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE /SAFESEH:NO /MACHINE:x64 /ignore:4099 ${BASE_LD_FLAGS}"
  )
else ()
  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} ${BASE_LD_FLAGS}"
  )
endif ()

# broken clock_gettime on MacOSX
SET(USE_LOCAL_CLOCK_GETTIME FALSE)

if (APPLE)
  TRY_RUN(RUN_CLOCK_GETTIME
          COMPILE_CLOCK_GETTIME
          ${PROJECT_BINARY_DIR}/test_clock_gettime
          ${CMAKE_SOURCE_DIR}/cmake/test_clock_gettime.c
          OUTPUT_VARIABLE OUTPUT_CLOCK_GETTIME)

  if (${COMPILE_CLOCK_GETTIME})
    if (${RUN_CLOCK_GETTIME} STREQUAL "FAILED_TO_RUN")
      set(USE_LOCAL_CLOCK_GETTIME TRUE)
    endif ()
  endif ()

  if (USE_LOCAL_CLOCK_GETTIME)
    message(STATUS "using a home-made clock_gettime")
  endif ()
endif ()

################################################################################
## INCLUDE DIRECTORIES
################################################################################

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_BINARY_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/arangod)
include_directories(${PROJECT_SOURCE_DIR}/arangosh)
include_directories(${PROJECT_SOURCE_DIR}/lib)

if (CUSTOM_INCLUDES)
  include_directories(${CUSTOM_INCLUDES})
endif ()

################################################################################
## TARGET ARCHITECTURE
################################################################################

set(ARANGODB_SSE42_FLAGS "")
if (WINDOWS)
  add_definitions("-DNO_SSE42")
else ()
  include(TargetArch)

  target_architecture(CMAKE_TARGET_ARCHITECTURES)
  list(LENGTH CMAKE_TARGET_ARCHITECTURES cmake_target_arch_len)

  if (NOT "${cmake_target_arch_len}" EQUAL "1")
    set(CMAKE_TARGET_ARCHITECTURE_UNIVERSAL TRUE)
    set(CMAKE_TARGET_ARCHITECTURE_CODE "universal")
  else ()
    set(CMAKE_TARGET_ARCHITECTURE_UNIVERSAL FALSE)
    set(CMAKE_TARGET_ARCHITECTURE_CODE "${CMAKE_TARGET_ARCHITECTURES}")
  endif ()

  include(VcMacros)

  option(USE_OPTIMIZE_FOR_ARCHITECTURE "try to determine CPU architecture" ON)

  if (USE_OPTIMIZE_FOR_ARCHITECTURE)
    # mop: core2 (merom) is our absolute minimum!
    # SET(TARGET_ARCHITECTURE "merom")
    include(OptimizeForArchitecture)
    OptimizeForArchitecture()
  endif ()

  if (USE_SSE4_2)
    set(ARANGODB_SSE42_FLAGS "-msse4.2")
  endif ()

  set(BASE_FLAGS "${Vc_ARCHITECTURE_FLAGS} ${BASE_FLAGS}")
endif ()

set(ARCHITECTURE_OPTIMIZATIONS "\"${Vc_ARCHITECTURE_FLAGS}\"")

################################################################################
## BACKTRACE
################################################################################

option(USE_BACKTRACE
  "whether we should try to generate c-level stacktraces"
  OFF
)

# iresearch uses backtrace anyway, so we need to find and link libexecinfo
# for the case that we are on libmusl and not on glibc
if (NOT MSVC)
  find_package(Backtrace)
endif()

if (USE_BACKTRACE)
  if (MSVC)
    set(BT_LIBS "Dbghelp" CACHE path "Debug Helper libraries")
  else ()
    set(BT_LIBS ${Backtrace_LIBRARY} CACHE path "Debug Helper libraries")
  endif ()

  add_definitions("-DARANGODB_ENABLE_BACKTRACE=1")
else ()
  set(BT_LIBS ${Backtrace_LIBRARY} CACHE path "Debug Helper libraries")
endif ()

################################################################################
## ASSEMBLER OPTIMIZATIONS
################################################################################

# Allow to prohibit assembler optimization code explicitly
if (MSVC)
  SET(ASM_OPTIMIZATIONS_DEFAULT OFF)
else (MSVC)
  SET(ASM_OPTIMIZATIONS_DEFAULT ON)
endif (MSVC)

option(ASM_OPTIMIZATIONS "whether hand-optimized assembler code should be used"
  ${ASM_OPTIMIZATIONS_DEFAULT})

if (ASM_OPTIMIZATIONS)
  add_definitions("-DASM_OPTIMIZATIONS=1")
else (ASM_OPTIMIZATIONS)
  add_definitions("-DASM_OPTIMIZATIONS=0")
endif (ASM_OPTIMIZATIONS)

################################################################################
## MAINTAINER MODE
################################################################################

option(USE_MAINTAINER_MODE
  "whether we want to have assertions and other development features"
  OFF
)

if (USE_MAINTAINER_MODE)
  add_definitions("-DARANGODB_ENABLE_MAINTAINER_MODE=1")
  if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions("-D_FORTIFY_SOURCE=2")
  endif()

  find_package(FLEX)
  find_package(BISON)
  # these are required for generateREADME.sh
  find_program(FGREP_EXECUTABLE fgrep)
  find_program(MARKDOWN_EXECUTABLE markdown)
  find_program(HTML2TEXT_EXECUTABLE html2text)
  find_program(SED_EXECUTABLE sed)
  find_program(AWK_EXECUTABLE awk)
endif ()

option(USE_CATCH_TESTS "Compile catch C++ tests" ON)
if (USE_CATCH_TESTS)
  add_definitions("-DTEST_VIRTUAL=virtual")
else()
  add_definitions("-DTEST_VIRTUAL=")
endif()

include(debugInformation)
find_program(READELF_EXECUTABLE readelf)
detect_binary_id_type(CMAKE_DEBUG_FILENAMES_SHA_SUM)

################################################################################
## FAILURE TESTS
################################################################################

option(USE_FAILURE_TESTS
  "whether we want to have failure tests compiled in"
  OFF
)

if (USE_FAILURE_TESTS)
  add_definitions("-DARANGODB_ENABLE_FAILURE_TESTS=1")
endif ()

################################################################################
## LIBRARY RESOLV
################################################################################

if (NOT WINDOWS)
  set(SYS_LIBS ${SYS_LIBS} resolv)

  if (NOT DARWIN)
    set(SYS_LIBS ${SYS_LIBS} rt)
  endif ()
endif ()

# ------------------------------------------------------------------------------
# IMPLICIT INCLUDES AND LIBIRARY DIRECTORIES
# ------------------------------------------------------------------------------

function(CREATE_FLAGS OUTPUT GLUE)
  set(_TMP_RESULT "")

  foreach(arg ${ARGN})
    set(_TMP_RESULT "${_TMP_RESULT} ${GLUE}${arg}")
  endforeach()

  set(${OUTPUT} "${_TMP_RESULT}" PARENT_SCOPE)
endfunction()

# CREATE_FLAGS(IMPLICIT_C_INCLUDES "${CMAKE_C_SYSROOT_FLAG} " ${CMAKE_OSX_SYSROOT})
# CREATE_FLAGS(IMPLICIT_CXX_INCLUDES "${CMAKE_CXX_SYSROOT_FLAG} " ${CMAKE_OSX_SYSROOT})

# ------------------------------------------------------------------------------
# JEMALLOC
# ------------------------------------------------------------------------------

if (USE_JEMALLOC)
  add_definitions("-DARANGODB_HAVE_JEMALLOC=1")
endif ()

# ------------------------------------------------------------------------------
# NDEBUG
# ------------------------------------------------------------------------------

add_definitions(-DNDEBUG)

################################################################################
## FLAGS
################################################################################

if (VERBOSE)
  message(STATUS)
endif ()


# compiler options
if (CMAKE_COMPILER_IS_GNUCC)
  if (VERBOSE)
    message(STATUS "Compiler type GNU: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(BASE_FLAGS "-Wall -Wextra -Wno-unused-parameter ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "-g"                             CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g -D_DEBUG=1"              CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "-g -Wnon-virtual-dtor"          CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -D_DEBUG=1"              CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")

elseif (CMAKE_COMPILER_IS_CLANG)
  if (VERBOSE)
    message(STATUS "Compiler type CLANG: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(BASE_FLAGS "-Wall -Wextra -Wno-unused-parameter ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "-g"                             CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g -D_DEBUG=1"              CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "-g -Wnon-virtual-dtor"          CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -D_DEBUG=1"              CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")

elseif (MSVC)
  if (VERBOSE)
    message(STATUS "Compiler type MSVC: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(CMAKE_C_FLAGS                "/MTd"                              CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1 /bigobj" CACHE INTERNAL "C++ debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "/MT /O1 /Ob1"                      CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "/MT /O2 /Ob2"                      CACHE INTERNAL "C++ release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1"                  CACHE INTERNAL "C++ release with debug info flags")

  set(CMAKE_CXX_FLAGS                "/MTd"                              CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1 /bigobj" CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1"                      CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2"                      CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1"                  CACHE INTERNAL "C++ release with debug info flags")

else ()
  # unknown compiler
  message(STATUS "Compiler type UNKNOWN: ${CMAKE_CXX_COMPILER}")

  set(BASE_FLAGS "-Wall ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "-g"                             CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "-g"                             CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")
endif ()

set(CMAKE_C_FLAGS                "${CMAKE_C_FLAGS}                    ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}              ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}            ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}     ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_C_FLAGS_MINSIZEREL}         ${BASE_FLAGS} ${BASE_C_FLAGS}")

set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS}                ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}          ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}        ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL}     ${BASE_FLAGS} ${BASE_CXX_FLAGS}")

if (VERBOSE)
  message(STATUS "Info BASE_FLAGS:     ${BASE_FLAGS}")
  message(STATUS "Info BASE_C_FLAGS:   ${BASE_C_FLAGS}")
  message(STATUS "Info BASE_CXX_FLAGS: ${BASE_CXX_FLAGS}")
  message(STATUS "Info BASE_LD_FLAGS:  ${BASE_LD_FLAGS}")
  message(STATUS "Info BASE_LIBS:      ${BASE_LIBS}")
  message(STATUS)

  if (CMAKE_BUILD_TYPE STREQUAL "None")
    message(STATUS "Info CMAKE_C_FLAGS:   ${CMAKE_C_FLAGS}")
    message(STATUS "Info CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  else ()
    message(STATUS "Info CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}:   ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
    message(STATUS "Info CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  endif ()

  message(STATUS "Info CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
  message(STATUS)
endif ()

if(ARANGODB_DEBUG_CMAKE)
  get_directory_property( DirDefs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMPILE_DEFINITIONS )
  foreach( d ${DirDefs} )
    message( STATUS "Found Define: " ${d} )
  endforeach()
  message( STATUS "DirDefs: ${DirDefs}" )
endif()


################################################################################
## OPENSSL
################################################################################

# Some special stuff for Mac OSX and homebrew as a preparation for the 
# generic FindOpenSSL script:
if (APPLE AND BREW AND NOT OPENSSL_ROOT_DIR)
  message("searching openssl with brew (${BREW})")
  # if we have a brew openssl, prefer it over the elderly system one.
  execute_process(OUTPUT_VARIABLE BREW_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND ${BREW} --prefix)
  set(OPENSSL_ROOT_DIR "${BREW_ROOT}/opt/openssl")
  message("auto-set OPENSSL_ROOT_DIR to: ${OPENSSL_ROOT_DIR}")
endif ()

find_package(OpenSSL REQUIRED)

include_directories(${OPENSSL_INCLUDE_DIR})
add_definitions(-DARANGODB_OPENSSL_VERSION=\"${OPENSSL_VERSION}\")
add_definitions(-DOPENSSL_VERSION_MAJOR=${OPENSSL_VERSION_MAJOR})
add_definitions(-DOPENSSL_VERSION_MINOR=${OPENSSL_VERSION_MINOR})

if (OPENSSL_VERSION)
  string(REPLACE "." ";" OPENSSL_VERSION_LIST ${OPENSSL_VERSION})
  list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
  list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)

  if ("${OPENSSL_VERSION_MAJOR}" GREATER 0 AND "${OPENSSL_VERSION_MINOR}" GREATER 0)
    option(USE_OPENSSL_NO_SSL2
      "do not use OPENSSL_NO_SSL2"
      ON
    )
  else ()
    option(USE_OPENSSL_NO_SSL2
      "do not use OPENSSL_NO_SSL2"
      OFF
    )
  endif ()
endif ()

if (USE_OPENSSL_NO_SSL2)
  add_definitions(-DOPENSSL_NO_SSL2)
endif ()

################################################################################
## 3RD PARTY
################################################################################

# ------------------------------------------------------------------------------
# IResearch
# ------------------------------------------------------------------------------

set(LIB_ARANGO_IRESEARCH arango_iresearch)

# 3rdParty exports:
#
# V8_VERSION
# V8_LIBS
# V8_INCLUDE_DIR
#
# ICU_VERSION
# ICU_LIBS
# ICU_INCLUDE_DIR
#
# ZLIB_VERSION
# ZLIB_LIBS
# ZLIB_INCLUDE_DIR

add_definitions(-DBOOST_ALL_NO_LIB=1) #disable boost autolink on windows
add_subdirectory(3rdParty)

add_definitions("-DARANGODB_BOOST_VERSION=\"${BOOST_VERSION}\"")


################################################################################
## 3rd Party INCLUDE DIRECTORIES
################################################################################

include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/include)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/include)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/fuerte/include)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/rocksdb/${ARANGO_ROCKSDB_VERSION}/include)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/s2geometry/${ARANGO_S2GEOMETRY_VERSION}/src)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/rocksdb/${ARANGO_ROCKSDB_VERSION})
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/date/include)

# ------------------------------------------------------------------------------
# RocksDB
# ------------------------------------------------------------------------------

add_definitions(-DARANGODB_ROCKSDB_VERSION=\"${ARANGO_ROCKSDB_VERSION}\")
include_directories(${ROCKSDB_INCLUDE_DIR})

set(ROCKSDB_LIBS rocksdb;${SNAPPY_LIB})

add_dependencies(rocksdb ${SNAPPY_LIB})

if (USE_JEMALLOC)
  add_dependencies(rocksdb jemalloc)
endif ()

include_directories(SYSTEM ${ASIO_INCLUDES})

################################################################################
## VELOCYPACK
################################################################################

add_definitions("-DVELOCYPACK_XXHASH=1")

set(V8_LINK_DIRECTORIES "${LINK_DIRECTORIES}" CACHE INTERNAL "" FORCE)

################################################################################
## ICU
################################################################################

include_directories(SYSTEM ${ICU_INCLUDE_DIR})

################################################################################
## V8
################################################################################

include_directories(SYSTEM ${V8_INCLUDE_DIR})
add_definitions("-DARANGODB_V8_VERSION=\"${V8_VERSION}\"")

foreach (LINK_DIR ${V8_LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

################################################################################
## ZLIB
################################################################################

include_directories(SYSTEM ${ZLIB_INCLUDE_DIR})
add_definitions("-DARANGODB_ZLIB_VERSION=\"${ZLIB_VERSION}\"")
link_directories("${PROJECT_BINARY_DIR}/bin")

################################################################################
## cURL
################################################################################

add_definitions(-DCURL_STATICLIB=1)
include_directories(SYSTEM
  ${CURL_SRC_DIR}/include/
  ${CURL_BIN_DIR}/include/curl/
)

################################################################################
## PATHS, installation, packages, frontend
################################################################################

if (MSVC)
  # versioning
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Installation/Windows/version")
  include(generate_product_version)
endif()

include(ArangoDBInstall)
include(frontend/aardvark)
if (NOT(SKIP_PACKAGING))
  include(packages/packages)
endif()

################################################################################
## ERRORS FILE
################################################################################

if (USE_MAINTAINER_MODE)
  set(ERROR_FILES
    lib/Basics/voc-errors.h
    lib/Basics/voc-errors.cpp
    js/common/bootstrap/errors.js
  )

  set(ERROR_FILES_GEN)
  set(ERRORS_DAT lib/Basics/errors.dat)

  foreach (m IN LISTS ERROR_FILES)
    add_custom_command(
      OUTPUT ${CMAKE_SOURCE_DIR}/${m}
      COMMAND ${PYTHON_EXECUTABLE} ./utils/generateErrorfile.py ./${ERRORS_DAT} ./${m}.tmp
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ./${m}.tmp ./${m}
      COMMAND ${CMAKE_COMMAND} -E remove ./${m}.tmp
      DEPENDS ${CMAKE_SOURCE_DIR}/${ERRORS_DAT}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT  "Building errors files ${m}"
      VERBATIM
    )

    list(APPEND ERROR_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(errorfiles ALL DEPENDS ${ERROR_FILES_GEN})

  set(EXIT_CODE_FILES
    lib/Basics/exitcodes.h
    lib/Basics/exitcodes.cpp
    js/common/bootstrap/exitcodes.js
    Installation/Windows/Plugins/exitcodes.nsh
  )

  set(EXIT_CODE_FILES_GEN)
  set(EXIT_CODES_DAT lib/Basics/exitcodes.dat)

  foreach (m IN LISTS EXIT_CODE_FILES)
    add_custom_command(
      OUTPUT ${CMAKE_SOURCE_DIR}/${m}
      COMMAND ${PYTHON_EXECUTABLE} ./utils/generateExitCodesFiles.py ./${EXIT_CODES_DAT} ./${m}.tmp
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ./${m}.tmp ./${m}
      COMMAND ${CMAKE_COMMAND} -E remove ./${m}.tmp
      DEPENDS ${CMAKE_SOURCE_DIR}/${EXIT_CODES_DAT}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT  "Building exitcode files ${m}"
      VERBATIM
    )

    list(APPEND EXIT_CODE_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(exitcodefiles ALL DEPENDS ${EXIT_CODE_FILES_GEN})
endif ()

################################################################################
## MIMETYPES FILE
################################################################################

if (USE_MAINTAINER_MODE)
  set(MIMETYPES_FILES
    lib/Basics/voc-mimetypes.h
    lib/Basics/voc-mimetypes.cpp
  )

  set(MIMETYPES_FILES_GEN)
  set(MIMETYPES_DAT lib/Basics/mimetypes.dat)

  foreach (m IN LISTS MIMETYPES_FILES)
    add_custom_command(
      OUTPUT ${CMAKE_SOURCE_DIR}/${m}
      COMMAND ${PYTHON_EXECUTABLE} ./utils/generateMimetypes.py ./${MIMETYPES_DAT} ./${m}.tmp
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ./${m}.tmp ./${m}
      COMMAND ${CMAKE_COMMAND} -E remove ./${m}.tmp
      DEPENDS ${CMAKE_SOURCE_DIR}/${MIMETYPES_DAT}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Building mimetypes files ${m}"
      VERBATIM
    )

    list(APPEND MIMETYPES_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(mimetypes ALL DEPENDS ${MIMETYPES_FILES_GEN})
endif ()

################################################################################
## SUB-PROJECTS
################################################################################

list(INSERT SYSTEM_LIBRARIES 0
  ${BT_LIBS}
  ${ZLIB_LIBS}
  ${ICU_LIBS}
  ${OPENSSL_LIBRARIES}
  libcurl
  ${BASE_LIBS}
  ${SYS_LIBS}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)

add_subdirectory(Documentation)

if (USE_CATCH_TESTS)
  add_subdirectory(tests)
endif()

add_dependencies(arangobench   zlibstatic)
add_dependencies(arangod       zlibstatic)
add_dependencies(arangodump    zlibstatic)
add_dependencies(arangoexport  zlibstatic)
add_dependencies(arangoimport  zlibstatic)
add_dependencies(arangorestore zlibstatic)
add_dependencies(arangosh      zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  # all binaries depend on v8_build because it contains ICU as well
  add_dependencies(arangobench   v8_build)
  add_dependencies(arangod       v8_build)
  add_dependencies(arangodump    v8_build)
  add_dependencies(arangoexport  v8_build)
  add_dependencies(arangoimport  v8_build)
  add_dependencies(arangorestore v8_build)
  add_dependencies(arangosh      v8_build)
  if (USE_CATCH_TESTS)
    add_dependencies(arangodbtests v8_build)
  endif()
endif ()

# This copies the compile commands to the source dir.
# There they can be used to repeat parts of compile
# steps generating an AST. This AST can in turn be used
# for meaningful auto-completion in editors without
# heuristics, dice rolling and other guessing.

if( EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json" )
  message(STATUS "copy compile_commands.json")

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
      ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
  )
endif()

add_custom_target(packages
  DEPENDS ${PACKAGES_LIST}
)

add_custom_target(copy_packages
  DEPENDS ${COPY_PACKAGES_LIST}
)

add_custom_target(clean_packages
  DEPENDS ${CLEAN_PACKAGES_LIST}
  )

add_custom_target(clean_autogenerated_files
  DEPENDS ${CLEAN_AUTOGENERATED_FILES}
  )

message(STATUS "building for git revision: ${ARANGODB_BUILD_REPOSITORY}")

# if (VERBOSE)
#   get_cmake_property(_variableNames VARIABLES)
#
#   foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
#   endforeach ()
# endif ()
