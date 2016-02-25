# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# General
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0037)
  cmake_policy(SET CMP0037 NEW)
endif ()

if (APPLE)
  if (NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/clang)
  endif ()
  
  if (NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++)
  endif ()
endif ()

project(ArangoDB)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo
      CACHE string
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE
  )
endif ()

# where to find CMAKE modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

################################################################################
## ARANGODB
################################################################################

set(ARANGODB_VERSION_MAJOR      "3")
set(ARANGODB_VERSION_MINOR      "0")
set(ARANGODB_VERSION_REVISION   "0-devel")

set(ARANGODB_VERSION
    "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.${ARANGODB_VERSION_REVISION}")

string(TIMESTAMP ARANGODB_BUILD_DATE "%Y-%m-%d %H:%M:%S")
add_definitions("-DARANGODB_BUILD_DATE=\"${ARANGODB_BUILD_DATE}\"")

# for NSIS
set(ARANGODB_DISPLAY_NAME       "ArangoDB")
set(ARANGODB_URL_INFO_ABOUT     "https://www.arangodb.com")
set(ARANGODB_CONTACT            "hackers@arangodb.com")
set(ARANGODB_FRIENDLY_STRING    "ArangoDB - the multi-model database")

# MSVC
set(ARANGOB_FRIENDLY_STRING        "arangob - stress test program")
set(ARANGO_DUMP_FRIENDLY_STRING    "arangodump - export")
set(ARANGO_RESTORE_FRIENDLY_STRING "arangrestore - importer")
set(ARANGO_IMP_FRIENDLY_STRING     "arangoimp - TSV/CSV/JSON importer")
set(ARANGOSH_FRIENDLY_STRING       "arangosh - commandline client")

# libraries
set(LIB_ARANGO        arango)
set(LIB_ARANGO_V8     arango_v8)

# binaries
set(BIN_ARANGOB       arangob)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOIMP     arangoimp)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)

# test binaries
set(TEST_BASICS_SUITE basics_suite)
set(TEST_GEO_SUITE    geo_suite)

################################################################################
## VERSION FILES
################################################################################

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build.h"
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/VERSION.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
)

################################################################################
## OPERATION SYSTEM
################################################################################

if (WIN32)
  set(WINDOWS TRUE)
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
    set(MACOSX TRUE)
  endif ()
endif ()

################################################################################
## EXTERNAL PROGRAMS
################################################################################

if (SOLARIS)
  set(MAKE gmake)
else ()
  set(MAKE make)
endif()

find_package(FLEX)
find_package(BISON)
find_package(PythonInterp 2)

find_package(Go 1.2)

if (NOT GO_FOUND)
  message("Go version >=1.2 not found. etcd will not be built.")
endif ()

################################################################################
## COMPILER FEATURES
################################################################################

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_COMPILER_IS_CLANG 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  set(CMAKE_COMPILER_IS_CLANG 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  set(CMAKE_COMPILER_IS_INTEL 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_COMPILER_IS_INTEL 1)
endif ()

if (SOLARIS)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

  set(CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
    /lib;/lib64;/lib64;/usr/lib;/usr/lib64;/usr/lib64
  )

  list(APPEND SYSTEM_LIBRARIES nsl socket)

  # force 64bit compile
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I /opt/csw/include -D_REENTRANT -m64")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I /opt/csw/include -D_REENTRANT -m64")
endif ()

