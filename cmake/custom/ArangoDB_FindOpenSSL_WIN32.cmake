# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindOpenSSL
# -----------
#
# Find the OpenSSL encryption library.
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines the following :prop_tgt:`IMPORTED` targets:
#
# ``OpenSSL::SSL``
#   The OpenSSL ``ssl`` library, if found.
# ``OpenSSL::Crypto``
#   The OpenSSL ``crypto`` library, if found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``OPENSSL_FOUND``
#   System has the OpenSSL library.
# ``OPENSSL_INCLUDE_DIR``
#   The OpenSSL include directory.
# ``OPENSSL_CRYPTO_LIBRARY``
#   The OpenSSL crypto library.
# ``OPENSSL_SSL_LIBRARY``
#   The OpenSSL SSL library.
# ``OPENSSL_LIBRARIES``
#   All OpenSSL libraries.
# ``OPENSSL_VERSION``
#   This is set to ``$major.$minor.$revision$patch`` (e.g. ``0.9.8s``).
#

if (UNIX)
	message(FATAL_ERROR "FindOpenssl not implemented on UNIX")
endif ()

## get path parts determined by configuration
if(OPENSSL_USE_STATIC_LIBS)
	set(BUILD_TYPE "static")
else()
	set(BUILD_TYPE "shared")
endif(OPENSSL_USE_STATIC_LIBS)

# visual studio version
# see https://cmake.org/cmake/help/latest/variable/MSVC_VERSION.html
# related: https://cmake.org/cmake/help/latest/variable/MSVC_TOOLSET_VERSION.html
if (1909 LESS ${MSVC_VERSION} AND ${MSVC_VERSION} LESS 1920)
# 1910-1919: VS 15.0 (v141 toolset)
    set(VS_VERSION "2017")
elseif (1919 LESS ${MSVC_VERSION} AND ${MSVC_VERSION} LESS 1930)
# 1920-1929: VS 16.0 (v142 toolset)
    set(VS_VERSION "2019")
else()
    message(FATAL_ERROR "MSVC version not supported: ${MSVC_VERSION} (supported are 1910-1929)")
endif()

