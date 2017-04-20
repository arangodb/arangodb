# -*- mode: CMAKE; -*-

project(jemalloc C)
include(ExternalProject)

# set version and paths
set(JEMALLOC_VERSION "4.5.0")
set(JEMALLOC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/v${JEMALLOC_VERSION}")
set(JEMALLOC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/v${JEMALLOC_VERSION}")

# export to paths to parent scope
set(JEMALLOC_VERSION "${JEMALLOC_VERSION}" PARENT_SCOPE)
set(JEMALLOC_HOME "${JEMALLOC_BINARY_DIR}" PARENT_SCOPE)

# copy files into binary directory
file(
  COPY
    "${JEMALLOC_SOURCE_DIR}"
  DESTINATION
    "${CMAKE_CURRENT_BINARY_DIR}"
)

if (LINUX OR DARWIN)
  file(
    COPY
      "${CMAKE_CURRENT_SOURCE_DIR}/arangodb-jemalloc.c"
    DESTINATION
      "${JEMALLOC_BINARY_DIR}/src"
  )
endif ()

# generate external project
if (LINUX OR DARWIN)
  ExternalProject_Add(
    jemalloc
    SOURCE_DIR
      ${JEMALLOC_BINARY_DIR}
    PATCH_COMMAND
      cmake -E touch
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.html
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.3
    CONFIGURE_COMMAND
      ./configure CFLAGS=-DARANGODB_MMAP_JEMALLOC=1 --prefix=${CMAKE_CURRENT_BINARY_DIR} --disable-munmap --with-version=${JEMALLOC_VERSION}-0-g0
    PREFIX
      ${JEMALLOC_BINARY_DIR}
    BUILD_COMMAND
      make
    BUILD_IN_SOURCE
      1
  )
endif ()