if (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif ()

# need c++11
set(CMAKE_CXX_STANDARD 11)
include(CheckCXX11Features)

# need threads
find_package(Threads REQUIRED)

if (APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

  if (CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    add_definitions("-Wno-deprecated-declarations")
  endif ()
endif ()

if (MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
  add_definitions("-DFD_SETSIZE=2048")
  add_definitions("-DUSE_REGEX_STATIC=1")
  add_definitions("-DU_STATIC_IMPLEMENTATION=1")

  foreach (flag_var
	   CMAKE_C_FLAGS
	   CMAKE_C_FLAGS_DEBUG
	   CMAKE_C_FLAGS_RELEASE
	   CMAKE_C_FLAGS_MINSIZEREL
	   CMAKE_C_FLAGS_RELWITHDEBINFO
           CMAKE_CXX_FLAGS
           CMAKE_CXX_FLAGS_DEBUG
           CMAKE_CXX_FLAGS_RELEASE
           CMAKE_CXX_FLAGS_MINSIZEREL
           CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if (${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MDd?" "" ${flag_var} "${${flag_var}}")
    endif ()

    if (flag_var MATCHES "DEBUG")
      set(${flag_var} "${${flag_var}} /MTd")
    else ()
      set(${flag_var} "${${flag_var}} /MT")
    endif ()
  endforeach()

  set(MSVC_LIBS crypt32.lib;WINMM.LIB;Ws2_32.lib;getopt;regex)

  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE /LTCG /SAFESEH:NO /ignore:4099"
  )
endif ()

include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/include)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/velocypack/include)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_BINARY_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/arangod)
include_directories(${PROJECT_SOURCE_DIR}/arangosh)
include_directories(${PROJECT_SOURCE_DIR}/lib)

if (CUSTOM_INCLUDES)
  include_directories(${CUSTOM_INCLUDES})
endif ()

if (MSVC)
  set(MSVC_INCLUDE
    ${PROJECT_SOURCE_DIR}/WindowsLibraries/${BITS}/include
    CACHE path
    "MSVC 3rd party include path"
  )

  include_directories(${MSVC_INCLUDE})

  set(MSVC_LIB_PATH
    ${PROJECT_SOURCE_DIR}/WindowsLibraries/${BITS}/lib
    CACHE path
    "MSVC 3rd party library path"
  )

  link_directories(${MSVC_LIB_PATH})
endif ()

################################################################################
## ARCHITECTURE
################################################################################

if (SOLARIS)
  set(CMAKE_SIZEOF_VOID_P 8)
  set(CMAKE_CXX_SIZEOF_DATA_PTR 8)
endif ()

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
add_definitions("-DARANGODB_BITS=${BITS}")

################################################################################
# BUILD TYPES
################################################################################

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_TL)

set(DEBUG FALSE)

if (CMAKE_BUILD_TYPE_TL MATCHES debug)
  set(DEBUG TRUE)
  set(WIN_RELEASE_TYPE Debug)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
elseif (CMAKE_BUILD_TYPE_TL MATCHES relwithdebinfo)
  set(WIN_RELEASE_TYPE RelWithDebInfo)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g")
elseif (CMAKE_BUILD_TYPE_TL MATCHES release)
  set(WIN_RELEASE_TYPE Release)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif () 

################################################################################
## TARGET ARCHITECTURE
################################################################################

include(TargetArch)

target_architecture(CMAKE_TARGET_ARCHITECTURES)
list(LENGTH CMAKE_TARGET_ARCHITECTURES cmake_target_arch_len)

if (NOT "${cmake_target_arch_len}" STREQUAL "1")
  set(CMAKE_TARGET_ARCHITECTURE_UNIVERSAL TRUE)
  set(CMAKE_TARGET_ARCHITECTURE_CODE "universal")
else ()
  set(CMAKE_TARGET_ARCHITECTURE_UNIVERSAL FALSE)
  set(CMAKE_TARGET_ARCHITECTURE_CODE "${CMAKE_TARGET_ARCHITECTURES}")
endif ()

include(VcMacros)

# we do not want to build specialized packages for one architecture
# include(OptimizeForArchitecture)
# OptimizeForArchitecture()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Vc_ARCHITECTURE_FLAGS}")

################################################################################
## BACKTRACE
################################################################################

option(USE_BACKTRACE
  "whether we should try to generate c-level stacktraces"
  OFF
)

if (USE_BACKTRACE)
  if (MSVC)
    set(BT_LIBS "Dbghelp" CACHE path "Debug Helper libraries")
  else ()
    set(BT_LIBS "" CACHE path "Debug Helper libraries")
  endif ()

  add_definitions("-DARANGODB_ENABLE_BACKTRACE=1")
else ()
  set(BT_LIBS "" CACHE path "Debug Helper libraries")
endif ()

################################################################################
## MAINTAINER MODE
################################################################################

option(USE_MAINTAINER_MODE
  "whether we want to have assertions and other development features"
  OFF
)

if (USE_MAINTAINER_MODE)
  add_definitions("-DARANGODB_ENABLE_MAINTAINER_MODE=1")
endif ()

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
## TCMALLOC
################################################################################

set(USE_TCMALLOC, TRUE)

if (WINDOWS OR SOLARIS)
  set(USE_TCMALLOC, FALSE)
endif ()

if (USE_TCMALLOC)
  find_package(tcmalloc)
endif()

################################################################################
## 3RD PARTY
################################################################################

# 3rdParty exports:
#
# BOOST_VERSION
# BOOST_INCLUDE_DIR
# USE_BOOST_UNITTESTS
#
# V8_VERSION
# V8_LIBS
# V8_INCLUDE_DIR
#
# ICU_VERSION
# ICU_LIBS
# ICU_INCLUDE_DIR
#
# LIBEV_VERSION
# LIBEV_LIBS
# LIBEV_INCLUDE_DIR
#
# ZLIB_VERSION
# ZLIB_LIBS
# ZLIB_INCLUDE_DIR

add_subdirectory(3rdParty)

################################################################################
## BOOST
################################################################################

include_directories(${BOOST_INCLUDE_DIR})
add_definitions(-DARANGODB_BOOST_VERSION=\"${BOOST_VERSION}\")

################################################################################
## ICU
################################################################################

include_directories(${ICU_INCLUDE_DIR})

################################################################################
## LIBEV
################################################################################

include_directories(${LIBEV_INCLUDE_DIR})
add_definitions("-DARANGODB_LIBEV_VERSION=\"${LIBEV_VERSION}\"")

################################################################################
## OPENSSL
################################################################################

include_directories(${OPENSSL_INCLUDE_DIR})
add_definitions(-DARANGODB_OPENSSL_VERSION=\"${BOOST_VERSION}\")

################################################################################
## V8
################################################################################

include_directories(${V8_INCLUDE_DIR})
add_definitions("-DARANGODB_V8_VERSION=\"${V8_VERSION}\"")

################################################################################
## ZLIB
################################################################################

include_directories(${ZLIB_INCLUDE_DIR})
add_definitions("-DARANGODB_ZLIB_VERSION=\"${ZLIB_VERSION}\"")

################################################################################
## PATHS
################################################################################

include(ArangoDBMacros)

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
      OUTPUT
        ${CMAKE_SOURCE_DIR}/${m}
      COMMAND
        ./utils/generateErrorfile.sh ./${ERRORS_DAT} ./${m}
      DEPENDS
        ${CMAKE_SOURCE_DIR}/${ERRORS_DAT}
      WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
      COMMENT
        "Building errors files ${m}"
      VERBATIM
    )

    list(APPEND ERROR_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(errorfiles ALL DEPENDS ${ERROR_FILES_GEN})
endif ()

################################################################################
## MIMETYPES FILE
################################################################################

if (USE_MAINTAINER_MODE)
  set(MIMETYPES_FILES
    lib/Basics/voc-mimetypes.h
    lib/Basics/voc-mimetypes.cpp
    js/common/modules/@arangodb/mimetypes.js
  )

  set(MIMETYPES_FILES_GEN)
  set(MIMETYPES_DAT lib/Basics/mimetypes.dat)

  foreach (m IN LISTS MIMETYPES_FILES)
    add_custom_command(
      OUTPUT
        ${CMAKE_SOURCE_DIR}/${m}
      COMMAND
        ./utils/generateMimetypes.sh ./${MIMETYPES_DAT} ./${m}
      DEPENDS
        ${CMAKE_SOURCE_DIR}/${MIMETYPES_DAT}
      WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
      COMMENT
        "Building mimetypes files ${m}"
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
  ${OPENSSL_LIBRARIES}
  ${V8_LIBS}
  ${ICU_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS}
)

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)
add_subdirectory(UnitTests)
add_subdirectory(Documentation)
