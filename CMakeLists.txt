# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# General
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0037)
  cmake_policy(SET CMP0037 NEW)
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo
      CACHE string
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE
  )
endif ()

project(ArangoDB)

if (APPLE)
  if (NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/clang)
  endif ()
  
  if (NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++)
  endif ()
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
  message ("Go version >=1.2 not found. etcd will not be built.")
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

  # make the SOLARIS ld call the GNU ld
  set(ENV{LD_ALTEXEC} /opt/csw/bin/gld)

  # force 64bit compile
  list(APPEND CMAKE_C_FLAGS -I\ /opt/csw/include\ -D_REENTRANT\ -m64)
  list(APPEND CMAKE_CXX_FLAGS -I\ /opt/csw/include\ -D_REENTRANT\ -m64\ -std=gnu++11)
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
add_definitions("-DTRI_BITS=${BITS}")

################################################################################
## TCMALLOC
################################################################################

set(USE_TCMALLOC, TRUE)

if (WINDOWS OR SOLARIS)
  set(USE_TCMALLOC, FALSE)
endif ()

################################################################################
# BUILD TYPES
################################################################################

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_TL)
message (${CMAKE_BUILD_TYPE_TL})

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

include(OptimizeForArchitecture)
OptimizeForArchitecture()

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

  add_definitions("-DHAVE_BACKTRACE=1")
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
  add_definitions("-DTRI_ENABLE_MAINTAINER_MODE=1")
endif ()

################################################################################
## FAILURE TESTS
################################################################################

option(USE_FAILURE_TESTS
  "whether we want to have failure tests compiled in"
  OFF
)

if (USE_FAILURE_TESTS)
  add_definitions("-DTRI_ENABLE_FAILURE_TESTS=1")
endif ()

################################################################################
## BOOST
################################################################################

option(AR_USE_SYSTEM_LIBS "use libraries provided by the system" OFF)

set(Boost_USE_MULTITHREADED ON)
set(BOOST_MINIMUM_VERSION 1.58)

set(BOOST_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/boost/1.58.0 CACHE path "BOOST include path")

set(Boost_INCLUDE_DIR ${BOOST_INCLUDE})
find_package(Boost COMPONENTS unit_test_framework)

if (${Boost_UNIT_TEST_FRAMEWORK_FOUND})
  option(USE_BOOST_UNITTESTS "use boost unit-tests" ON)
else ()
  option(USE_BOOST_UNITTESTS "use boost unit-tests" OFF)
endif ()

if (NOT USE_BOOST_UNITTESTS)
  message ("BOOST unit tests are disabled")
endif ()

