################################################################################
## OpenSSL
################################################################################

if (NOT EXISTS "${PROJECT_SOURCE_DIR}/VERSIONS")
  message(FATAL_ERROR "expecting ${PROJECT_SOURCE_DIR}/VERSIONS")
else ()
  file(READ "${PROJECT_SOURCE_DIR}/VERSIONS" ARANGODB_VERSIONS_CONTENT)
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

if (OPENSSL_FOUND AND USE_STRICT_OPENSSL_VERSION)
  if (NOT "${OPENSSL_VERSION}" MATCHES "${ARANGODB_REQUIRED_OPENSSL_VERSION}")
    message (FATAL_ERROR "Wrong OpenSSL version was found: ${OPENSSL_VERSION}! Required version: ${MSG_ARANGODB_REQUIRED_OPENSSL_VERSION}!")
  endif ()
endif ()

message(${OPENSSL_INCLUDE_DIR})

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
