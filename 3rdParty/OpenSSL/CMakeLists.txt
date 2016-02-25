# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# OPENSSL
# ------------------------------------------------------------------------------

find_package(OpenSSL REQUIRED)

if (OPENSSL_VERSION)
  string(REPLACE "." ";" OPENSSL_VERSION_LIST ${OPENSSL_VERSION})
  list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
  list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)
  if (${OPENSSL_VERSION_MAJOR} GREATER 0 AND ${OPENSSL_VERSION_MINOR} GREATER 0)
    add_definitions(-DOPENSSL_NO_SSL2)
  endif ()
endif ()

set(OPENSSL_VERSION
  "${OPENSSL_VERSION}"
  CACHE INTERNAL
  "OpenSSL: Version"
  FORCE
)

set(OPENSSL_INCLUDE_DIR
  "${OPENSSL_INCLUDE_DIR}"
  CACHE INTERNAL
  "OpenSSL: Include Directory"
  FORCE
)
