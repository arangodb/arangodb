# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# General
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.9)
message(STATUS "CMake version: ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release
      CACHE STRING
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

if (APPLE)
  if (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR "${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
    if (NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET} OR "ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
      message(FATAL_ERROR "environment variable MACOSX_DEPLOYMENT_TARGET is missing")
    else ()
      set(CMAKE_OSX_DEPLOYMENT_TARGET "$ENV{MACOSX_DEPLOYMENT_TARGET}" CACHE STRING "deployment target for MacOSX; adjust to your system")
    endif ()
  else ()
    set($ENV{MACOSX_DEPLOYMENT_TARGET} ${CMAKE_OSX_DEPLOYMENT_TARGET})
  endif ()
endif ()

# where to find CMAKE modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

option(SKIP_PACKAGING "" OFF)

# be verbose about flags used
option(VERBOSE "be verbose about flags used" OFF)

# treat warnings as errors on some platforms
option(USE_FAIL_ON_WARNINGS "treat warnings as errors (MSVC and MacOS only)" ON)

# use strictly required OpenSSL version (from ./VERSIONS file)
option(USE_STRICT_OPENSSL_VERSION "use strictly required OpenSSL version (from ./VERSIONS file)" OFF)

option(USE_MINIMAL_DEBUGINFO "use minimal debug symbols for builds containing debug information" OFF)

# don't use standalone boost asio
# use this when asio is included outside of Boost
# add_definitions("-DARANGODB_STANDALONE_ASIO=0")
# add_definitions("-DFUERTE_STANDALONE_ASIO=0")

# ------------------------------------------------------------------------------
# VERSION information
# ------------------------------------------------------------------------------

# stable release:   MAJOR.MINOR.PATCH
# hot fix:          MAJOR.MINOR.PATCH-FIXNUMBER
# unstable release: MAJOR.MINOR.PATCH-TYPE.NUMBER
# devel:            MAJOR.MINOR.0-devel
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
set(ARANGODB_VERSION_MINOR "9")

# when building the nightly ARANGODB_VERSION_PATCH will be set
if (NOT DEFINED ARANGODB_VERSION_PATCH)
  set(ARANGODB_VERSION_PATCH "0")
  set(ARANGODB_VERSION_RELEASE_TYPE "devel")
  set(ARANGODB_VERSION_RELEASE_NUMBER "")
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

  # devel
  if (ARANGODB_VERSION_PATCH STREQUAL "0" AND ARANGODB_VERSION_RELEASE_TYPE STREQUAL "devel")
    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.0~~${ARANGODB_VERSION_PATCH}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.0")
    set(ARANGODB_RPM_REVISION "0.1.devel")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DARWIN_REVISION "devel")

  # nightly
  elseif (ARANGODB_VERSION_RELEASE_TYPE STREQUAL "nightly")
    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.0")
    set(ARANGODB_RPM_REVISION "0.2.nightly")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_DARWIN_REVISION "nightly")

  # unstable version
  elseif (   ARANGODB_VERSION_RELEASE_TYPE STREQUAL "alpha"
          OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "beta"
          OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "milestone"
          OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "preview"
          OR ARANGODB_VERSION_RELEASE_TYPE STREQUAL "rc")
    if (NOT DEFINED ARANGODB_VERSION_RELEASE_NUMBER)
      message(FATAL_ERROR "ARANGODB_VERSION_RELEASE_NUMBER is missing")
    endif()

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
    if (DEFINED ARANGODB_VERSION_RELEASE_NUMBER)
      message(FATAL_ERROR "ARANGODB_VERSION_RELEASE_NUMBER must be empty for type ${ARANGODB_VERSION_RELEASE_TYPE}")
    endif()

    set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}.${ARANGODB_VERSION_RELEASE_TYPE}")
    set(ARANGODB_DEBIAN_REVISION "1")

    set(ARANGODB_RPM_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
    set(ARANGODB_RPM_REVISION "1.${ARANGODB_VERSION_RELEASE_TYPE}")

    set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}.${ARANGODB_VERSION_RELEASE_TYPE}")
    set(ARANGODB_DARWIN_REVISION "")
  endif()
