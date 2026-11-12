# -*- mode: CMAKE; -*-

project(libunwind C)
include(ExternalProject)
include(FetchContent)

set(LIBUNWIND_VERSION "1.8.3")
set(LIBUNWIND_SHA256 "be30d910e67f58d82e753231f1357f326a1a088acf126b21ff77e60aab19b90b")

# export to paths to parent scope
set(LIBUNWIND_VERSION "${LIBUNWIND_VERSION}" PARENT_SCOPE)

if (LINUX)
  # the release tarball ships a pre-generated configure script, which the
  # git archive does not
  set(LIBUNWIND_FETCH_OPTIONS
    URL https://github.com/libunwind/libunwind/releases/download/v${LIBUNWIND_VERSION}/libunwind-${LIBUNWIND_VERSION}.tar.gz
    URL_HASH SHA256=${LIBUNWIND_SHA256}
  )
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    list(APPEND LIBUNWIND_FETCH_OPTIONS DOWNLOAD_EXTRACT_TIMESTAMP OFF)
  endif()

  # the tarball has no CMakeLists.txt, so this only downloads and extracts
  FetchContent_Declare(libunwind ${LIBUNWIND_FETCH_OPTIONS})
  FetchContent_MakeAvailable(libunwind)

  set(LIBUNWIND_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/install")
  set(LIBUNWIND_LIB "${LIBUNWIND_INSTALL_DIR}/lib/libunwind.a")

  # add build-type specific c flags and cxx flags here, because otherwise
  # they would be lost
  set(LIBUNWIND_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  set(LIBUNWIND_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  message(DEBUG "LIBUNWIND_C_FLAGS=${LIBUNWIND_C_FLAGS} LIBUNWIND_CXX_FLAGS=${LIBUNWIND_CXX_FLAGS}")

  # note: in the following configure, turning on cxx exceptions will cause duplicate
  # symbols (_Unwind_Resume and such) when linking, as some libc++ implementations
  # also use it internally
  ExternalProject_Add(
    libunwind_build
    SOURCE_DIR
      ${libunwind_SOURCE_DIR}
    BINARY_DIR
      ${libunwind_BINARY_DIR}
    INSTALL_DIR
      ${LIBUNWIND_INSTALL_DIR}
    CONFIGURE_COMMAND
      "${libunwind_SOURCE_DIR}/configure" CC=${CMAKE_C_COMPILER}
                  CXX=${CMAKE_CXX_COMPILER}
                  CFLAGS=${LIBUNWIND_C_FLAGS}
                  CXXFLAGS=${LIBUNWIND_CXX_FLAGS}
                  --prefix=${LIBUNWIND_INSTALL_DIR}
                  --libdir=${LIBUNWIND_INSTALL_DIR}/lib
                  --disable-shared --enable-static --disable-tests --disable-documentation --disable-per-thread-cache --disable-coredump --disable-ptrace --disable-setjmp --disable-debug --enable-debug-frame --disable-zlibdebuginfo --disable-minidebuginfo --disable-cxx-exceptions --disable-weak-backtrace --disable-unwind-header
    BUILD_COMMAND
      ${MAKE} ${MAKE_JOBS}
    BUILD_BYPRODUCTS
      ${LIBUNWIND_LIB}
    INSTALL_COMMAND
      ${MAKE} install
  )

  set(LIBUNWIND_HOME "${LIBUNWIND_INSTALL_DIR}" PARENT_SCOPE)
  set(LIBUNWIND_LIB "${LIBUNWIND_LIB}" PARENT_SCOPE)
endif ()
