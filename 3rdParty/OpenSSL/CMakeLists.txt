# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# OPENSSL
# ------------------------------------------------------------------------------

find_package(OpenSSL REQUIRED)

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

set(OPENSSL_VERSION
  "${OPENSSL_VERSION}"
  CACHE INTERNAL
  "OpenSSL: Version"
)

set(OPENSSL_INCLUDE_DIR
  "${OPENSSL_INCLUDE_DIR}"
  CACHE INTERNAL
  "OpenSSL: Include Directory"
)

if (WIN32 AND NOT SSL_NUGET)
  if (DEBUG)
    set(OPENSSL_EXT "d")
  else ()
    set(OPENSSL_EXT "")
  endif ()

  if (NOT ${OPENSSL_LIB_DIR})
    set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib/VC")
  endif ()

message(STATUS "OPENSSL_LIB_DIR = ${OPENSSL_LIB_DIR}")
message(STATUS "OPENSSL_ROOT_DIR = ${OPENSSL_ROOT_DIR}")

  set(OPENSSL_LIBRARIES
    debug "${OPENSSL_LIB_DIR}/ssleay32MTd.lib;${OPENSSL_LIB_DIR}/libeay32MTd.lib"
    optimized "${OPENSSL_LIB_DIR}/ssleay32MT.lib;${OPENSSL_LIB_DIR}/libeay32MT.lib"
    CACHE INTERNAL
    "OpenSSL: libraries"
  )
else ()
  set(OPENSSL_LIBRARIES
    "${OPENSSL_LIBRARIES}"
    CACHE INTERNAL
    "OpenSSL: libraries"
  )
endif ()
