# -*- mode: CMAKE; -*-

project(jemalloc C)
include(ExternalProject)

# set version and paths
set(JEMALLOC_VERSION "5.2.1")
set(JEMALLOC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/v${JEMALLOC_VERSION}")
set(JEMALLOC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/v${JEMALLOC_VERSION}")

# export to paths to parent scope
set(JEMALLOC_VERSION "${JEMALLOC_VERSION}" PARENT_SCOPE)

# copy files into binary directory
file(
  COPY
    "${JEMALLOC_SOURCE_DIR}"
  DESTINATION
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# generate external project
if (LINUX OR DARWIN)
  if (DARWIN)
    set(JEMALLOC_CC_TMP "cc")
    set(JEMALLOC_CXX_TMP "c++}")
    set(JEMALLOC_CONFIG "background_thread:false")
  else ()
    set(JEMALLOC_CC_TMP "${CMAKE_C_COMPILER}")
    set(JEMALLOC_CXX_TMP "${CMAKE_CXX_COMPILER}")
    set(JEMALLOC_CONFIG "background_thread:true")
  endif ()

  if (USE_JEMALLOC_PROF)
    SET(JEMALLOC_PROF "--enable-prof")
  endif ()

  set(JEMALLOC_LIB "${CMAKE_CURRENT_BINARY_DIR}/lib/libjemalloc.a")
  ExternalProject_Add(
    jemalloc_build
    SOURCE_DIR
      ${JEMALLOC_BINARY_DIR}
    PREFIX
      ${JEMALLOC_BINARY_DIR}
    PATCH_COMMAND
      cmake -E touch
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.html
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.3
    CONFIGURE_COMMAND
      "${JEMALLOC_SOURCE_DIR}/configure" CC=${JEMALLOC_CC_TMP}
                  CXX=${JEMALLOC_CXX_TMP}
                  CFLAGS=${CMAKE_C_FLAGS}
                  CXXFLAGS=${CMAKE_CXX_FLAGS}
                  --prefix=${CMAKE_CURRENT_BINARY_DIR}
                  --with-malloc-conf=${JEMALLOC_CONFIG}
                  --with-version=${JEMALLOC_VERSION}-0-g0
		  ${JEMALLOC_PROF}
    BUILD_COMMAND
      $(MAKE) build_lib_static
    BUILD_IN_SOURCE
      1
    INSTALL_COMMAND
      $(MAKE) install_include
           && ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/lib
           && ${CMAKE_COMMAND} -E copy ${JEMALLOC_BINARY_DIR}/lib/libjemalloc.a ${CMAKE_CURRENT_BINARY_DIR}/lib
    STEP_TARGETS
      ${JEMALLOC_LIB} jemalloc
  )

  set(JEMALLOC_HOME "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
  set(JEMALLOC_LIB "${JEMALLOC_LIB}" PARENT_SCOPE)
endif ()
