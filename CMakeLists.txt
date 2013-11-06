# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                    COMMON DEFINES
## -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE string "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

################################################################################
### @brief arangodb project
################################################################################

project(arangodb)

set(LIB_ARANGO        arango)
set(LIB_ARANGO_CLIENT arango_client)
set(LIB_ARANGO_FE     arango_fe)
set(LIB_ARANGO_MRUBY  arango_mruby)
set(LIB_ARANGO_V8     arango_v8)

set(BIN_ARANGOB       arangob)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOIMP     arangoimp)
set(BIN_ARANGOIRB     arangoirb)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)

set(SCRIPT_ARANGO_DFDB  arango-dfdb)
set(SCRIPT_FOXX_MANAGER foxx-manager)

set(TEST_BASICS_SUITE basics_suite)
set(TEST_GEO_SUITE    geo_suite)

################################################################################
### @brief threads
################################################################################

find_package(Threads)

################################################################################
### @brief GNU C/C++ compiler
################################################################################

if (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89 -g")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -g")
endif ()

if (APPLE)
  add_definitions("-Wno-deprecated-declarations")
endif ()

################################################################################
### @brief TRI_BITS
################################################################################

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
add_definitions("-DTRI_BITS=${BITS}")

## -----------------------------------------------------------------------------
## --SECTION--                                                           OPTIONS
## -----------------------------------------------------------------------------

################################################################################
### @brief ARANGODB_VERSION
################################################################################

set(ARANGODB_VERSION "1.x.y-rc1" CACHE path "ArangoDB version")

################################################################################
### @brief BUILD_PACKAGE
###
### Posible options are:
### - dmg-cli: Mac OS X shell client application
### - debian: debian package
### - raspbian: raspberry debian package
################################################################################

set(BUILD_PACKAGE "unknown" CACHE path "Package type to build")

################################################################################
### @brief MRUBY
################################################################################

option(USE_MRUBY "Do you want to use MRUBY" OFF)

if (USE_MRUBY)
  add_definitions("-DTRI_ENABLE_MRUBY=1")
endif ()

################################################################################
### @brief RAW_CONFIG
################################################################################

option(USE_RAW_CONFIG "Do you want to install configuration files with variables" OFF)

################################################################################
### @brief SYSTEM CONFIGURATION DIRECTORY (/etc)
################################################################################

set(ETCDIR "" CACHE path "System configuration directory (defaults to prefix/etc)")