find_package(Boost ${BOOST_MINIMUM_VERSION} REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

set(BOOST_VERSION
  "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}"
  CACHE INTERNAL
  "Boost: Version"
  FORCE
)

add_definitions(-DARANGODB_BOOST_VERSION=\"${BOOST_VERSION}\")

################################################################################
## OPENSSL
################################################################################

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

if (OPENSSL_VERSION)
  string(REPLACE "." ";" OPENSSL_VERSION_LIST ${OPENSSL_VERSION})
  list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
  list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)
  if (${OPENSSL_VERSION_MAJOR} GREATER 0 AND ${OPENSSL_VERSION_MINOR} GREATER 0)
    add_definitions(-DOPENSSL_NO_SSL2)
  endif ()				
endif ()

################################################################################
## 3RD PARTY
################################################################################

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
# LIBEV_VERSION
# LIBEV_LIBS
# LIBEV_INCLUDE_DIR
#
# ZLIB_VERSION
# ZLIB_LIBS
# ZLIB_INCLUDE_DIR

add_subdirectory(3rdParty)

################################################################################
## LIBEV
################################################################################

include_directories(${LIBEV_INCLUDE_DIR})
add_definitions("-DARANGODB_LIBEV_VERSION=\"${LIBEV_VERSION}\"")

################################################################################
## ZLIB
################################################################################

include_directories(${ZLIB_INCLUDE_DIR})
add_definitions("-DARANGODB_ZLIB_VERSION=\"${ZLIB_VERSION}\"")

################################################################################
## V8
################################################################################

include_directories(${V8_INCLUDE_DIR})
add_definitions("-DARANGODB_V8_VERSION=\"${V8_VERSION}\"")

################################################################################
## ICU
################################################################################

include_directories(${ICU_INCLUDE_DIR})

################################################################################
## 
################################################################################

################################################################################
## PATHS
################################################################################

option(USE_RELATIVE
  "Do you want to have all path are relative to the binary"
  OFF
)

if (USE_RELATIVE)

  # /etc -------------------------------
  set(ETCDIR_NATIVE "./etc/relative")
  set(ETCDIR_INSTALL "etc/relative")

  # etcd -------------------------------
  file(TO_NATIVE_PATH "${ETCDIR_NATIVE}" ETCDIR_NATIVE)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_NATIVE}")

  # /var -------------------------------
  set(VARDIR ""
    CACHE path
    "System configuration directory (defaults to prefix/var/arangodb)"
  )

  if (VARDIR STREQUAL "")
    set(VARDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/var")
    set(VARDIR_INSTALL "var")
  else ()
    set(VARDIR_NATIVE "${VARDIR}")
    set(VARDIR_INSTALL "${VARDIR}")
  endif ()

  file(TO_NATIVE_PATH "${VARDIR_NATIVE}" VARDIR_NATIVE)

  # database
  FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb")

  # apps
  FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb-apps")

  # logs
  FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb")

  # tri_package
  set(TRI_PKGDATADIR "${CMAKE_INSTALL_PREFIX}/share/arangodb")

  # resources
  set(TRI_RESOURCEDIR "resources")

  # bin dir ----------------------------
  set(ARANGODB_INSTALL_BIN "bin")
  set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")

  # MS stuff ---------------------------
  if (MSVC)
    set(ARANGODB_INSTALL_SBIN "bin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  else ()
    set(ARANGODB_INSTALL_SBIN "sbin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")
  endif ()

  add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")
else () 
  # etcd -------------------------------
  set(ETCDIR "" CACHE path "System configuration directory (defaults to prefix/etc)")

  # /etc -------------------------------
  if (ETCDIR STREQUAL "")
    set(ETCDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/etc/arangodb")
    set(ETCDIR_INSTALL "etc/arangodb")
  else ()
    set(ETCDIR_NATIVE "${ETCDIR}/arangodb")
    set(ETCDIR_INSTALL "${ETCDIR}/arangodb")
  endif ()

  # MS stuff ---------------------------
  if (MSVC)
    file(TO_NATIVE_PATH "${ETCDIR_INSTALL}" ETCDIR_INSTALL)
    STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_INSTALL}")
  else ()
    file(TO_NATIVE_PATH "${ETCDIR_NATIVE}" ETCDIR_NATIVE)
    STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_NATIVE}")
  endif ()
  
  add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")

  # /var
  set(VARDIR ""
    CACHE path
    "System configuration directory (defaults to prefix/var/arangodb)"
  )

  if (VARDIR STREQUAL "")
    set(VARDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/var")
    set(VARDIR_INSTALL "var")
  else ()
    set(VARDIR_NATIVE "${VARDIR}")
    set(VARDIR_INSTALL "${VARDIR}")
  endif ()

  file(TO_NATIVE_PATH "${VARDIR_NATIVE}" VARDIR_NATIVE)

  # database directory 
  FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb")

  # apps
  FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb-apps")

  # logs
  FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb")

  # package
  set(TRI_PKGDATADIR "${CMAKE_INSTALL_PREFIX}/share/arangodb")

  # resources
  set(TRI_RESOURCEDIR "resources")

  # binaries
  if (MSVC)
    set(ARANGODB_INSTALL_BIN "bin")
    set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  else ()
    set(ARANGODB_INSTALL_BIN "bin")
    set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  endif ()

  # sbinaries
  if (MSVC)
    set(ARANGODB_INSTALL_SBIN "bin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  else ()
    set(ARANGODB_INSTALL_SBIN "sbin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")
  endif ()
endif (USE_RELATIVE)

# MS Windows -------------------------------------------------------------------
if (MSVC)
  # icon paths 
  file(TO_NATIVE_PATH
    "${TRI_RESOURCEDIR}/Icons/arangodb.ico"
    RELATIVE_ARANGO_ICON
  )

  file(TO_NATIVE_PATH
    "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.bmp"
    ARANGO_IMG
  )

  file(TO_NATIVE_PATH
    "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.ico"
    ARANGO_ICON
  )

  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" RELATIVE_ARANGO_ICON "${RELATIVE_ARANGO_ICON}") 

  # versioning
  set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/Installation/Windows/version
  )

  include("${PROJECT_SOURCE_DIR}/Installation/Windows/version/generate_product_version.cmake")
endif ()

################################################################################
## INSTALL
################################################################################

if (NOT WINDOWS)
  install(
    PROGRAMS ${PROJECT_SOURCE_DIR}/bin/etcd-arango
    DESTINATION ${ARANGODB_INSTALL_BIN}
  )
endif ()

# Global macros ----------------------------------------------------------------
macro (generate_root_config name)
  FILE(READ ${PROJECT_SOURCE_DIR}/etc/arangodb/${name}.conf.in FileContent)
  STRING(REPLACE "@PKGDATADIR@" "@ROOTDIR@/share/arangodb"
    FileContent "${FileContent}") 
  STRING(REPLACE "@LOCALSTATEDIR@" "@ROOTDIR@/var"
    FileContent "${FileContent}")
  STRING(REPLACE "@SBINDIR@" "@ROOTDIR@/bin"
    FileContent "${FileContent}")
  STRING(REPLACE "@LIBEXECDIR@/arangodb" "@ROOTDIR@/bin"
    FileContent "${FileContent}")
  STRING(REPLACE "@SYSCONFDIR@" "@ROOTDIR@/etc/arangodb"
    FileContent "${FileContent}")
  if (MSVC)
    STRING(REPLACE "@PROGRAM_SUFFIX@" ".exe"
      FileContent "${FileContent}")
    STRING(REGEX REPLACE "[\r\n]file =" "\n# file =" 
      FileContent "${FileContent}")
  endif ()
  FILE(WRITE ${PROJECT_BINARY_DIR}/etc/arangodb/${name}.conf "${FileContent}")
endmacro ()

#  generates config file using the configured paths ----------------------------
macro (generate_path_config name)
  FILE(READ ${PROJECT_SOURCE_DIR}/etc/arangodb/${name}.conf.in FileContent)
  STRING(REPLACE "@PKGDATADIR@" "${TRI_PKGDATADIR}" 
    FileContent "${FileContent}")
  STRING(REPLACE "@LOCALSTATEDIR@" "${VARDIR_NATIVE}" 
    FileContent "${FileContent}")
  FILE(WRITE ${PROJECT_BINARY_DIR}/etc/arangodb/${name}.conf "${FileContent}")
endmacro ()

# installs a config file -------------------------------------------------------
macro (install_config name)
  if (MSVC OR DARWIN)
    generate_root_config(${name})
  else ()
    generate_path_config(${name})
  endif ()
  install(
    FILES ${PROJECT_BINARY_DIR}/etc/arangodb/${name}.conf
    DESTINATION ${ETCDIR_INSTALL})
endmacro ()

# installs a readme file converting EOL ----------------------------------------
macro (install_readme input where output)
  FILE(READ ${PROJECT_SOURCE_DIR}/${input} FileContent)
  STRING(REPLACE "\r" "" FileContent "${FileContent}")
  if (MSVC)
    STRING(REPLACE "\n" "\r\n" FileContent "${FileContent}")
  endif ()
  FILE(WRITE ${PROJECT_BINARY_DIR}/${output} "${FileContent}")
  install(
    FILES ${PROJECT_BINARY_DIR}/${output}
    DESTINATION ${where})
endmacro ()

# installs a link to an executable ---------------------------------------------
macro (install_command_alias name where alias)
  if (MSVC)
    add_custom_command(
      TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}>
	      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}.exe)
    install(
      PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}.exe
      DESTINATION ${where})
  else ()
    add_custom_command(
      TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink ${name}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}) 
    install(
      PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}
      DESTINATION ${where})
  endif ()
