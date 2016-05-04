# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# ROCKSDB
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

project(ROCKSDB)
set(ROCKSDB_VERSION 4.6.0)

# create target directory

set(ROCKSDB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rocksdb")
set(ROCKSDB_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rocksdb")

file(MAKE_DIRECTORY ${ROCKSDB_TARGET_DIR})

################################################################################
## ICU EXPORTS
################################################################################

set(ROCKSDB_VERSION
  ${ROCKSDB_VERSION}
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
)

set(ROCKSDB_INCLUDE_DIR
  "${ROCKSDB_DIR}/include"
  CACHE INTERNAL
  "${PROJECT_NAME}: include path"
)


if (${CMAKE_GENERATOR} MATCHES "Ninja")
# TODO

################################################################################
## WINDOWS
################################################################################

elseif (WIN32)
# TODO
else ()

################################################################################
## UNIX
################################################################################

  set(ROCKSDB_LIBRARY_DIR "${ROCKSDB_TARGET_DIR}/lib")

  ExternalProject_Add(rocksdb_build
    BUILD_IN_SOURCE
      TRUE
    SOURCE_DIR
      "${ROCKSDB_DIR}"
    CONFIGURE_COMMAND
      ""
    BUILD_COMMAND
      $(MAKE) static_lib
    STEP_TARGETS
      rocksdb
    INSTALL_COMMAND
      $(MAKE) INSTALL_PATH="${ROCKSDB_TARGET_DIR}" install
      "")

  list(APPEND LINK_DIRECTORIES "${ROCKSDB_LIBRARY_DIR}")

  set(ROCKSDB_LIBS
    "rocksdb"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
endif()

