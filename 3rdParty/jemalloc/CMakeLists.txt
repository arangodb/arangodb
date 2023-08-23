# -*- mode: CMAKE; -*-

project(jemalloc C)
include(ExternalProject)

# set version and paths
set(JEMALLOC_VERSION "5.3.0")
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
    set(JEMALLOC_CC "cc")
    set(JEMALLOC_CXX "c++}")
    set(JEMALLOC_CONFIG "background_thread:false")
  else ()
    set(JEMALLOC_CC "${CMAKE_C_COMPILER}")
    set(JEMALLOC_CXX "${CMAKE_CXX_COMPILER}")
    set(JEMALLOC_CONFIG "background_thread:true,cache_oblivious:false")
  endif ()

  if (USE_JEMALLOC_PROF)
      if (USE_LIBUNWIND)
        # Note that CPPFLAGS are passed by jemalloc to both C and C++ compilers,
        # and to the preprocessor, and to dependency file generation. This is the
        # right place for includes.
        SET(JEMALLOC_CPPFLAGS "-I${LIBUNWIND_HOME}/include")
        SET(JEMALLOC_PROF "--enable-prof" "--enable-prof-libunwind" "--with-static-libunwind=${LIBUNWIND_LIB}")
      else ()
          SET(JEMALLOC_PROF "--enable-prof")
      endif()
  endif ()

  set(JEMALLOC_LIB "${CMAKE_CURRENT_BINARY_DIR}/lib/libjemalloc.a")
  ExternalProject_Add(
    jemalloc_build
    SOURCE_DIR
      ${JEMALLOC_SOURCE_DIR}
    BINARY_DIR
      ${JEMALLOC_BINARY_DIR}
    PREFIX
      ${JEMALLOC_BINARY_DIR}
    PATCH_COMMAND
      cmake -E touch
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.html
        ${JEMALLOC_BINARY_DIR}/doc/jemalloc.3
    CONFIGURE_COMMAND
      ./configure
        CC=${JEMALLOC_CC}
        CXX=${JEMALLOC_CXX}
        CFLAGS=${CMAKE_C_FLAGS}
        CXXFLAGS=${CMAKE_CXX_FLAGS}
        CPPFLAGS=${JEMALLOC_CPPFLAGS}
        --prefix=${CMAKE_CURRENT_BINARY_DIR}
        --with-malloc-conf=${JEMALLOC_CONFIG}
        --with-version=${JEMALLOC_VERSION}-0-g0
        ${JEMALLOC_PROF}
    BUILD_COMMAND
      $(MAKE) build_lib_static
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