endmacro ()

################################################################################
## SUB-PROJECTS
################################################################################

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)
add_subdirectory(UnitTests)

get_cmake_property(_variableNames VARIABLES)





# ICU ---------------------------------------------------------------------------
# V8 ----------------------------------------------------------------------------

# Boost ------------------------------------------------------------------------

# Tcmalloc ---------------------------------------------------------------------
if (USE_TCMALLOC)
  find_package(tcmalloc)
endif()

# sub directories --------------------------------------------------------------

#if(BUILD_STATIC_EXECUTABLES)
#  set(CMAKE_EXE_LINKER_FLAGS -static)
#  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
#  set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
#  set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
#  set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
#  set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
#  set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
#  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
#  # Maybe this works as well, haven't tried yet.
#  # set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
#else(BUILD_STATIC_EXECUTABLES)
#  # Set RPATH to use for installed targets; append linker search path
#  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LOFAR_LIBDIR}")
#  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
#endif(BUILD_STATIC_EXECUTABLES) 


#--------------------------------------------------------------------------------
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach ()
#--------------------------------------------------------------------------------

# install ----------------------------------------------------------------------
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
  DESTINATION share/man)

if (MSVC)
  install_readme(README . README.txt)
  install_readme(README.md . README.md)
  install_readme(README.windows . README.windows.txt)