else()

  # stable version
  set(ARANGODB_DEBIAN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
  set(ARANGODB_DEBIAN_REVISION "1")

  set(ARANGODB_RPM_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
  set(ARANGODB_RPM_REVISION "1.0")

  set(ARANGODB_DARWIN_UPSTREAM "${ARANGODB_PLAIN_VERSION}")
  set(ARANGODB_DARWIN_REVISION "")
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

if (MSVC)
  option(USE_CLCACHE_MODE
    "whether we want to use CLCACHE-specific build options"
    OFF
  )
endif ()

################################################################################
# SNAP version
################################################################################

set(ARANGODB_SNAP_REVISION "1")

message(STATUS "SNAP REVISION: ${ARANGODB_SNAP_REVISION}")

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------

if (WIN32)
  project(arangodb3 LANGUAGES CXX C VERSION ${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR})
else ()
  project(arangodb3 LANGUAGES CXX C ASM VERSION ${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR})
endif ()

# required for clang completion in editors - must be set after creating project
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static executables:
option(STATIC_EXECUTABLES "produce static executables" OFF)
if (STATIC_EXECUTABLES)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  if (WINDOWS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
  else ()
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  endif ()
endif()

# enable dtrace
option(USE_DTRACE "enable dtrace probes" OFF)
if (USE_DTRACE)
  add_definitions("-DUSE_DTRACE=1")
endif ()

# enable Enterprise Edition features
set(ENTERPRISE_INCLUDE_DIR "enterprise")
option(USE_ENTERPRISE "enable enterprise build" OFF)

if (USE_ENTERPRISE)
  add_definitions("-DUSE_ENTERPRISE=1")
  add_subdirectory(enterprise)
endif ()

# we want the following definitions to be in effect for both rocksdb and arangodb
add_definitions("-DNROCKSDB_THREAD_STATUS")
add_definitions("-DROCKSDB_SUPPORT_THREAD_LOCAL")

# for the packages
set(ARANGODB_PACKAGE_VENDOR  "ArangoDB GmbH")
set(ARANGODB_PACKAGE_CONTACT "info@arangodb.com")
set(ARANGODB_DISPLAY_NAME    "ArangoDB")
set(ARANGODB_URL_INFO_ABOUT  "https://www.arangodb.com")
set(ARANGODB_HELP_LINK       "https://www.arangodb.com/docs/${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}/")
set(ARANGODB_CONTACT         "hackers@arangodb.com")
set(ARANGODB_FRIENDLY_STRING "ArangoDB - the native multi-model NoSQL database")

# MSVC
set(ARANGO_BENCH_FRIENDLY_STRING   "arangobench - stress test tool")
set(ARANGO_BACKUP_FRIENDLY_STRING  "arangobackup - hot backup tool")
set(ARANGO_DUMP_FRIENDLY_STRING    "arangodump - data and configuration dumping tool")
set(ARANGO_RESTORE_FRIENDLY_STRING "arangrestore - data and configuration restoration tool")
set(ARANGO_EXPORT_FRIENDLY_STRING  "arangoexport - data exporter")
set(ARANGO_IMPORT_FRIENDLY_STRING  "arangoimport - data importer")
set(ARANGOSH_FRIENDLY_STRING       "arangosh - commandline client")
set(ARANGO_VPACK_FRIENDLY_STRING   "arangovpack - VelocyPack formatter")

# binaries
set(BIN_ARANGOBENCH   arangobench)
set(BIN_ARANGOBACKUP  arangobackup)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOEXPORT  arangoexport)
set(BIN_ARANGOIMPORT  arangoimport)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)
set(BIN_ARANGOVPACK   arangovpack)

# test binaries
set(BIN_ARANGODB_TESTS arangodbtests)
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

if (DEFINED GIT_EXE AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/enterprise/.git")
  execute_process(
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}

    COMMAND ${GIT_EXE} describe --all --tags --long --dirty=-dirty
    OUTPUT_VARIABLE GIT_OUTPUT)

  # this may fail on shallow clones that only knows about a limited number of commits.
  # if there is an older merged revision the head, it may not be available to git.
  if (NOT GIT_OUTPUT)
      set(ENTERPRISE_BUILD_REPOSITORY "GIT FAILED TO RETRIEVE THE VERSION - SHALLOW CLONE?")
      set(HAVE_ENTERPRISE_BUILD_REPOSITORY "1")
    else()
      string(STRIP ${GIT_OUTPUT} REPOSITORY_VERSION)
      set(ENTERPRISE_BUILD_REPOSITORY ${REPOSITORY_VERSION})
      set(HAVE_ENTERPRISE_BUILD_REPOSITORY "1")
    endif()
else ()
  set(ENTERPRISE_BUILD_REPOSITORY "")
  set(HAVE_ENTERPRISE_BUILD_REPOSITORY "0")
endif()

if (DEFINED OSKAR_BUILD_REPOSITITORY)
  set(HAVE_OSKAR_BUILD_REPOSITORY "1")
else ()
  set(OSKAR_BUILD_REPOSITORY "")
  set(HAVE_OSKAR_BUILD_REPOSITORY "0")
endif ()

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

  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-DNOMINMAX)
  add_definitions(-D_WIN32_WINNT=0x600)
  add_definitions(-DINCL_EXTRA_HTON_FUNCTIONS)
  add_definitions(-DPSAPI_VERSION=1)

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
elseif (APPLE)
  SET(USE_JEMALLOC OFF)
