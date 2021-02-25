# -*- mode: CMAKE; -*-

project(libunwind C)
include(ExternalProject)

# set version and paths
set(LIBUNWIND_VERSION "1.5")
set(LIBUNWIND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/v${LIBUNWIND_VERSION}")
set(LIBUNWIND_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/v${LIBUNWIND_VERSION}")

# export to paths to parent scope
set(LIBUNWIND_VERSION "${LIBUNWIND_VERSION}" PARENT_SCOPE)

# copy files into binary directory
file(
  COPY
    "${LIBUNWIND_SOURCE_DIR}"
  DESTINATION
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# generate external project
if (LINUX)
  set(LIBUNWIND_CC_TMP "${CMAKE_C_COMPILER}")
  set(LIBUNWIND_CXX_TMP "${CMAKE_CXX_COMPILER}")

  set(LIBUNWIND_LIB "${CMAKE_CURRENT_BINARY_DIR}/v${LIBUNWIND_VERSION}/src/.libs/libunwind.a")
  # note: in the following configure, turning on cxx exceptions will cause duplicate
  # symbols (_Unwind_Resume and such) when linking, as some libc++ implementations
  # also use it internally
  ExternalProject_Add(
    libunwind_build
    SOURCE_DIR
      ${LIBUNWIND_BINARY_DIR}
    PREFIX
      ${LIBUNWIND_BINARY_DIR}
    CONFIGURE_COMMAND
      "${LIBUNWIND_SOURCE_DIR}/configure" CC=${LIBUNWIND_CC_TMP}
                  CXX=${LIBUNWIND_CXX_TMP}
                  CFLAGS=${CMAKE_C_FLAGS}
                  CXXFLAGS=${CMAKE_CXX_FLAGS}
                  --prefix=${CMAKE_CURRENT_BINARY_DIR}
                  --disable-shared --enable-static --disable-tests --disable-documentation --disable-per-thread-cache --disable-coredump --disable-ptrace --disable-setjmp --disable-debug --disable-zlibdebuginfo --disable-minidebuginfo --disable-cxx-exceptions
    BUILD_COMMAND
      $(MAKE) 
    BUILD_IN_SOURCE
      1
    INSTALL_COMMAND
      $(MAKE) install prefix=${CMAKE_CURRENT_BINARY_DIR} DESTDIR=${CMAKE_CURRENT_BINARY_DIR}
    STEP_TARGETS
      ${LIBUNWIND_LIB} libunwind
  )

  set(LIBUNWIND_HOME "${CMAKE_CURRENT_BINARY_DIR}/v${LIBUNWIND_VERSION}" PARENT_SCOPE)
  set(LIBUNWIND_LIB "${LIBUNWIND_LIB}" PARENT_SCOPE)
endif ()