endif ()

if (MSVC)
  install_readme(LICENSE . LICENSE.txt)
  install_readme(LICENSES-OTHER-COMPONENTS.md . LICENSES-OTHER-COMPONENTS.md)
else ()
  install_readme(README share/doc/arangodb README)
  install_readme(README.md share/doc/arangodb README.md)
  install_readme(LICENSE share/doc/arangodb LICENSE)
  install_readme(LICENSES-OTHER-COMPONENTS.md share/doc/arangodb LICENSES-OTHER-COMPONENTS.md)
endif ()

# Build package ----------------------------------------------------------------
# Posible options are:
# - dmg-cli: Mac OS X shell client application
# - debian: debian package
set(BUILD_PACKAGE "unknown" CACHE path "Package type to build")

# OSX
if (BUILD_PACKAGE STREQUAL "dmg-cli")
  option(DARWIN "Building for MacOS" ON)
else ()
  option(DARWIN "Building for MacOS" OFF)
endif ()

# Windows
if (MSVC)
else ()
  set(CPACK_SET_DESTDIR ON)
endif ()

# General
set(CPACK_PACKAGE_VENDOR  "ArangoDB GmbH")
set(CPACK_PACKAGE_CONTACT "info@arangodb.org")
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_STRIP_FILES "ON")

if (DARWIN)
  set(CPACK_PACKAGE_NAME "ArangoDB-CLI")
elseif (BUILD_PACKAGE STREQUAL "debian")
  set(CPACK_PACKAGE_NAME "arangodb")
elseif (MSVC)
  set(CPACK_PACKAGE_NAME "ArangoDB")
endif ()

# debian
if (BUILD_PACKAGE STREQUAL "debian")

  set(CPACK_DEBIAN_PACKAGE_SECTION "database")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
    "${PROJECT_SOURCE_DIR}/Installation/Debian/postinst;${PROJECT_SOURCE_DIR}/Installation/Debian/preinst;${PROJECT_SOURCE_DIR}/Installation/Debian/postrm;${PROJECT_SOURCE_DIR}/Installation/Debian/prerm;")
  
  install(
    FILES ${PROJECT_SOURCE_DIR}/Installation/Debian/rc.arangodb
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION /etc/init.d
    RENAME arangodb)
  
  install(
    CODE
    "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E make_directory \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/share/arangodb/js/apps)")
  
  install(
    CODE
    "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink /var/lib/arangodb-apps \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/share/arangodb/js/apps/databases)")
