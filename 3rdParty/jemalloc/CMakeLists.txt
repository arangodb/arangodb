# -*- mode: CMAKE; -*-

project(jemalloc C)
include(ExternalProject)

# set version and paths
set(JEMALLOC_VERSION "5.0.1")
# set(JEMALLOC_VERSION "3.6.0")
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
  if (DARWIN)
    set(JEMALLOC_CC_TMP "cc")
    set(JEMALLOC_CXX_TMP "c++}")
    set(JEALLOC_CONFIG "background_thread:false")
  else ()
    set(JEMALLOC_CC_TMP "${CMAKE_C_COMPILER}")
    set(JEMALLOC_CXX_TMP "${CMAKE_CXX_COMPILER}")
    set(JEALLOC_CONFIG "background_thread:true")
  endif ()

  set(JEMALLOC_CFLAGS_TMP "-DARANGODB_MMAP_JEMALLOC=1")
  set(JEMALLOC_CXXFLAGS_TMP "-DARANGODB_MMAP_JEMALLOC=1")

  ExternalProject_Add(
    jemalloc
    SOURCE_DIR
      ${JEMALLOC_BINARY_DIR}
    PREFIX
      ${JEMALLOC_BINARY_DIR}
    PATCH_COMMAND
      cmake -E touch
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.html
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.3
    CONFIGURE_COMMAND
      ./configure CC=${JEMALLOC_CC_TMP}
                  CXX=${JEMALLOC_CXX_TMP}
                  CFLAGS=${JEMALLOC_CFLAGS_TMP}
                  CXXFLAGS=${JEMALLOC_CXXFLAGS_TMP}
                  --prefix=${CMAKE_CURRENT_BINARY_DIR}
                  --with-malloc-conf=${JEMALLOC_CONFIG}
                  --with-version=${JEMALLOC_VERSION}-0-g0
    BUILD_COMMAND
      make build_lib_static
    BUILD_IN_SOURCE
      1
    INSTALL_COMMAND
      make install_include
           && mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/lib
           && cp -a ${JEMALLOC_BINARY_DIR}/lib/libjemalloc.a ${CMAKE_CURRENT_BINARY_DIR}/lib
  )

  set(JEMALLOC_LIB "${JEMALLOC_BINARY_DIR}/lib/libjemalloc.a" PARENT_SCOPE)
endif ()
