# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                    COMMON DEFINES
## -----------------------------------------------------------------------------

################################################################################
### @brief required cmake version
################################################################################

cmake_minimum_required(VERSION 2.6)

################################################################################
### @brief cmake build types
################################################################################

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

set(TEST_BASICS_SUITE basics_suite)
set(TEST_GEO_SUITE    geo_suite)

################################################################################
### @brief GNU C/C++ compiler
################################################################################

if (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89 -g")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
endif ()

if (APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
  add_definitions("-Wno-deprecated-declarations")
endif ()

################################################################################
### @brief Visual Studio Compiler
################################################################################

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

################################################################################
### @brief TRI_BITS
################################################################################

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
add_definitions("-DTRI_BITS=${BITS}")

################################################################################
### @brief raspberry pi
################################################################################

if (BUILD_PACKAGE STREQUAL "raspbian")
  add_definitions("-DTRI_FORCE_ARMV6")
endif ()

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
### @brief VISTA LOCKS
################################################################################

option(USE_VISTA_LOCKS "Do you want to use Windows Vista R/W locks" OFF)

if (USE_VISTA_LOCKS)
  add_definitions("-DTRI_WINDOWS_VISTA_LOCKS=1")
endif ()

################################################################################
### @brief SYSTEM CONFIGURATION DIRECTORY (/etc)
################################################################################

set(ETCDIR "" CACHE path "System configuration directory (defaults to prefix/etc)")

if (ETCDIR STREQUAL "")
  set(ETCDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/etc/arangodb")
  set(ETCDIR_INSTALL "etc/arangodb")
else ()
  set(ETCDIR_NATIVE "${ETCDIR}/arangodb")
  set(ETCDIR_INSTALL "${ETCDIR}/arangodb")
endif ()

file(TO_NATIVE_PATH "${ETCDIR_NATIVE}" ETCDIR_NATIVE)
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ETCDIR_ESCAPED "${ETCDIR_NATIVE}")

add_definitions("-D_SYSCONFDIR_=\"${ETCDIR_ESCAPED}\"")

################################################################################
### @brief DATA DIRECTORY (/var)
################################################################################

set(VARDIR "" CACHE path "System configuration directory (defaults to prefix/var/arangodb)")

if (VARDIR STREQUAL "")
  set(VARDIR_NATIVE "${CMAKE_INSTALL_PREFIX}/var")
  set(VARDIR_INSTALL "var")
else ()
  set(VARDIR_NATIVE "${VARDIR}")
  set(VARDIR_INSTALL "${VARDIR}")
endif ()

file(TO_NATIVE_PATH "${VARDIR_NATIVE}" VARDIR_NATIVE)

## -----------------------------------------------------------------------------
## --SECTION--                                                       DIRECTORIES
## -----------------------------------------------------------------------------

################################################################################
### @brief database directory
################################################################################

FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb")

################################################################################
### @brief apps directory
################################################################################

FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/lib/arangodb-apps")

################################################################################
### @brief log directory
################################################################################

FILE(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/var/log/arangodb")

################################################################################
### @brief TRI_PKGDATADIR
################################################################################

set(TRI_PKGDATADIR "${CMAKE_INSTALL_PREFIX}/share/arangodb")

################################################################################
### @brief TRI_RESOURCEDIR
################################################################################

set(TRI_RESOURCEDIR "resources")

################################################################################
### @brief TRI_BINDIR
################################################################################

if (MSVC)
  set(TRI_BINDIR_INSTALL "bin")
  set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
else ()
  set(TRI_BINDIR_INSTALL "bin")
  set(TRI_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
endif ()

################################################################################
### @brief TRI_SBINDIR
################################################################################

if (MSVC)
  set(TRI_SBINDIR_INSTALL "bin")
  set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/bin")
else ()
  set(TRI_SBINDIR_INSTALL "sbin")
  set(TRI_SBINDIR "${CMAKE_INSTALL_PREFIX}/sbin")
endif ()

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

################################################################################
### @brief project source
################################################################################

include_directories(${PROJECT_SOURCE_DIR})

## -----------------------------------------------------------------------------
## --SECTION--                                                     COMMON MACROS
## -----------------------------------------------------------------------------

################################################################################
### @brief generates config file using ROOTDIR
################################################################################

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

################################################################################
### @brief generates config file using the configured paths
################################################################################

macro (generate_path_config name)
  FILE(READ ${PROJECT_SOURCE_DIR}/etc/arangodb/${name}.conf.in FileContent)

  STRING(REPLACE "@PKGDATADIR@" "${TRI_PKGDATADIR}" 
         FileContent "${FileContent}")

  STRING(REPLACE "@LOCALSTATEDIR@" "${VARDIR_NATIVE}" 
         FileContent "${FileContent}")

  if ((${name} STREQUAL "arangod") AND (BUILD_PACKAGE STREQUAL "raspbian"))
    STRING(REGEX REPLACE "[\r\n]threads =[^\r\n]*" "\nthreads = 1" 
           FileContent "${FileContent}")
  endif ()

  FILE(WRITE ${PROJECT_BINARY_DIR}/etc/arangodb/${name}.conf "${FileContent}")
endmacro ()

################################################################################
### @brief installs a config file
################################################################################

macro (install_config name)
  if (MSVC OR (BUILD_PACKAGE STREQUAL "dmg-cli"))
    generate_root_config(${name})
  else ()
    generate_path_config(${name})
  endif ()

  install(
    FILES ${PROJECT_BINARY_DIR}/etc/arangodb/${name}.conf
    DESTINATION ${ETCDIR_INSTALL})
endmacro ()

################################################################################
### @brief installs a readme file converting EOL
################################################################################

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

################################################################################
### @brief installs a link to an executable
################################################################################

macro (install_command_alias name where alias)
  if (MSVC)

  add_custom_command(
    TARGET ${name}
    POST_BUILD
      COMMAND 
	${CMAKE_COMMAND} -E copy
	  $<TARGET_FILE:${name}>
	  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}.exe)

    install(
      PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}.exe
      DESTINATION ${where})

  else ()

  add_custom_command(
    TARGET ${name}
    POST_BUILD
      COMMAND 
	${CMAKE_COMMAND} -E create_symlink
	  ${name}
	  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias})

    install(
      PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${alias}
      DESTINATION ${where})

  endif ()
endmacro ()

## -----------------------------------------------------------------------------
## --SECTION--                                                EXTERNAL LIBRARIES
## -----------------------------------------------------------------------------

################################################################################
### @brief threads
################################################################################

find_package(Threads)

################################################################################
### @brief windows
################################################################################

if (MSVC)
  set(MSVC_INCLUDE ${PROJECT_SOURCE_DIR}/WindowsLibraries/${BITS}/include CACHE path "MSVC 3rd party include path")
  set(MSVC_LIB_PATH ${PROJECT_SOURCE_DIR}/WindowsLibraries/${BITS}/lib CACHE path "MSVC 3rd party library path")
endif ()

if (MSVC_INCLUDE)
  include_directories(${MSVC_INCLUDE})
endif ()

if (MSVC_LIB_PATH)
  link_directories(${MSVC_LIB_PATH})
endif ()

################################################################################
### @brief raspberry pi
################################################################################

if (BUILD_PACKAGE STREQUAL "raspbian")
  set(ARM_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty-ARM/include CACHE path "ARM 3rd party include path")
  set(ARM_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty-ARM/lib CACHE path "ARM 3rd party library path")
endif ()

if (ARM_INCLUDE)
  include_directories(${ARM_INCLUDE})
endif ()

if (ARM_LIB_PATH)
  link_directories(${ARM_LIB_PATH})
endif ()

################################################################################
### @brief ICU
################################################################################

set(ICU_VERSION "52.1" CACHE string "ICU version")

if (MSVC)
  set(ICU_LIBS "icui18n;icuuc;icudata" CACHE path "ICU libraries")

  add_definitions("-DU_STATIC_IMPLEMENTATION=1")
elseif (BUILD_PACKAGE STREQUAL "raspbian")
  set(ICU_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty-ARM/icu/include CACHE path "ICU include path")
  set(ICU_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty-ARM/icu/lib CACHE path "ICU library path")
  set(ICU_LIBS icui18n;icuuc;icudata;pthread;dl;m CACHE path "ICU libraries")
else ()
  set(ICU_INCLUDE1 ${PROJECT_SOURCE_DIR}/3rdParty/V8/third_party/icu/source/common CACHE path "ICU include path")
  set(ICU_INCLUDE2 ${PROJECT_SOURCE_DIR}/3rdParty/V8/third_party/icu/source/i18n CACHE path "ICU include path")
  set(ICU_INCLUDE3 ${PROJECT_SOURCE_DIR}/3rdParty/V8/third_party/icu/source/io CACHE path "ICU include path")
  set(ICU_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/V8/out/$V8_TARGET CACHE path "ICU library path")
  set(ICU_LIBS icui18n;icuuc;icudata;pthread;dl;m CACHE path "ICU libraries")
endif ()

if (ICU_INCLUDE)
  include_directories(${ICU_INCLUDE})
endif ()

if (ICU_INCLUDE1)
  include_directories(${ICU_INCLUDE1})
endif ()

if (ICU_INCLUDE2)
  include_directories(${ICU_INCLUDE2})
endif ()

if (ICU_INCLUDE3)
  include_directories(${ICU_INCLUDE3})
endif ()

if (ICU_LIB_PATH)
  link_directories(${ICU_LIB_PATH})
endif ()

add_definitions("-DTRI_ICU_VERSION=\"${ICU_VERSION}\"")

################################################################################
### @brief LIBEV
################################################################################

if (MSVC OR (BUILD_PACKAGE STREQUAL "raspbian")) 
  set(LIBEV_LIBS ev CACHE string "LIBEV libraries")
  set(LIBEV_VERSION "4.11" CACHE string "LIBEV version")
else ()
  set(LIBEV_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/libev CACHE path "LIBEV include path")
  set(LIBEV_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/libev/BUILD/.libs CACHE path "LIBEV library path")
  set(LIBEV_LIBS ev CACHE string "LIBEV libraries")
  set(LIBEV_VERSION "4.11" CACHE string "LIBEV version")
endif ()

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

if (MSVC OR (BUILD_PACKAGE STREQUAL "raspbian"))
else ()
  set(MRUBY_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/mruby/include CACHE path "MRUBY include path")
  set(MRUBY_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/mruby/build/host/lib CACHE path "MRUBY library path")
  set(MRUBY_LIBS mruby CACHE string "MRUBY libraries")
  set(MRUBY_VERSION "2013-08-22 (aa099f7f7d1f8c61db936fb7c39a3c734175af16)" CACHE string "MRUBY version")
endif ()

if (USE_MRUBY)
  if (MRUBY_INCLUDE)
    include_directories(${MRUBY_INCLUDE})
  endif ()

  if (MRUBY_LIB_PATH)
    link_directories(${MRUBY_LIB_PATH})
  endif ()

  add_definitions("-DTRI_MRUBY_VERSION=\"${MRUBY_VERSION}\"")
endif ()

################################################################################
### @brief OPENSSL
################################################################################

if (MSVC)
  set(OPENSSL_LIBS ssleay;libeay CACHE string "OPENSSL libraries")
  set(OPENSSL_VERSION "OpenSSL 1.0.1e" CACHE string "OPENSSL version")
elseif (BUILD_PACKAGE STREQUAL "raspbian")
  set(OPENSSL_LIBS ssl;crypto CACHE string "OPENSSL libraries")
  set(OPENSSL_VERSION "OpenSSL 0.9.8r 8 Feb 2011" CACHE string "OPENSSL version")
else ()
  set(OPENSSL_INCLUDE "" CACHE path "OPENSSL include path")
  set(OPENSSL_LIB_PATH "" CACHE path "OPENSSL library path")
  set(OPENSSL_LIBS ssl;crypto CACHE string "OPENSSL libraries")
  set(OPENSSL_VERSION "OpenSSL 0.9.8r 8 Feb 2011" CACHE string "OPENSSL version")
endif ()

if (OPENSSL_INCLUDE)
  include_directories(${OPENSSL_INCLUDE})
endif ()

if (OPENSSL_LIB_PATH)
  link_directories(${OPENSSL_LIB_PATH})
endif ()

add_definitions("-DTRI_OPENSSL_VERSION=\"${OPENSSL_VERSION}\"")

################################################################################
### @brief READLINE or LINENOISE
################################################################################

if (MSVC)
  set(READLINE_LIBS linenoise CACHE string "LINENOISE libraries")
  set(READLINE_VERSION "linenoise" CACHE string "LINENOISE version")
else ()
  set(READLINE_INCLUDE "" CACHE path "READLINE include path")
  set(READLINE_LIB_PATH "" CACHE path "READLINE library path")
  set(READLINE_LIBS readline CACHE string "READLINE libraries")
  set(READLINE_VERSION "0x0402.hex" CACHE string "READLINE version")
endif ()

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

set(V8_VERSION "3.29.59" CACHE string "V8 version")

if (MSVC OR (BUILD_PACKAGE STREQUAL "raspbian"))
  set(V8_LIBS v8_base;v8_nosnapshot CACHE string "V8 libraries")
else ()
  set(V8_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/V8/include CACHE path "V8 include path")
  set(V8_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/V8/out/x64.release CACHE path "V8 library path")
  set(V8_LIBS v8_base;v8_libbase;v8_libplatform v8_nosnapshot CACHE string "V8 libraries")
endif ()

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

if (MSVC OR (BUILD_PACKAGE STREQUAL "raspbian"))
  set(ZLIB_LIBS z CACHE string "ZLIB libraries")
  set(ZLIB_VERSION "1.2.7" CACHE string "ZLIB version")
else ()
  set(ZLIB_INCLUDE ${PROJECT_SOURCE_DIR}/3rdParty/zlib-1.2.7 CACHE path "ZLIB include path")
  set(ZLIB_LIB_PATH ${PROJECT_SOURCE_DIR}/3rdParty/zlib-1.2.7 CACHE path "ZLIB library path")
  set(ZLIB_LIBS z CACHE string "ZLIB libraries")
  set(ZLIB_VERSION "1.2.7" CACHE string "ZLIB version")
endif ()

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

################################################################################
### @brief lib
################################################################################

add_subdirectory(lib)

################################################################################
### @brief arangosh
################################################################################

add_subdirectory(arangosh)

################################################################################
### @brief arangod
################################################################################

add_subdirectory(arangod)

################################################################################
### @brief arangoirb
################################################################################

if (USE_MRUBY)
  add_subdirectory(arangoirb)
endif ()

## -----------------------------------------------------------------------------
## --SECTION--                                                    DOCUMENTATIONS
## -----------------------------------------------------------------------------

################################################################################
### @brief install manual
################################################################################

install(DIRECTORY ${PROJECT_SOURCE_DIR}/Documentation/man/
        DESTINATION share/man)

################################################################################
### @brief install arangod readme
################################################################################

if (MSVC)
  install_readme(README . README.txt)
  install_readme(README.md . README.md)
  install_readme(README.windows . README.windows.txt)
endif ()

################################################################################
### @brief install arangod license
################################################################################

if (MSVC)
  install_readme(LICENSE . LICENSE.txt)
endif ()

## -----------------------------------------------------------------------------
## --SECTION--                                                             TESTS
## -----------------------------------------------------------------------------

################################################################################
### @brief enable testing
################################################################################

enable_testing()

################################################################################
### @brief load test definition
################################################################################

add_subdirectory(UnitTests)

## -----------------------------------------------------------------------------
## --SECTION--                                                          PACKAGES
## -----------------------------------------------------------------------------

################################################################################
### @brief general information
################################################################################

if (MSVC)
else ()
  set(CPACK_SET_DESTDIR ON)
endif ()

set(CPACK_PACKAGE_VENDOR  "triAGENS GmbH")
set(CPACK_PACKAGE_CONTACT "info@arangodb.org")
set(CPACK_PACKAGE_VERSION "${ARANGODB_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_STRIP_FILES "ON")

if (BUILD_PACKAGE STREQUAL "dmg-cli")
  set(CPACK_PACKAGE_NAME "ArangoDB-CLI")
elseif (BUILD_PACKAGE STREQUAL "raspbian")
  set(CPACK_PACKAGE_NAME "arangodb")
elseif (BUILD_PACKAGE STREQUAL "debian")
  set(CPACK_PACKAGE_NAME "arangodb")
elseif (MSVC)
  set(CPACK_PACKAGE_NAME "ArangoDB")
endif ()

################################################################################
### @brief debian package
################################################################################

if ((BUILD_PACKAGE STREQUAL "debian") OR (BUILD_PACKAGE STREQUAL "raspbian"))
  if (BUILD_PACKAGE STREQUAL "raspbian")
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
### @brief Micrsoft Installer
################################################################################

if (MSVC)

  ##############################################################################
  ## Initializing variables
  ##############################################################################

  set(CPACK_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Installation/Windows/Templates")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL 1)
  set(BITS 64)

  if(CMAKE_CL_64)
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    SET(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}")
    SET(BITS 64)
  else()
    SET(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
    SET(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}")
    SET(BITS 32)
  endif()

  SET(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS  "SetRegView ${BITS}")

  # SetRegView controlls where die regkeys are written to
  # SetRegView 32 writes the keys into Wow6432
  # this variable was defined by eld and included in NSIS.template.in
  SET(CPACK_NSIS_EXTRA_PREUNINSTALL_COMMANDS  
      "  SetRegView ${BITS}
         SimpleSC::StopService  'ArangoDB' 0 30
         SimpleSC::RemoveService  'ArangoDB' 
       "
   )

  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.bmp" ARANGO_IMG)
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons/arangodb.ico" ARANGO_ICON)

  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_IMG "${ARANGO_IMG}")
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" ARANGO_ICON "${ARANGO_ICON}")

  message(STATUS "ARANGO_IMG:  ${ARANGO_IMG}")
  message(STATUS "ARANGO_ICON: ${ARANGO_ICON}")

  install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/Installation/Windows/Icons"
    DESTINATION ${TRI_RESOURCEDIR})

  set(CPACK_PACKAGE_ICON ${ARANGO_ICON})

  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_MUI_ICON ${ARANGO_ICON})
  set(CPACK_NSIS_MUI_UNIICON ${ARANGO_ICON})
  set(CPACK_NSIS_INSTALLED_ICON_NAME ${ARANGO_ICON})
  
  set(CPACK_NSIS_CREATE_ICONS_EXTRA 
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Arango Shell.lnk'  '$INSTDIR\\\\bin\\\\arangosh.exe'  '' '$INSTDIR\\\\resources\\\\Icons\\\\arangodb.ico' '0' SW_SHOWMAXIMIZED
     CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Arango Server.lnk'  '$INSTDIR\\\\bin\\\\arangod.exe'  '' '$INSTDIR\\\\resources\\\\Icons\\\\arangodb.ico' '0' SW_SHOWMAXIMIZED
    ")

  # Note, short cuts are created in $SMPROGRAMS\$STARTMENU_FOLDER but
  # they are deleted from '$SMPROGRAMS\$MUI_TEMP
  set(CPACK_NSIS_DELETE_ICONS_EXTRA 
    "Delete  '$SMPROGRAMS\\\\$MUI_TEMP\\\\Arango Server.lnk'
     Delete  '$SMPROGRAMS\\\\$MUI_TEMP\\\\Arango Shell.lnk'
    ")

  set(CPACK_NSIS_DISPLAY_NAME, "ArangoDB")
  set(CPACK_NSIS_HELP_LINK "http://www.arangodb.org")
  set(CPACK_NSIS_URL_INFO_ABOUT  "http://www.arangodb.org")
  set(CPACK_NSIS_CONTACT "hackers@arangodb.org")

  ##############################################################################
  ## etcd
  ##############################################################################

  if(CMAKE_CL_64)
    install(PROGRAMS WindowsLibraries/64/bin/etcd-arango.exe
            DESTINATION ${TRI_SBINDIR_INSTALL})
  else()
    install(PROGRAMS WindowsLibraries/32/bin/etcd-arango.exe
            DESTINATION ${TRI_SBINDIR_INSTALL})
  endif()

endif ()

################################################################################
### @brief use CPack
################################################################################

include(CPack)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "### @brief\\|## --SECTION--\\|# -\\*- "
## End:
