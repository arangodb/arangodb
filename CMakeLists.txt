# General -----------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)
project(ArangoDB)
set (ArangoDB_VERSION_MAJOR 3)
set (ArangoDB_VERSION_MINOR 0)

if(POLICY CMP0037)
  cmake_policy(SET CMP0037 NEW)
endif()

set(ARANGODB_DISPLAY_NAME       "ArangoDB")
set(ARANGODB_URL_INFO_ABOUT     "http://www.arangodb.org")
set(ARANGODB_CONTACT            "hackers@arangodb.org")
set(ARANGODB_VERSION            "3.0" CACHE path "ArangoDB version")

set(ARANGODB_FRIENDLY_STRING    "ArangoDB - the multi-model database")
set(ARANGOB_FRIENDLY_STRING     "ArangoBench - stresstester")
set(ARANGO_DUMP_FRIENDLY_STRING
  "arangodump - export whole databases from ArangoDB to JSON") 
set(ARANGO_IMP_FRIENDLY_STRING  "arangoimp - import CSV files into ArangoDB")
set(ARANGO_RESTORE_HELP_LINK
  "arangrestore - import dumpfiles created with arangodump into ArangoDB")
set(ARANGOSH_HELP_LINK          "arangosh - commandline client of ArangoDB")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE string
    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
    FORCE)
endif ()

# Where 2 find CMAKE modules
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# ArangoDB needs c++11
set(CMAKE_CXX_STANDARD 11)

# Need threads
find_package(Threads REQUIRED)

option(AR_USE_SYSTEM_LIBS "use libraries provided by the system" OFF)

# Object archives
set(LIB_ARANGO        arango)
set(LIB_ARANGO_CLIENT arango_client)
set(LIB_ARANGO_V8     arango_v8)

# Binaries
set(BIN_ARANGOB       arangob)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOIMP     arangoimp)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)

# Tests
set(TEST_BASICS_SUITE basics_suite)
set(TEST_GEO_SUITE    geo_suite)

# Google V8 engine including ICU
set(V8_VERSION        4.3.61)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/local-configuration.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/local-configuration.h")

# OS type (Convenience) --------------------------------------------------------
if(WIN32)
  if(NOT WINDOWS)
    set(WINDOWS TRUE)
  endif()