else ()
  option(
    USE_JEMALLOC
    "use jemalloc memory allocator"
    ON
  )
endif ()

if (LINUX)
  option(
    USE_LIBUNWIND
    "use libunwind for stack traces"
    ON
  )
else ()
  SET(USE_LIBUNWIND OFF)
endif ()

if (CROSS_COMPILING)
# currently off, need additional params to configure like --host=triple <params>
  SET(USE_JEMALLOC OFF)
  SET(USE_LIBUNWIND OFF)
endif()

################################################################################
## EXTERNAL PROGRAMS
################################################################################

set(MAKE make)

find_package(PythonInterp 3 EXACT REQUIRED)
get_filename_component(PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE}" REALPATH)

set($ENV{PYTHON_EXECUTABLE} ${PYTHON_EXECUTABLE})

# FIXME the build containers seem to have a
# /usr/bin/ch(mod|own) to prevent the search
# to find those files the NO_DEFAULT_PATH
# argument is passed
if (NOT WINDOWS)
  find_program (MAKE make gmake)

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
  set(BASE_C_FLAGS   ""                                  CACHE STRING "base C flags")
  set(BASE_CXX_FLAGS "/GR /EHsc /Zc:__cplusplus"         CACHE STRING "base C++flags")
else ()
  set(BASE_FLAGS     ""                                  CACHE STRING "base flags")
  set(BASE_C_FLAGS   "${CMAKE_C_FLAGS}   $ENV{CFLAGS}"   CACHE STRING "base C flags")
  set(BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{CXXFLAGS}" CACHE STRING "base C++ flags")
  set(BASE_LD_FLAGS                     "$ENV{LDFLAGS}"  CACHE STRING "base linker flags")
  set(BASE_LIBS                         "$ENV{LIBS}"     CACHE STRING "base libraries")
endif ()

if (CMAKE_COMPILER_IS_GNUCC)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
    message(WARNING "ArangoDB requires g++ 10.0 or newer, building with older compiler versions is unsupported")
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "10.9.9")
    message(WARNING "ArangoDB doesn't support g++ 11.0 yet, building with newer compiler versions is unsupported")
  endif()
  set(BASE_C_FLAGS "${BASE_C_FLAGS}")