#store original suffixes
set(_openssl_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

## find includes
set(ssl_base_path "${OPENSSL_ROOT_DIR}")
set(BUILD_MODE "release")
set(path_part "VS_${VS_VERSION}/${BUILD_TYPE}-${BUILD_MODE}")
set(ssl_search_path "${ssl_base_path}/${path_part}/")
set(CMAKE_FIND_LIBRARY_SUFFIXES .lib ${CMAKE_FIND_LIBRARY_SUFFIXES} )

find_path(OPENSSL_INCLUDE_DIR
  NAMES "openssl/ssl.h"
  PATHS ${ssl_search_path}
  PATH_SUFFIXES "include"
  NO_DEFAULT_PATH
)


## find libs
set(_OPENSSL_PATH_SUFFIXES "bin" "lib")


# release
find_library(LIB_EAY_RELEASE
  NAMES
	libcrypto
	libeay32
	crypto
  NAMES_PER_DIR
	PATHS ${ssl_search_path}
  PATH_SUFFIXES
	${_OPENSSL_PATH_SUFFIXES}
  NO_DEFAULT_PATH
)

find_library(SSL_EAY_RELEASE
  NAMES
	libssl
	ssl
  NAMES_PER_DIR
	PATHS ${ssl_search_path}
  PATH_SUFFIXES
	${_OPENSSL_PATH_SUFFIXES}
  NO_DEFAULT_PATH
)

if(NOT OPENSSL_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES_STORED ${CMAKE_FIND_LIBRARY_SUFFIXES} )
  set(CMAKE_FIND_LIBRARY_SUFFIXES .dll ${CMAKE_FIND_LIBRARY_SUFFIXES} )

  #libcrypto-1_1-x64.dll
  find_library(LIB_EAY_RELEASE_DLL
    NAMES
      libcrypto-1_1-x64
  	  libcrypto
  	  crypto
    NAMES_PER_DIR
  	  PATHS ${ssl_search_path}
    PATH_SUFFIXES
  	  ${_OPENSSL_PATH_SUFFIXES}
    NO_DEFAULT_PATH
  )

  #libssl-1_1-x64.dll
  find_library(SSL_EAY_RELEASE_DLL
    NAMES
      libssl-1_1-x64
  	  libssl
  	  ssl
    NAMES_PER_DIR
  	  PATHS ${ssl_search_path}
    PATH_SUFFIXES
  	  ${_OPENSSL_PATH_SUFFIXES}
    NO_DEFAULT_PATH
  )

  # THIS WILL COLLIDE WITH DEBUG
  install (FILES "${LIB_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)
  install (FILES "${SSL_EAY_RELEASE_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES_STORED} )
endif()

# update search path
set(BUILD_MODE "debug")
set(path_part "${VS_VERSION}/${BUILD_TYPE}-${BUILD_MODE}")
set(ssl_search_path "${ssl_base_path}/${path_part}/")

find_library(LIB_EAY_DEBUG
  NAMES
	libcrypto
	libeay32
	crypto
  NAMES_PER_DIR
	PATHS ${ssl_search_path}
  PATH_SUFFIXES
	${_OPENSSL_PATH_SUFFIXES}
  NO_DEFAULT_PATH
)

find_library(SSL_EAY_DEBUG
  NAMES
	libssl
	ssl
  NAMES_PER_DIR
	PATHS ${ssl_search_path}
  PATH_SUFFIXES
	${_OPENSSL_PATH_SUFFIXES}
  NO_DEFAULT_PATH
)

if(NOT OPENSSL_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES_STORED ${CMAKE_FIND_LIBRARY_SUFFIXES} )
  set(CMAKE_FIND_LIBRARY_SUFFIXES .dll ${CMAKE_FIND_LIBRARY_SUFFIXES} )

  #libcrypto-1_1-x64.dll
  find_library(LIB_EAY_DEBUG_DLL
    NAMES
      libcrypto-1_1-x64
  	  libcrypto
  	  crypto
    NAMES_PER_DIR
  	  PATHS ${ssl_search_path}
    PATH_SUFFIXES
  	  ${_OPENSSL_PATH_SUFFIXES}
    NO_DEFAULT_PATH
  )

  #libssl-1_1-x64.dll
  find_library(SSL_EAY_DEBUG_DLL
    NAMES
      libssl-1_1-x64
  	  libssl
  	  ssl
    NAMES_PER_DIR
  	  PATHS ${ssl_search_path}
    PATH_SUFFIXES
   	  ${_OPENSSL_PATH_SUFFIXES}
    NO_DEFAULT_PATH
  )

  # THIS WILL COLLIDE WITH RELEASE
  install (FILES "${LIB_EAY_DEBUG_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)
  install (FILES "${SSL_EAY_DEBUG_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}/" COMPONENT Libraries)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES_STORED})
endif()


set(LIB_EAY_LIBRARY_DEBUG "${LIB_EAY_DEBUG}")
set(LIB_EAY_LIBRARY_RELEASE "${LIB_EAY_RELEASE}")
set(SSL_EAY_LIBRARY_DEBUG "${SSL_EAY_DEBUG}")
set(SSL_EAY_LIBRARY_RELEASE "${SSL_EAY_RELEASE}")

include(SelectLibraryConfigurations)
select_library_configurations(LIB_EAY)
select_library_configurations(SSL_EAY)

mark_as_advanced(LIB_EAY_LIBRARY_DEBUG LIB_EAY_LIBRARY_RELEASE
				 SSL_EAY_LIBRARY_DEBUG SSL_EAY_LIBRARY_RELEASE)

set(OPENSSL_SSL_LIBRARY ${SSL_EAY_LIBRARY} )
set(OPENSSL_CRYPTO_LIBRARY ${LIB_EAY_LIBRARY} )

function(from_hex HEX DEC)
  string(TOUPPER "${HEX}" HEX)
  set(_res 0)
  string(LENGTH "${HEX}" _strlen)

  while (_strlen GREATER 0)
    math(EXPR _res "${_res} * 16")
    string(SUBSTRING "${HEX}" 0 1 NIBBLE)
    string(SUBSTRING "${HEX}" 1 -1 HEX)
    if (NIBBLE STREQUAL "A")
      math(EXPR _res "${_res} + 10")
    elseif (NIBBLE STREQUAL "B")
      math(EXPR _res "${_res} + 11")
    elseif (NIBBLE STREQUAL "C")
      math(EXPR _res "${_res} + 12")
    elseif (NIBBLE STREQUAL "D")
      math(EXPR _res "${_res} + 13")
    elseif (NIBBLE STREQUAL "E")
      math(EXPR _res "${_res} + 14")
    elseif (NIBBLE STREQUAL "F")
      math(EXPR _res "${_res} + 15")
    else()
      math(EXPR _res "${_res} + ${NIBBLE}")
    endif()

    string(LENGTH "${HEX}" _strlen)
  endwhile()

  set(${DEC} ${_res} PARENT_SCOPE)
endfunction()

if(OPENSSL_INCLUDE_DIR AND EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h")
  file(STRINGS "${OPENSSL_INCLUDE_DIR}/openssl/opensslv.h" openssl_version_str
       REGEX "^#[\t ]*define[\t ]+OPENSSL_VERSION_NUMBER[\t ]+0x([0-9a-fA-F])+.*")

  if(openssl_version_str)
    # The version number is encoded as 0xMNNFFPPS: major minor fix patch status
    # The status gives if this is a developer or prerelease and is ignored here.
    # Major, minor, and fix directly translate into the version numbers shown in
    # the string. The patch field translates to the single character suffix that
    # indicates the bug fix state, which 00 -> nothing, 01 -> a, 02 -> b and so
    # on.

    string(REGEX REPLACE "^.*OPENSSL_VERSION_NUMBER[\t ]+0x([0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F][0-9a-fA-F])([0-9a-fA-F]).*$"
           "\\1;\\2;\\3;\\4;\\5" OPENSSL_VERSION_LIST "${openssl_version_str}")
    list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
    list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)
    from_hex("${OPENSSL_VERSION_MINOR}" OPENSSL_VERSION_MINOR)
    list(GET OPENSSL_VERSION_LIST 2 OPENSSL_VERSION_FIX)
    from_hex("${OPENSSL_VERSION_FIX}" OPENSSL_VERSION_FIX)
    list(GET OPENSSL_VERSION_LIST 3 OPENSSL_VERSION_PATCH)

    if (NOT OPENSSL_VERSION_PATCH STREQUAL "00")
      from_hex("${OPENSSL_VERSION_PATCH}" _tmp)
      # 96 is the ASCII code of 'a' minus 1
      math(EXPR OPENSSL_VERSION_PATCH_ASCII "${_tmp} + 96")
      unset(_tmp)
      # Once anyone knows how OpenSSL would call the patch versions beyond 'z'
      # this should be updated to handle that, too. This has not happened yet
      # so it is simply ignored here for now.
      string(ASCII "${OPENSSL_VERSION_PATCH_ASCII}" OPENSSL_VERSION_PATCH_STRING)
    endif ()

    set(OPENSSL_VERSION "${OPENSSL_VERSION_MAJOR}.${OPENSSL_VERSION_MINOR}.${OPENSSL_VERSION_FIX}${OPENSSL_VERSION_PATCH_STRING}")
  endif ()
endif ()

include(FindPackageHandleStandardArgs)

set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} )

