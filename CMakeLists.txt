# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                    COMMON DEFINES
## -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE string "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

project(arangodb)

SET(EXECUTABLE_OUTPUT_PATH bin)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -g")

set(LIB_ARANGO         arango)
set(LIB_ARANGO_CLIENT  arango_client)
set(LIB_ARANGO_FE      arango_fe)
set(LIB_ARANGO_V8      arango_v8)

set(BIN_ARANGOSH  arangosh)

add_definitions("-DTRI_BITS=64")


add_definitions("-D_REENTRANT") 
add_definitions("-D_THREAD_SAFE") 
add_definitions("-DHAVE_CONFIG_H") 
add_definitions("-D_SYSCONFDIR_=\"/usr/local/etc/arangodb\"")

## -----------------------------------------------------------------------------
## --SECTION--                                                           OPTIONS
## -----------------------------------------------------------------------------

option(USE_MRUBY "Do you want to use MRUBY")

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
set(ICU_VERSION "49.1.2" CACHE string "ICU version")

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

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