endif ()

# OSX bundle 
if (DARWIN)
  set(CPACK_BUNDLE_NAME            "${CPACK_PACKAGE_NAME}")
  set(CPACK_BUNDLE_PLIST           "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/Info.plist")
  set(CPACK_BUNDLE_ICON            "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/icon.icns")
  set(CPACK_BUNDLE_STARTUP_COMMAND "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh")
endif ()

# MS installer
if (MSVC)
  set(CPACK_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Templates")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL 1)
  set(BITS 64)

  if (CMAKE_CL_64)
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    SET(BITS 64)
  else ()
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
    SET(BITS 32)
  endif ()

  message(STATUS "ARANGO_IMG:  ${ARANGO_IMG}")
  message(STATUS "ARANGO_ICON: ${ARANGO_ICON}")
  message(STATUS "RELATIVE_ARANGO_ICON: ${RELATIVE_ARANGO_ICON}")

  install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons"
    DESTINATION ${TRI_RESOURCEDIR})

  set(CPACK_NSIS_DEFINES "
    !define BITS ${BITS}
    !define TRI_FRIENDLY_SVC_NAME '${ARANGODB_FRIENDLY_STRING}'
    !define TRI_AARDVARK_URL 'http://127.0.0.1:8529'
    ")

  set(CPACK_PACKAGE_ICON             ${ARANGO_ICON})

  set(CPACK_NSIS_MODIFY_PATH         ON)
  set(CPACK_NSIS_MUI_ICON            ${ARANGO_ICON})
  set(CPACK_NSIS_MUI_UNIICON         ${ARANGO_ICON})
  set(CPACK_NSIS_INSTALLED_ICON_NAME ${RELATIVE_ARANGO_ICON})
  set(CPACK_NSIS_DISPLAY_NAME,       ${ARANGODB_DISPLAY_NAME})
  set(CPACK_NSIS_HELP_LINK           ${ARANGODB_HELP_LINK})
  set(CPACK_NSIS_URL_INFO_ABOUT      ${ARANGODB_URL_INFO_ABOUT})
  set(CPACK_NSIS_CONTACT             ${ARANGODB_CONTACT})

  # etcd
  if (CMAKE_CL_64)
    install(PROGRAMS WindowsLibraries/64/bin/etcd-arango.exe
            DESTINATION ${ARANGODB_INSTALL_SBIN})

    install(FILES WindowsLibraries/64/icudtl.dat
            DESTINATION share/arangodb
            RENAME icudt54l.dat)
  else ()
    install(PROGRAMS WindowsLibraries/32/bin/etcd-arango.exe
            DESTINATION ${ARANGODB_INSTALL_SBIN})

    install(FILES WindowsLibraries/32/icudtl.dat
            DESTINATION share/arangodb
            RENAME icudt54l.dat)
  endif ()

endif ()


# Custom targets ----------------------------------------------------------------

# swagger
add_custom_target (swagger
  COMMAND ${PYTHON_EXECUTABLE}
    ${PROJECT_SOURCE_DIR}/Documentation/Scripts/generateSwagger.py
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/api-docs api-docs
    ${PROJECT_SOURCE_DIR}/Documentation/DocuBlocks/Rest/
    > ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/api-docs.json)

# love
add_custom_target (love
  COMMENT "ArangoDB loves you."
  COMMAND ""
  )

 
# Finally: user cpack
include(CPack)


################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:

################################################################################
### @brief install server-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/js
  DESTINATION share/arangodb)

################################################################################
### @brief install log directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/log/arangodb
  DESTINATION ${VARDIR_INSTALL}/log)

################################################################################
### @brief install database directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb
  DESTINATION ${VARDIR_INSTALL}/lib)

################################################################################
### @brief install apps directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb-apps
  DESTINATION ${VARDIR_INSTALL}/lib)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "### @brief\\|## --SECTION--\\|# -\\*- "
## End:
