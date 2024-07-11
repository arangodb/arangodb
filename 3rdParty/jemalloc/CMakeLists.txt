# -*- mode: CMAKE; -*-

project(jemalloc C)
include(ExternalProject)

set(JEMALLOC_VERSION "5.3.0")
set(JEMALLOC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jemalloc")
set(JEMALLOC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/jemalloc")

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

  if (USE_JEMALLOC_CHECKS)
    set(JEMALLOC_CHECKS "--enable-opt-safety-checks" "--enable-opt-size-checks")
  else ()
    set(JEMALLOC_CHECKS "")
  endif ()

  if (USE_JEMALLOC_PROF)
    if (USE_LIBUNWIND)
      # Note that CPPFLAGS are passed by jemalloc to both C and C++ compilers,
      # and to the preprocessor, and to dependency file generation. This is the
      # right place for includes.
      set(JEMALLOC_CPPFLAGS "-I${LIBUNWIND_HOME}/include")
      set(JEMALLOC_PROF "--enable-prof" "--enable-prof-libunwind" "--with-static-libunwind=${LIBUNWIND_LIB}")
    else ()
      set(JEMALLOC_PROF "--enable-prof")
    endif()
  endif ()

  # add build-type specific c flags and cxx flags here, because otherwise
  # they would be lost
  set(JEMALLOC_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  set(JEMALLOC_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  message(DEBUG "JEMALLOC_C_FLAGS=${JEMALLOC_C_FLAGS} JEMALLOC_CXX_FLAGS=${JEMALLOC_CXX_FLAGS}")

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
# One could run autogen.sh (which is needed if jemalloc becomes a submodule), otherwise
# it's fine to just commit the configure script to the arangodb repository.
#      ./autogen.sh
#      COMMAND
      ./configure
        CC=${JEMALLOC_CC}
        CXX=${JEMALLOC_CXX}
        CFLAGS=${JEMALLOC_C_FLAGS}
        CXXFLAGS=${JEMALLOC_CXX_FLAGS}
        CPPFLAGS=${JEMALLOC_CPPFLAGS}
        --prefix=${CMAKE_CURRENT_BINARY_DIR}
        --with-malloc-conf=${JEMALLOC_CONFIG}
        --with-version=${JEMALLOC_VERSION}-0-g0
        ${JEMALLOC_PROF}
        ${JEMALLOC_CHECKS}
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