endif ()

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG OR APPLE)
  set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -std=c++17")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # MSVC2018.1 - MSVC2018.7 does not correctly support alignas()
  if (MSVC_VERSION VERSION_LESS 1915)
    message(FATAL_ERROR "ArangoDB doesn't support MSVC less than 2017 update 15.8!")
  elseif (MSVC_VERSION VERSION_GREATER_EQUAL 1915 AND MSVC_VERSION VERSION_LESS 1920)
    message(WARNING "ArangoDB requires MSVC 2019 update 16.4 or newer, building with older compiler versions is unsupported")
    # according to https://developercommunity.visualstudio.com/solutions/616098/view.html,
    # the following define is not necessary anymore, as it should be fixed in MSVC 16.0:
    # MSVC2018.8 requires the following define
    add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)
  elseif (MSVC_VERSION VERSION_GREATER_EQUAL 1920 AND MSVC_VERSION VERSION_LESS 1924)
    message(FATAL_ERROR "ArangoDB requires at least MSVC 2019 update 16.4!")
  endif()
  set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -std:c++17") 
endif()

if (CMAKE_COMPILER_IS_CLANG)
  if (APPLE)
    if (NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET} OR ENV{MACOSX_DEPLOYMENT_TARGET} STREQUAL "")
      set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -stdlib=libc++")
    else ()
      set(BASE_C_FLAGS "${BASE_C_FLAGS} -mmacosx-version-min=$ENV{MACOSX_DEPLOYMENT_TARGET}")
      set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -stdlib=libc++ -mmacosx-version-min=$ENV{MACOSX_DEPLOYMENT_TARGET}")
      set(BASE_LD_FLAGS "${BASE_LD_FLAGS} -mmacosx-version-min=$ENV{MACOSX_DEPLOYMENT_TARGET}")
    endif ()
    add_definitions("-Wno-deprecated-declarations")
  else ()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
      message(WARNING "ArangoDB requires clang 10.0 or newer, building with older compiler versions is unsupported")
    endif()
    list(APPEND BASE_LIBS atomic)
  endif ()
endif ()

# need c++17
# XXX this should really be set on a per target level using cmake compile_features capabilties
set(CMAKE_CXX_STANDARD 17)

# need threads
find_package(Threads REQUIRED)

if (MSVC)
  configure_file("Installation/Windows/vcproj.user/arangod.vcxproj.user" ${CMAKE_BINARY_DIR})
  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
  add_definitions("-DFD_SETSIZE=8192")
  add_definitions("-DU_STATIC_IMPLEMENTATION=1")

  # bcrypt is needed for SSL
  set(MSVC_LIBS Shlwapi.lib;crypt32.lib;bcrypt.lib;WINMM.LIB;Ws2_32.lib;Psapi.lib)

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

include_directories(${PROJECT_SOURCE_DIR}/arangod)
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

# iresearch uses backtrace, so we need to find and link libexecinfo
# for the case that we are on libmusl and not on glibc
if (NOT MSVC)
  find_package(Backtrace)
endif()

