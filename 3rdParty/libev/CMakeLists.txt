# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# LIBEV
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

project(libev)
set(libev_VERSION 4.22)

if (MSVC)
  set (FLAGS "${FLAGS} /FI\"${PROJECT_SOURCE_DIR}/ev_arangodb_win32.h\"")
endif ()

################################################################################
## Detect which select (alternative) to use
################################################################################

include(CheckIncludeFile)

if (MSVC)
  CHECK_INCLUDE_FILE("winsock.h" HAVE_SYS_SELECT_H)
else()
  CHECK_INCLUDE_FILE(poll.h HAVE_POLL_H)
  CHECK_INCLUDE_FILE(port.h HAVE_PORT_H)
  CHECK_INCLUDE_FILE(sys/epoll.h HAVE_SYS_EPOLL_H)
  CHECK_INCLUDE_FILE(sys/event.h HAVE_SYS_EVENT_H)
  CHECK_INCLUDE_FILE(sys/inotify.h HAVE_SYS_INOTIFY_H)
  CHECK_INCLUDE_FILE(sys/select.h HAVE_SYS_SELECT_H)
  CHECK_INCLUDE_FILE(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
endif()

include(CheckFunctionExists)

CHECK_FUNCTION_EXISTS(inotify_init HAVE_INOTIFY_INIT)
CHECK_FUNCTION_EXISTS(epoll_ctl HAVE_EPOLL_CTL)
CHECK_FUNCTION_EXISTS(kqueue HAVE_KQUEUE)
CHECK_FUNCTION_EXISTS(port_create HAVE_PORT_CREATE)
CHECK_FUNCTION_EXISTS(poll HAVE_POLL)
if (MSVC)
  set(HAVE_SELECT 1) # Force select for windows.
else()
  CHECK_FUNCTION_EXISTS(select HAVE_SELECT)
endif()
CHECK_FUNCTION_EXISTS(eventfd HAVE_EVENTFD)
CHECK_FUNCTION_EXISTS(signalfd HAVE_SIGNALFD)
CHECK_FUNCTION_EXISTS(clock_gettime HAVE_CLOCK_GETTIME)
CHECK_FUNCTION_EXISTS(nanosleep HAVE_NANOSLEEP)
CHECK_FUNCTION_EXISTS(floor HAVE_FLOOR)

# check system library
include(CheckCXXSourceCompiles)

if (HAVE_CLOCK_GETTIME)
  CHECK_CXX_SOURCE_COMPILES("
    #include <unistd.h>
    #include <sys/syscall.h>
    #include <time.h>
    int main(void)
    {
      struct timespec ts;
      int status = syscall (SYS_clock_gettime, CLOCK_REALTIME, &ts);
    }"
  HAVE_CLOCK_SYSCALL)
endif ()

################################################################################
## libev library
################################################################################

set(LIBEV_DIR libev-${libev_VERSION})

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
  foreach (flag in 
             CMAKE_C_FLAGS
             CMAKE_C_FLAGS_DEBUG
             CMAKE_C_FLAGS_RELEASE
             CMAKE_C_FLAGS_MINSIZEREL
             CMAKE_C_FLAGS_RELWITHDEBINFO
             CMAKE_CXX_FLAGS
             CMAKE_CXX_FLAGS_DEBUG
             CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL
             CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    set(${flag} "${${flag}} -Wno-all")
  endforeach()
endif ()

include_directories(${LIBEV_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/libev-${libev_VERSION})

add_library(ev STATIC
  ${LIBEV_DIR}/ev.c
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/libev-${libev_VERSION}/config.h
)

set_target_properties(ev PROPERTIES COMPILE_FLAGS "${FLAGS}")

set(LIBEV_VERSION
  ${libev_VERSION}
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
)

set(LIBEV_LIBS
  ev
  CACHE INTERNAL
  "${PROJECT_NAME}: Libraries"
)

set(LIBEV_INCLUDE_DIR
  ${PROJECT_SOURCE_DIR}/${LIBEV_DIR}
  CACHE INTERNAL
  "${PROJECT_NAME}: Include Directories"
)

list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")
set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