elseif(UNIX AND NOT APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(LINUX TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*")
    set(FREEBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
    set(NETBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(OPENBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES ".*GNU.*")
    set(GNU TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES ".*BSDI.*")
    set(BSDI TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly.*|FreeBSD")
    set(FREEBSD TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "SYSV5.*")
    set(SYSV5 TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Solaris.*")
    set(SOLARIS TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "HP-UX.*")
    set(HPUX TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "AIX.*")
    set(AIX TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Minix.*")
    set(MINIX TRUE)
  endif()
elseif(APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*")
    set(DARWIN TRUE)
  elseif(CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
    set(MACOSX TRUE)
  endif()
endif()

# TODO: Exclude non-supported?

## Compiler flags --------------------------------------------------------------
# GCC 
if (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89 -g -O2")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -O2")
endif ()
if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O2")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set (CMAKE_COMPILER_IS_CLANG 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  set (CMAKE_COMPILER_IS_INTEL 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set (CMAKE_COMPILER_IS_INTEL 1)
endif()

# OSX
if (APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -std=c++11")
  if (CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    add_definitions("-Wno-deprecated-declarations")
  endif()
endif ()

# Visual Studio
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
    endif()

    if (flag_var MATCHES "DEBUG")
      set(${flag_var} "${${flag_var}} /MTd")
    else ()
      set(${flag_var} "${${flag_var}} /MT")
    endif ()
  endforeach()

  set(MSVC_LIBS crypt32.lib;WINMM.LIB;Ws2_32.lib;getopt;regex)

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE /LTCG /SAFESEH:NO /ignore:4099")
endif ()

# Architecture
math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
add_definitions("-DTRI_BITS=${BITS}")

# Enable Backtrace -------------------------------------------------------------
option(USE_BACKTRACE "whether we should try to generate c-level stacktraces"
  OFF)
if (USE_BACKTRACE)
  if (MSVC)
    set(BT_LIBS "Dbghelp" CACHE path "Debug Helper libraries")
  else ()
    set(BT_LIBS "" CACHE path "Debug Helper libraries")
  endif()
  add_definitions("-DHAVE_BACKTRACE=1")
else()
  set(BT_LIBS "" CACHE path "Debug Helper libraries")
endif()

# Maintainer mode (Assertions) -------------------------------------------------
option(USE_MAINTAINER_MODE
  "whether we want to have assertions and other development features" OFF)
if (USE_MAINTAINER_MODE)
  add_definitions("-DTRI_ENABLE_MAINTAINER_MODE=1")
endif()

# Enable relative paths to binaries --------------------------------------------
option(USE_RELATIVE "Do you want to have all path are relative to the binary" OFF)

if (USE_RELATIVE)
  # /etc -------------------------------
  set(ETCDIR_NATIVE "./etc/relative")
  set(ETCDIR_INSTALL "etc/relative")
  # etcd -------------------------------
  file(TO_NATIVE_PATH "${ETCDIR_NATIVE}" ETCDIR_NATIVE)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED
    "${ETCDIR_NATIVE}")
  # /var -------------------------------
  set(VARDIR "" CACHE path
    "System configuration directory (defaults to prefix/var/arangodb)")
  if (VARDIR STREQUAL "")
    set(VARDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/var")
    set(VARDIR_INSTALL "var")
  else ()
    set(VARDIR_NATIVE "${VARDIR}")
    set(VARDIR_INSTALL "${VARDIR}")
  endif ()
  file(TO_NATIVE_PATH "${VARDIR_NATIVE}" VARDIR_NATIVE)
  # arango directories -----------------
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
  set(TRI_BINDIR_INSTALL "bin")
  set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  # MS stuff ---------------------------
  if (MSVC)
    set(TRI_SBINDIR_INSTALL "bin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  else ()
    set(TRI_SBINDIR_INSTALL "sbin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")
  endif ()
else(USE_RELATIVE) 
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
  endif()
  add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")
  # arango directories -----------------
  # /var
  set(VARDIR "" CACHE path "System configuration directory (defaults to prefix/var/arangodb)")
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
    set(TRI_BINDIR_INSTALL "bin")
    set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  else ()
    set(TRI_BINDIR_INSTALL "bin")
    set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  endif ()
  # sbinaries
  if (MSVC)
    set(TRI_SBINDIR_INSTALL "bin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/bin")
  else ()
    set(TRI_SBINDIR_INSTALL "sbin")
    set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")
  endif ()
endif (USE_RELATIVE)

# MS Windows -------------------------------------------------------------------
if (MSVC)
  # icon paths 
  file(TO_NATIVE_PATH "${TRI_RESOURCEDIR}/Icons/arangodb.ico"
    RELATIVE_ARANGO_ICON) 
  file(TO_NATIVE_PATH
    "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.bmp"
    ARANGO_IMG) 
  file(TO_NATIVE_PATH
    "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.ico"
    ARANGO_ICON) 
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" RELATIVE_ARANGO_ICON
    "${RELATIVE_ARANGO_ICON}") 
  # versioning
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/Installation/Windows/version) 
  include("${PROJECT_SOURCE_DIR}/Installation/Windows/version/generate_product_version.cmake")
endif ()

# etcd -------------------------------------------------------------------------
if (DARWIN)
  install(PROGRAMS ${PROJECT_SOURCE_DIR}/bin/etcd-arango DESTINATION
    ${TRI_BINDIR_INSTALL}) 
endif ()

# Global include directories ---------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR}/lib)
if (CUSTOM_INCLUDES)
  include_directories(${CUSTOM_INCLUDES})
endif()

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
  if ((${name} STREQUAL "arangod") AND RASPBIAN)
    STRING(REGEX REPLACE "[\r\n]threads =[^\r\n]*" "\nthreads = 1" 
      FileContent "${FileContent}")
    STRING(REGEX REPLACE "[\r\n]v8-contexts =[^\r\n]*" "\n8-contexts = 1" 
      FileContent "${FileContent}")
  endif ()
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

# Threads libraries -------------------------------------------------------------
find_package(Threads)

# Windows libraries -------------------------------------------------------------
if (MSVC)
  set(MSVC_INCLUDE
    ${PROJECT_SOURCE_DIR}/WindowsLibraries/${BITS}/include
    CACHE path "MSVC 3rd party include path") 
  set(MSVC_LIB_PATH ${PROJECT_SOURCE_DIR}/WindowsLibraries/${BITS}/lib
    CACHE path "MSVC 3rd party library path") 
endif ()
if (MSVC_INCLUDE)
  include_directories(${MSVC_INCLUDE})
endif ()
if (MSVC_LIB_PATH)
  link_directories(${MSVC_LIB_PATH})
endif ()

# Raspberry Pi ------------------------------------------------------------------
if (RASPBIAN)
  set(ARM_INCLUDE ${PROJECT_SOURCE_DIR}/ArmLibraries/include
    CACHE path "ARM 3rd party include path") 
  set(ARM_LIB_PATH ${PROJECT_SOURCE_DIR}/ArmLibraries/lib
    CACHE path "ARM 3rd party library path") 
endif ()
if (ARM_INCLUDE)
  include_directories(${ARM_INCLUDE})
endif ()
if (ARM_LIB_PATH)
  link_directories(${ARM_LIB_PATH})
endif ()

# ICU ---------------------------------------------------------------------------
if (MSVC)
  set(ICU_LIBS "icui18n;icuuc" CACHE path "ICU libraries")
  
  add_definitions("-DU_STATIC_IMPLEMENTATION=1")
elseif (RASPBIAN)
  set(ICU_LIBS icui18n;icuuc;icudata;pthread;dl;m CACHE path "ICU libraries")
else ()
  set(ICU_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/3rdParty/V8-${V8_VERSION}/third_party/icu/source/common;${PROJECT_SOURCE_DIR}/3rdParty/V8-${V8_VERSION}/third_party/icu/source/i18n;${PROJECT_SOURCE_DIR}/3rdParty/V8-${V8_VERSION}/third_party/icu/source/io CACHE path "ICU include path")
  set(ICU_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/V8-${V8_VERSION}/out/native CACHE path "ICU library path")
  if (LINUX)
    set(ICU_LIB_PATH "${ICU_LIB_PATH}/obj.target/third_party/icu")
  endif()
  set(ICU_LIBS icui18n;icuuc;icudata;pthread;dl;m CACHE path "ICU libraries")
endif ()

if (ICU_INCLUDE_DIR)
  include_directories(${ICU_INCLUDE_DIR})
endif ()

if (ICU_LIB_PATH)
  link_directories(${ICU_LIB_PATH})
endif ()

# V8 ----------------------------------------------------------------------------

if (MSVC OR RASPBIAN)
  set(V8_LIBS v8_base;v8_libbase;v8_libplatform;v8_nosnapshot CACHE string "V8 libraries")
else ()
  set(V8_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/3rdParty/V8-${V8_VERSION}/include CACHE path "V8 include path")
  set(V8_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/V8-${V8_VERSION}/out/native CACHE path "V8 library path")
  if (LINUX)
    set(V8_LIB_PATH "${V8_LIB_PATH}/obj.target/tools/gyp")
  endif()
  set(V8_LIBS v8_base;v8_libbase;v8_libplatform;v8_nosnapshot CACHE string "V8 libraries")
endif ()

if (V8_INCLUDE_DIR)
  include_directories(${V8_INCLUDE_DIR})
endif ()

if (V8_LIB_PATH)
  link_directories(${V8_LIB_PATH})
endif ()

add_definitions("-DTRI_V8_VERSION=\"${V8_VERSION}\"")


add_definitions("-DTRI_V8_VERSION=\"${V8_VERSION}\"")

# LibEV ------------------------------------------------------------------------
set(LIBEV_VERSION "unknown" CACHE string "LIBEV version")

if (MSVC OR RASPBIAN) 
  set(LIBEV_LIBS ev CACHE string "LIBEV libraries")
else ()
  set(LIBEV_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/libev CACHE path "LIBEV include path")
  set(LIBEV_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/libev/.libs CACHE path "LIBEV library path")
  set(LIBEV_LIBS ev CACHE string "LIBEV libraries")
endif ()

if (LIBEV_INCLUDE)
  include_directories(${LIBEV_INCLUDE})
endif ()

if (LIBEV_LIB_PATH)
  link_directories(${LIBEV_LIB_PATH})
endif ()

add_definitions("-DTRI_LIBEV_VERSION=\"${LIBEV_VERSION}\"")

# libnoise ---------------------------------------------------------------------
set(LINENOISE_VERSION "unknown" CACHE string "LINENOISE version")

# Boost ------------------------------------------------------------------------
find_package(Boost 1.48 COMPONENTS unit_test_framework)
if (${Boost_FOUND})
  include_directories(${Boost_INCLUDE_DIRS})
  add_definitions(-DBoost_MAJOR_VERSION=${Boost_MAJOR_VERSION})
  add_definitions(-DBoost_MINOR_VERSION=${Boost_MINOR_VERSION})
  link_directories (${BOOST_LIBRARYDIR})
else()
  message ("Boost unit test framework not found. Unit tests are disabled.")
endif()

# Tcmalloc ---------------------------------------------------------------------
find_package(tcmalloc)

# OpenSSL ----------------------------------------------------------------------
#set (OPENSSL_USE_STATIC_LIBS "TRUE")
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
if (OPENSSL_VERSION)
  string(REPLACE "." ";" OPENSSL_VERSION_LIST ${OPENSSL_VERSION})
  list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
  list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)
  if (${OPENSSL_VERSION_MAJOR} GREATER 0 AND ${OPENSSL_VERSION_MINOR} GREATER 0)
    add_definitions(-DOPENSSL_NO_SSL2)
  endif()				
endif()

# BISON&FLEX -------------------------------------------------------------------
find_package (BISON)

# FLEX -------------------------------------------------------------------------
find_package (FLEX)

# GO ---------------------------------------------------------------------------
find_package(Go 1.2)

# Python
find_package(PythonInterp 2)
set(ENV{PYTHON_EXECUTABLE} ${PYTHON_EXECUTABLE})

# zlib -------------------------------------------------------------------------
set(ZLIB_VERSION "unknown" CACHE string "ZLIB version")

if (MSVC OR RASPBIAN)
  set(ZLIB_LIBS z CACHE string "ZLIB libraries")
else ()
  set(ZLIB_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/zlib-1.2.7
    CACHE path "ZLIB include path")
  set(ZLIB_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/zlib-1.2.7
    CACHE path "ZLIB library path")
  set(ZLIB_LIBS z CACHE string "ZLIB libraries")
endif ()

if (ZLIB_INCLUDE)
  include_directories(${ZLIB_INCLUDE})
endif ()

if (ZLIB_LIB_PATH)
  link_directories(${ZLIB_LIB_PATH})
endif ()

add_definitions("-DTRI_ZLIB_VERSION=\"${ZLIB_VERSION}\"")

# 3rd party libraries
add_subdirectory(3rdParty)

# sub directories --------------------------------------------------------------
add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)
add_subdirectory(js)

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

# Enable testing ---------------------------------------------------------------
enable_testing()

# Test definitions -------------------------------------------------------------
add_subdirectory(UnitTests)


# Build package ----------------------------------------------------------------
# Posible options are:
# - dmg-cli: Mac OS X shell client application
# - debian: debian package
# - raspbian: raspberry debian package
set(BUILD_PACKAGE "unknown" CACHE path "Package type to build")

# Raspberry Pi
if (BUILD_PACKAGE STREQUAL "raspbian")
  add_definitions("-DTRI_FORCE_ARMV6")
  option(RASPBIAN "Building for Raspberry PI" ON)
else ()
  option(RASPBIAN "Building for Raspberry PI" OFF)
endif ()

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
elseif (RASPBIAN)
  set(CPACK_PACKAGE_NAME "arangodb")
elseif (BUILD_PACKAGE STREQUAL "debian")
  set(CPACK_PACKAGE_NAME "arangodb")
elseif (MSVC)
  set(CPACK_PACKAGE_NAME "ArangoDB")
endif ()

# debian
if ((BUILD_PACKAGE STREQUAL "debian") OR RASPBIAN)
  if (RASPBIAN)
    set(CPACK_SYSTEM_NAME "raspbian")
  endif ()

  set(CPACK_DEBIAN_PACKAGE_SECTION "database")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
    "${PROJECT_SOURCE_DIR}/Installation/ARM/postinst;${PROJECT_SOURCE_DIR}/Installation/ARM/preinst;${PROJECT_SOURCE_DIR}/Installation/ARM/postrm;${PROJECT_SOURCE_DIR}/Installation/ARM/prerm;")
  
  install(
    FILES ${PROJECT_SOURCE_DIR}/Installation/ARM/rc.arangodb
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

  if(CMAKE_CL_64)
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    SET(BITS 64)
  else()
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
    SET(BITS 32)
  endif()

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
  if(CMAKE_CL_64)
    install(PROGRAMS WindowsLibraries/64/bin/etcd-arango.exe
            DESTINATION ${TRI_SBINDIR_INSTALL})

    install(FILES WindowsLibraries/64/icudtl.dat
            DESTINATION share/arangodb
            RENAME icudt54l.dat)
  else()
    install(PROGRAMS WindowsLibraries/32/bin/etcd-arango.exe
            DESTINATION ${TRI_SBINDIR_INSTALL})

    install(FILES WindowsLibraries/32/icudtl.dat
            DESTINATION share/arangodb
            RENAME icudt54l.dat)
  endif()

endif ()

# Finally: user cpack
include(CPack)