if (ETCDIR STREQUAL "")
  set(ETCDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/etc/arangodb")
else ()
  set(ETCDIR_NATIVE "${ETCDIR}/arangodb")
endif ()

file(TO_NATIVE_PATH "${ETCDIR_NATIVE}" ETCDIR_NATIVE)

add_definitions("-D_ETCDIR_=\"${ETCDIR_NATIVE}\"")

################################################################################
### @brief DATA DIRECTORY (/var)
################################################################################

set(VARDIR "" CACHE path "System configuration directory (defaults to prefix/etc)")

if (VARDIR STREQUAL "")
  set(VARDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/var/arangodb")
else ()
  set(VARDIR_NATIVE "${VARDIR}/arangodb")
endif ()

file(TO_NATIVE_PATH "${VARDIR_NATIVE}" VARDIR_NATIVE)

################################################################################
### @brief TRI_BINDIR
################################################################################

set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")

################################################################################
### @brief TRI_DATABASEDIR
################################################################################

set(TRI_DATABASEDIR "${VARDIR_NATIVE}/lib/arangodb")

FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb")

################################################################################
### @brief TRI_LOGDIR
################################################################################

set(TRI_LOGDIR "${VARDIR_NATIVE}/log/arangodb")

FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb")

################################################################################
### @brief TRI_PKGDATADIR
################################################################################

set(TRI_PKGDATADIR "${CMAKE_INSTALL_PREFIX}/share/arangodb")

################################################################################
### @brief TRI_SBINDIR
################################################################################

set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(${PROJECT_SOURCE_DIR})

## -----------------------------------------------------------------------------
## --SECTION--                                                EXTERNAL LIBRARIES
## -----------------------------------------------------------------------------

################################################################################
### @brief ICU
################################################################################

set(ICU_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/icu/BUILD/include CACHE path "ICU include path")
set(ICU_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/icu/BUILD/libs CACHE path "ICU library path")
set(ICU_LIBS icui18n;icuuc;icudata;pthread;m CACHE path "ICU libraries")
set(ICU_VERSION "52.1" CACHE string "ICU version")

if (ICU_INCLUDE)
  include_directories(${ICU_INCLUDE})
endif ()

if (ICU_LIB_PATH)
  link_directories(${ICU_LIB_PATH})
endif ()

add_definitions("-DTRI_ICU_VERSION=\"${ICU_VERSION}\"")

################################################################################
### @brief LIBEV
################################################################################

set(LIBEV_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/libev CACHE path "LIBEV include path")
set(LIBEV_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/libev/BUILD/.libs CACHE path "LIBEV library path")
set(LIBEV_LIBS ev CACHE string "LIBEV libraries")
set(LIBEV_VERSION "4.11" CACHE string "LIBEV version")

if (LIBEV_INCLUDE)
  include_directories(${LIBEV_INCLUDE})
endif ()

if (LIBEV_LIB_PATH)
  link_directories(${LIBEV_LIB_PATH})
endif ()

add_definitions("-DTRI_LIBEV_VERSION=\"${LIBEV_VERSION}\"")

################################################################################
### @brief MRUBY
################################################################################

set(MRUBY_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/mruby/include CACHE path "MRUBY include path")
set(MRUBY_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/mruby/build/host/lib CACHE path "MRUBY library path")
set(MRUBY_LIBS mruby CACHE string "MRUBY libraries")
set(MRUBY_VERSION "2013-08-22 (aa099f7f7d1f8c61db936fb7c39a3c734175af16)" CACHE string "MRUBY version")

if (MRUBY_INCLUDE)
  include_directories(${MRUBY_INCLUDE})
endif ()

if (MRUBY_LIB_PATH)
  link_directories(${MRUBY_LIB_PATH})
endif ()

add_definitions("-DTRI_MRUBY_VERSION=\"${MRUBY_VERSION}\"")

################################################################################
### @brief OPENSSL
################################################################################

set(OPENSSL_INCLUDE "" CACHE path "OPENSSL include path")
set(OPENSSL_LIB_PATH "" CACHE path "OPENSSL library path")
set(OPENSSL_LIBS ssl;crypto CACHE string "OPENSSL libraries")
set(OPENSSL_VERSION "OpenSSL 0.9.8r 8 Feb 2011" CACHE string "OPENSSL version")

if (OPENSSL_INCLUDE)
  include_directories(${OPENSSL_INCLUDE})
endif ()

if (OPENSSL_LIB_PATH)
  link_directories(${OPENSSL_LIB_PATH})
endif ()

add_definitions("-DTRI_OPENSSL_VERSION=\"${OPENSSL_VERSION}\"")

################################################################################
### @brief READLINE
################################################################################

set(READLINE_INCLUDE "" CACHE path "READLINE include path")
set(READLINE_LIB_PATH "" CACHE path "READLINE library path")
set(READLINE_LIBS readline CACHE string "READLINE libraries")
set(READLINE_VERSION "0x0402.hex" CACHE string "READLINE version")

if (READLINE_INCLUDE)
  include_directories(${READLINE_INCLUDE})
endif ()

if (READLINE_LIB_PATH)
  link_directories(${READLINE_LIB_PATH})
endif ()

add_definitions("-DTRI_READLINE_VERSION=\"${READLINE_VERSION}\"")

################################################################################
### @brief V8
################################################################################

set(V8_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/V8/include CACHE path "V8 include path")
set(V8_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/V8/out/x64.release CACHE path "V8 library path")
set(V8_LIBS v8_base v8_nosnapshot CACHE string "V8 libraries")
set(V8_VERSION "3.16.14" CACHE string "V8 version")

if (V8_INCLUDE)
  include_directories(${V8_INCLUDE})
endif ()

if (V8_LIB_PATH)
  link_directories(${V8_LIB_PATH})
endif ()

add_definitions("-DTRI_V8_VERSION=\"${V8_VERSION}\"")

################################################################################
### @brief ZLIB
################################################################################

set(ZLIB_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/zlib-1.2.7 CACHE path "ZLIB include path")
set(ZLIB_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/zlib-1.2.7 CACHE path "ZLIB library path")
set(ZLIB_LIBS z CACHE string "ZLIB libraries")
set(ZLIB_VERSION "1.2.7" CACHE string "ZLIB version")

if (ZLIB_INCLUDE)
  include_directories(${ZLIB_INCLUDE})
endif ()

if (ZLIB_LIB_PATH)
  link_directories(${ZLIB_LIB_PATH})
endif ()

add_definitions("-DTRI_ZLIB_VERSION=\"${ZLIB_VERSION}\"")

## -----------------------------------------------------------------------------
## --SECTION--                                                      SUB-PROJECTS
## -----------------------------------------------------------------------------

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)

if (USE_MRUBY)
  add_subdirectory(arangoirb)
endif ()

add_subdirectory(utils)

## -----------------------------------------------------------------------------
## --SECTION--                                                    DOCUMENTATIONS
## -----------------------------------------------------------------------------

INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
        DESTINATION share/man)