set(BT_LIBS ${Backtrace_LIBRARY} CACHE PATH "Debug Helper libraries")

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
  if (CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions("-D_FORTIFY_SOURCE=2")
  endif()

  find_package(FLEX)
  find_package(BISON)
  # these are required for generateREADME.sh
  find_program(FGREP_EXECUTABLE fgrep)
  find_program(SED_EXECUTABLE sed)
  find_program(AWK_EXECUTABLE awk)
endif ()

option(USE_GOOGLE_TESTS "Compile C++ unit tests" ON)
if (USE_GOOGLE_TESTS)
  add_definitions("-DARANGODB_USE_GOOGLE_TESTS=1")
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
## INTERPROCEDURAL OPTIMIZATION (LINK TIME OPTIMIZATION)
################################################################################

set(USE_IPO AUTO CACHE STRING "Use interprocedural optimization: ON, OFF or AUTO")
set_property(CACHE USE_IPO PROPERTY STRINGS AUTO ON OFF)

set(IPO_ENABLED False)

# Determine value if IPO_ENABLED from USE_IPO and CMAKE_BUILD_TYPE
if (USE_IPO STREQUAL "AUTO")
  # When USE_IPO=AUTO, enable IPO for optimized / release builds.
  # But to work around a g++ segfault triggered by using both -flto and
  # -fno-devirtualize-functions, we disable IPO when using google tests, because
  # this will set no-devirtualize. See
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91387 and
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91375.
  # So this check may be removed later as soon as we use fixed gcc versions.
  # - Tobias, 2019-08-08
  if (NOT USE_GOOGLE_TESTS AND
    (CMAKE_BUILD_TYPE STREQUAL "Release"
    OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"
    OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    set(IPO_ENABLED True)
  else()
    set(IPO_ENABLED False)
  endif ()
elseif (USE_IPO)
  set(IPO_ENABLED True)
else()
  set(IPO_ENABLED False)
endif()

message(STATUS "IPO_ENABLED: ${IPO_ENABLED}")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${IPO_ENABLED})

if (IPO_ENABLED)
  add_definitions("-DARANGODB_USE_IPO=1")
endif()

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

option(USE_JEMALLOC_PROF "use jemalloc profiler" OFF)

if (USE_JEMALLOC)
  add_definitions("-DARANGODB_HAVE_JEMALLOC=1")
endif ()

if (USE_JEMALLOC_PROF)
  add_definitions("-DUSE_MEMORY_PROFILE=1")
endif ()

# ------------------------------------------------------------------------------
# LIBUNWIND
# ------------------------------------------------------------------------------

if (USE_LIBUNWIND)
  add_definitions("-DARANGODB_HAVE_LIBUNWIND=1")
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
if (MSVC) 
  message(STATUS "Compiler type MSVC: ${CMAKE_CXX_COMPILER}")

  if (USE_MINIMAL_DEBUGINFO)
    message(FATAL_ERROR "USE_MINIMAL_DEBUGINFO is not supported for this compiler")
  endif ()

  set(BASE_FLAGS "/wd4996 ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "/MTd"                                              CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1 /bigobj"         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "/MT /O1 /Ob1"                                      CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "/MT /O2 /Ob2"                                      CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1"                                  CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "/MTd"                                            CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1 /bigobj"       CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1"                                    CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2"                                    CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1"                                CACHE INTERNAL "C++ release with debug info flags")

  if (USE_CLCACHE_MODE)
    set(CMAKE_VS_GLOBALS "TrackFileAccess=false")
    add_definitions(/Z7)
  endif ()

else () # NOT MSVC
  set(EXTRA_C_FLAGS "")
  set(EXTRA_CXX_FLAGS "")

  if (CMAKE_COMPILER_IS_GNUCC)
    message(STATUS "Compiler type GNU: ${CMAKE_CXX_COMPILER}")
    set(BASE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations ${BASE_FLAGS}")
    set(EXTRA_CXX_FLAGS "-Wsuggest-override -Wnon-virtual-dtor")
  elseif (CMAKE_COMPILER_IS_CLANG)
    message(STATUS "Compiler type CLANG: ${CMAKE_CXX_COMPILER}")
    set(BASE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations ${BASE_FLAGS}")
    set(EXTRA_CXX_FLAGS "-Wnon-virtual-dtor")
    
    if (NOT DARWIN AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0")
      # clang 11 and higher supports -Wsuggest-override. older versions don't
      set(EXTRA_CXX_FLAGS "-Wsuggest-override ${EXTRA_CXX_FLAGS}")
    endif ()

    if (DARWIN AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.1")
      # With the introduction of apple clang version 12.0.0 (clang-1200.0.32.2) the warning "range-loop-analysis" is now
      # enabled by default or changed analysis behaviour. The compiler now complains about "wrong" usage of
      # VPackObjectIterator and VPackArrayIterator - which is not the case.
      #
      # See example here: https://github.com/arangodb/arangodb/pull/12730
      #
      # Since we're stopping compilation on Apple in any warning case, we need to temporary disable that warning and
      # check upcoming versions if the compiler behaviour will be fixed.
      set(EXTRA_C_FLAGS "-Wno-range-loop-analysis")
      set(EXTRA_CXX_FLAGS "${EXTRA_CXX_FLAGS} -Wno-range-loop-analysis")
    endif ()
  else ()
    # unknown compiler
    message(STATUS "Compiler type UNKNOWN: ${CMAKE_CXX_COMPILER}")
    set(BASE_FLAGS "-Wall ${BASE_FLAGS}")
  endif ()

  # flags for builds without debug symbols
  set(NODEBUGINFO_FLAGS "-g0")

  # flags for builds with debug symbols
  if (USE_MINIMAL_DEBUGINFO)
    # minimal debug symbols
    set(DEBUGINFO_FLAGS "-g1 -gno-column-info -gz")
  else ()
    # full debug symbols
    set(DEBUGINFO_FLAGS "-g")
  endif ()

  # c
  # note: when building one of the build types, CMake will automatically combine
  # the base flags from CMAKE_C_FLAGS with build type-specific flags in 
  # CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}.
  # there is no need to repeat the base flags in the build-type specific flags!
  set(CMAKE_C_FLAGS                  "${EXTRA_C_FLAGS}"                                CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG            "${DEBUGINFO_FLAGS} -O0 -D_DEBUG=1"               CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL       "${NODEBUGINFO_FLAGS} -Os"                        CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE          "${NODEBUGINFO_FLAGS} -O3 -fomit-frame-pointer"   CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${DEBUGINFO_FLAGS} -O3 -fno-omit-frame-pointer"  CACHE INTERNAL "C release with debug info flags")

  # cxx
  # note: when building one of the build types, CMake will automatically combine
  # the base flags from CMAKE_CXX_FLAGS with build type-specific flags in 
  # CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}.
  # there is no need to repeat the base flags in the build-type specific flags!
  set(CMAKE_CXX_FLAGS                "${EXTRA_CXX_FLAGS}"                              CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "${DEBUGINFO_FLAGS} -O0 -D_DEBUG=1"               CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${NODEBUGINFO_FLAGS} -Os"                        CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "${NODEBUGINFO_FLAGS} -O3 -fomit-frame-pointer"   CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${DEBUGINFO_FLAGS} -O3 -fno-omit-frame-pointer"  CACHE INTERNAL "C++ release with debug info flags")

endif ()

# put together the final flags
set(CMAKE_C_FLAGS    "${BASE_FLAGS} ${BASE_C_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS  "${BASE_FLAGS} ${BASE_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

if (VERBOSE)
  message(STATUS "Info BASE_FLAGS:     ${BASE_FLAGS}")
  message(STATUS "Info BASE_C_FLAGS:   ${BASE_C_FLAGS}")
  message(STATUS "Info BASE_CXX_FLAGS: ${BASE_CXX_FLAGS}")
  message(STATUS "Info BASE_LD_FLAGS:  ${BASE_LD_FLAGS}")
  message(STATUS "Info BASE_LIBS:      ${BASE_LIBS}")
  message(STATUS)

  if (NOT CMAKE_BUILD_TYPE STREQUAL "None")
    message(STATUS "Info CMAKE_C_FLAGS:   ${CMAKE_C_FLAGS}")
    message(STATUS "Info CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  endif ()
  message(STATUS "Info CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}:   ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  message(STATUS "Info CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")

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
## OpenSSL
################################################################################

if (NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/VERSIONS")
  message(FATAL_ERROR "expecting ${CMAKE_CURRENT_LIST_DIR}/VERSIONS")
else ()
  file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSIONS" ARANGODB_VERSIONS_CONTENT)
  if (LINUX)
    set (TARGET_OS "LINUX")
  elseif (DARWIN)
    set (TARGET_OS "MACOS")
  elseif (WIN32)
    set (TARGET_OS "WINDOWS")
  endif ()

  if (USE_STRICT_OPENSSL_VERSION)
    set (OPENSSL_VERSION_PATTERN ".*OPENSSL_${TARGET_OS}[ ]+\"([^\"]*).*")
  else ()
    set (OPENSSL_VERSION_PATTERN ".*OPENSSL_${TARGET_OS}[ ]+\"([^\"a-z]*).*")
  endif ()
  string(REGEX MATCH
            "${OPENSSL_VERSION_PATTERN}"
            ARANGODB_REQUIRED_OPENSSL_VERSION
            "${ARANGODB_VERSIONS_CONTENT}")
  if ("${CMAKE_MATCH_1}" STREQUAL "")
    message(FATAL_ERROR "expecting OPENSSL_${TARGET_OS} in ${CMAKE_CURRENT_LIST_DIR}/VERSIONS")
  else ()
    set (ARANGODB_REQUIRED_OPENSSL_VERSION "${CMAKE_MATCH_1}")
    if (USE_STRICT_OPENSSL_VERSION)
      set (MSG_ARANGODB_REQUIRED_OPENSSL_VERSION "${ARANGODB_REQUIRED_OPENSSL_VERSION}")
    else ()
      set (MSG_ARANGODB_REQUIRED_OPENSSL_VERSION "${ARANGODB_REQUIRED_OPENSSL_VERSION}*")
    endif ()
    message ("Required OpenSSL version: ${MSG_ARANGODB_REQUIRED_OPENSSL_VERSION}")
  endif ()
endif ()


if (NOT DEFINED OPENSSL_ROOT_DIR OR "${OPENSSL_ROOT_DIR}" STREQUAL "")
  if (DEFINED ENV{OPENSSL_ROOT_DIR} AND NOT "$ENV{OPENSSL_ROOT_DIR}" STREQUAL "")
    set (OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}")
  endif ()
else ()
  set(ENV{OPENSSL_ROOT_DIR} "${OPENSSL_ROOT_DIR}")
endif ()

unset (OPENSSL_FOUND CACHE)
unset (OPENSSL_INCLUDE_DIR CACHE)
unset (OPENSSL_CRYPTO_LIBRARY CACHE)
unset (OPENSSL_SSL_LIBRARY CACHE)
unset (OPENSSL_LIBRARIES CACHE)
unset (OPENSSL_VERSION CACHE)

if (DEFINED OPENSSL_ROOT_DIR AND NOT "${OPENSSL_ROOT_DIR}" STREQUAL "")
  message ("Use OPENSSL_ROOT_DIR: ${OPENSSL_ROOT_DIR}")
endif ()

if (WIN32)
  # Attempt to find ArangoDB CI compiled OpenSSL
  message ("Attempt to find ArangoDB CI compiled OpenSSL:")
  include ("${CMAKE_CURRENT_LIST_DIR}/cmake/custom/ArangoDB_FindOpenSSL_WIN32.cmake")
  if (NOT OPENSSL_FOUND)
    message ("System-wide attempt to find OpenSSL:")
    find_package(OpenSSL REQUIRED)
  endif ()
else ()
  find_package(OpenSSL REQUIRED)
endif ()

if (OPENSSL_FOUND)
  if (NOT "${OPENSSL_VERSION}" MATCHES "${ARANGODB_REQUIRED_OPENSSL_VERSION}")
    message (FATAL_ERROR "Wrong OpenSSL version was found: ${OPENSSL_VERSION}! Required version: ${MSG_ARANGODB_REQUIRED_OPENSSL_VERSION}!")
  endif ()
endif ()

message(${OPENSSL_INCLUDE_DIR})

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

add_definitions(-DBOOST_UUID_RANDOM_PROVIDER_FORCE_POSIX=1) #fix random provider
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
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/3rdParty/function2)
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

add_dependencies(rocksdb snappy-static)

if (USE_JEMALLOC)
  add_dependencies(rocksdb jemalloc_build)
  link_directories("${JEMALLOC_HOME}/lib")
endif ()

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
## PATHS, installation, packages, frontend
################################################################################

if (MSVC)
  # versioning
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/Installation/Windows/version")
  include(generate_product_version)
endif()

add_subdirectory(Documentation)
include(ArangoDBInstall)
include(frontend/aardvark)
if (NOT(SKIP_PACKAGING))
  include(packages/packages)
endif()

################################################################################
## ERRORS FILE
################################################################################

if (USE_MAINTAINER_MODE)
  # If "make clean" removes these files, afterwards neither "make" nor "cmake" work any more.
  set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM "On")

  set(ERROR_FILES
    lib/Basics/voc-errors.h
    lib/Basics/error-registry.h
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
      DEPENDS ${CMAKE_SOURCE_DIR}/${ERRORS_DAT} ./utils/generateErrorfile.py
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT  "Building errors files ${m}"
      VERBATIM
    )

    list(APPEND ERROR_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(errorfiles ALL DEPENDS ${ERROR_FILES_GEN})

  set(EXIT_CODE_FILES
    lib/Basics/exitcodes.h
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
      DEPENDS ${CMAKE_SOURCE_DIR}/${EXIT_CODES_DAT} ./utils/generateExitCodesFiles.py
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT  "Building exitcode files ${m}"
      VERBATIM
    )

    list(APPEND EXIT_CODE_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(exitcodefiles ALL DEPENDS ${EXIT_CODE_FILES_GEN})
endif ()

################################################################################
## SUB-PROJECTS
################################################################################

list(INSERT SYSTEM_LIBRARIES 0
  ${BT_LIBS}
  ${ZLIB_LIBS}
  ${ICU_LIBS}
  ${OPENSSL_LIBRARIES}
  ${BASE_LIBS}
  ${SYS_LIBS}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)


if (USE_GOOGLE_TESTS)
  add_subdirectory(tests)
endif ()

add_dependencies(arangobench   zlibstatic)
if (USE_ENTERPRISE)
  add_dependencies(arangobackup  zlibstatic)
endif ()
add_dependencies(arangod       zlibstatic)
add_dependencies(arangodump    zlibstatic)
add_dependencies(arangoexport  zlibstatic)
add_dependencies(arangoimport  zlibstatic)
add_dependencies(arangorestore zlibstatic)
add_dependencies(arangosh      zlibstatic)

if (UNIX)
  add_dependencies(arangobench   man)
  if (USE_ENTERPRISE)
    add_dependencies(arangobackup  man)
  endif ()
  add_dependencies(arangod       man)
  add_dependencies(arangodump    man)
  add_dependencies(arangoexport  man)
  add_dependencies(arangoimport  man)
  add_dependencies(arangorestore man)
  add_dependencies(arangosh      man)
endif ()

if (NOT USE_PRECOMPILED_V8)
  # all binaries depend on v8_build because it contains ICU as well
  add_dependencies(arangobench   v8_build)

  if (USE_ENTERPRISE)
    add_dependencies(arangobackup  v8_build)
  endif ()
  add_dependencies(arangod       v8_build)
  add_dependencies(arangodump    v8_build)
  add_dependencies(arangoexport  v8_build)
  add_dependencies(arangoimport  v8_build)
  add_dependencies(arangorestore v8_build)
  add_dependencies(arangosh      v8_build)
  if (USE_GOOGLE_TESTS)
    add_dependencies(arangodbtests v8_build)
  endif ()
endif ()

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

add_custom_target(arangodb
        DEPENDS arangod arangosh arangodump arangoexport arangoimport arangorestore)