if (OPENSSL_VERSION)
  find_package_handle_standard_args(OpenSSL
    REQUIRED_VARS
      #OPENSSL_SSL_LIBRARY # FIXME: require based on a component request?
      OPENSSL_CRYPTO_LIBRARY
      OPENSSL_INCLUDE_DIR
    VERSION_VAR
      OPENSSL_VERSION
    FAIL_MESSAGE
      "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR"
  )
else ()
  find_package_handle_standard_args(OpenSSL "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR"
    #OPENSSL_SSL_LIBRARY # FIXME: require based on a component request?
    OPENSSL_CRYPTO_LIBRARY
    OPENSSL_INCLUDE_DIR
  )
endif ()

mark_as_advanced(OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES)

if(OPENSSL_FOUND)
  if(NOT TARGET OpenSSL::Crypto AND
      (EXISTS "${OPENSSL_CRYPTO_LIBRARY}" OR
       EXISTS "${LIB_EAY_LIBRARY_DEBUG}" OR
       EXISTS "${LIB_EAY_LIBRARY_RELEASE}")
      )
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    if(EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}")
    endif()
    if(EXISTS "${LIB_EAY_LIBRARY_RELEASE}")
      set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
        IMPORTED_LOCATION_RELEASE "${LIB_EAY_LIBRARY_RELEASE}")
    endif()
    if(EXISTS "${LIB_EAY_LIBRARY_DEBUG}")
      set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
        IMPORTED_LOCATION_DEBUG "${LIB_EAY_LIBRARY_DEBUG}")
    endif()
  endif()
  if(NOT TARGET OpenSSL::SSL AND
      (EXISTS "${OPENSSL_SSL_LIBRARY}" OR
        EXISTS "${SSL_EAY_LIBRARY_DEBUG}" OR
        EXISTS "${SSL_EAY_LIBRARY_RELEASE}")
      )
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    if(EXISTS "${OPENSSL_SSL_LIBRARY}")
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}")
    endif()
    if(EXISTS "${SSL_EAY_LIBRARY_RELEASE}")
      set_property(TARGET OpenSSL::SSL APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
        IMPORTED_LOCATION_RELEASE "${SSL_EAY_LIBRARY_RELEASE}")
    endif()
    if(EXISTS "${SSL_EAY_LIBRARY_DEBUG}")
      set_property(TARGET OpenSSL::SSL APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
        IMPORTED_LOCATION_DEBUG "${SSL_EAY_LIBRARY_DEBUG}")
    endif()
    if(TARGET OpenSSL::Crypto)
      set_target_properties(OpenSSL::SSL PROPERTIES
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
    endif()
  endif()
endif()

# Restore the original find library ordering
if(OPENSSL_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_openssl_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()
