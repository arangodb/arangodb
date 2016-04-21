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
elseif (CMAKE_COMPILER_IS_GNUCC)
  set (FLAGS "${FLAGS} -Wold-style-declaration")
endif ()

################################################################################
## Detect which select (alternative) to use
################################################################################

include(CheckIncludeFile)

if (MSVC)
  check_include_file("winsock.h" HAVE_SYS_SELECT_H)
else()
  check_include_file(poll.h HAVE_POLL_H)
  check_include_file(port.h HAVE_PORT_H)
  check_include_file(sys/epoll.h HAVE_SYS_EPOLL_H)
  check_include_file(sys/event.h HAVE_SYS_EVENT_H)
  check_include_file(sys/inotify.h HAVE_SYS_INOTIFY_H)
  check_include_file(sys/select.h HAVE_SYS_SELECT_H)
  check_include_file(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
endif()

include(CheckFunctionExists)

check_function_exists(inotify_init HAVE_INOTIFY_INIT)
check_function_exists(epoll_ctl HAVE_EPOLL_CTL)
check_function_exists(kqueue HAVE_KQUEUE)
check_function_exists(port_create HAVE_PORT_CREATE)
check_function_exists(poll HAVE_POLL)
if (MSVC)
  set(HAVE_SELECT 1) # Force select for windows.
else()
  check_function_exists(select HAVE_SELECT)
endif()
check_function_exists(eventfd HAVE_EVENTFD)
check_function_exists(signalfd HAVE_SIGNALFD)
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists(nanosleep HAVE_NANOSLEEP)
check_function_exists(floor HAVE_FLOOR)

# check system library
include(CheckCXXSourceCompiles)

if (HAVE_CLOCK_GETTIME)
  check_cxx_source_compiles("
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