## -----------------------------------------------------------------------------
## --SECTION--                                                             TESTS
## -----------------------------------------------------------------------------

enable_testing()

add_subdirectory(UnitTests)

## -----------------------------------------------------------------------------
## --SECTION--                                                          PACKAGES
## -----------------------------------------------------------------------------

set(CPACK_SET_DESTDIR ON)

################################################################################
### @brief general information
################################################################################

if (BUILD_PACKAGE STREQUAL "dmg-cli")
  set(CPACK_PACKAGE_NAME "ArangoDB-CLI")
elseif (BUILD_PACKAGE STREQUAL "raspbian")
  set(CPACK_PACKAGE_NAME "arangodb")
elseif (BUILD_PACKAGE STREQUAL "debian")
  set(CPACK_PACKAGE_NAME "arangodb")
endif ()


set(CPACK_PACKAGE_VENDOR  "triAGENS GmbH")
set(CPACK_PACKAGE_CONTACT "info@arangodb.org")
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")

################################################################################
### @brief MacOSX bundle
################################################################################

if (BUILD_PACKAGE STREQUAL "dmg-cli")
  set(CPACK_BUNDLE_NAME            "${CPACK_PACKAGE_NAME}")
  set(CPACK_BUNDLE_PLIST           "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/Info.plist")
  set(CPACK_BUNDLE_ICON            "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/icon.icns")
  set(CPACK_BUNDLE_STARTUP_COMMAND "${PROJECT_SOURCE_DIR}/Installation/MacOSX/Bundle/arangodb-cli.sh")
endif ()

################################################################################
### @brief debian package
################################################################################

if ((BUILD_PACKAGE STREQUAL "debian") OR (BUILD_PACKAGE STREQUAL "raspbian"))
  set(CPACK_DEBIAN_PACKAGE_SECTION "database")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
    "${PROJECT_SOURCE_DIR}/Installation/Debian/postinst;${PROJECT_SOURCE_DIR}/Installation/Debian/preinst;${PROJECT_SOURCE_DIR}/Installation/Debian/postrm;${PROJECT_SOURCE_DIR}/Installation/Debian/prerm;")

  install(
    FILES ${PROJECT_SOURCE_DIR}/Installation/Debian/rc.arangodb
    DESTINATION /etc/init.d
    RENAME arangodb)

  install(
    CODE
      "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E make_directory \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/share/arangodb/js/apps)")

  install(
    CODE
      "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink /var/lib/arangodb-apps \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/share/arangodb/js/apps/databases)")
endif ()

include(CPack)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
