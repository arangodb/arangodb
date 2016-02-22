# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# LIBEV
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release
      CACHE string
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif ()

project(libev)

if (MSVC)
  foreach (flag_var
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
    if (${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MDd?" "" ${flag_var} "${${flag_var}}")
    endif ()

    if (flag_var MATCHES "DEBUG")
      set(${flag_var} "${${flag_var}} /MTd")
    else ()
      set(${flag_var} "${${flag_var}} /MT")
    endif ()

    message(STATUS "${flag_var}=${${flag_var}}")
  endforeach ()

  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
  add_definitions("-DFD_SETSIZE=2048")
endif ()

################################################################################
## Detect which select (alternative) to use
################################################################################

include(CheckCXXSourceCompiles)

CHECK_INCLUDE_FILES(inttypeas.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
CHECK_INCLUDE_FILES(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILES(poll.h HAVE_POLL_H)
CHECK_INCLUDE_FILES(port.h HAVE_PORT_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILES(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILES(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILES(sys/epoll.h HAVE_SYS_EPOLL_H)
CHECK_INCLUDE_FILES(sys/eventfd.h HAVE_SYS_EVENTFD_H)
CHECK_INCLUDE_FILES(sys/event.h HAVE_SYS_EVENT_H)
CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
CHECK_INCLUDE_FILES(sys/select.h HAVE_SYS_SELECT_H)
CHECK_INCLUDE_FILES(sys/signalfd.h HAVE_SYS_SIGNALFD_H)
CHECK_INCLUDE_FILES(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILES(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILES("assert.h;ctype.h;errno.h;float.h;limits.h;locale.h;math.h;setjmp.h;signal.h;stdarg.h;stddef.h;stdio.h;stdlib.h;string.h;time.h;" STDC_HEADERS)
CHECK_INCLUDE_FILES("winsock.h" HAVE_WINSOCK_H)


CHECK_FUNCTION_EXISTS(inotify_init HAVE_INOTIFY_INIT)
CHECK_FUNCTION_EXISTS(epoll_ctl HAVE_EPOLL_CTL)
CHECK_FUNCTION_EXISTS(kqueue HAVE_KQUEUE)
CHECK_FUNCTION_EXISTS(port_create HAVE_PORT_CREATE)
CHECK_FUNCTION_EXISTS(poll HAVE_POLL)
CHECK_FUNCTION_EXISTS(select HAVE_SELECT)
CHECK_FUNCTION_EXISTS(eventfd HAVE_EVENTFD)
CHECK_FUNCTION_EXISTS(signalfd HAVE_SIGNALFD)
CHECK_FUNCTION_EXISTS(clock_gettime HAVE_CLOCK_GETTIME)
CHECK_FUNCTION_EXISTS(nanosleep HAVE_NANOSLEEP)
CHECK_FUNCTION_EXISTS(floor HAVE_FLOOR)

# check system library
check_library_exists(rt pthread_create "" HAVE_LIBRT)


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

set(LIBEV_VERSION
  "4.22"
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
  FORCE
)

set(LIBEV_DIR libev-${LIBEV_VERSION})

include_directories(${LIBEV_DIR})

#list(APPEND CMAKE_C_FLAGS -I${CMAKE_CURRENT_BINARY_DIR}/libev-${LIBEV_VERSION}) 
#list(APPEND CMAKE_CXX_FLAGS -I${CMAKE_CURRENT_BINARY_DIR}/libev-${LIBEV_VERSION}) 
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_CURRENT_BINARY_DIR}/libev-${LIBEV_VERSION}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_CURRENT_BINARY_DIR}/libev-${LIBEV_VERSION}")

configure_file(${LIBEV_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/libev-${LIBEV_VERSION}/config.h)

add_library(
  ev
  STATIC
  ${LIBEV_DIR}/ev.c
  ${LIBEV_DIR}/event.c
)

set(LIBEV_INCLUDE_DIR
  ${PROJECT_SOURCE_DIR}/${LIBEV_DIR}
  CACHE INTERNAL
  "${PROJECT_NAME}: Include Directories"
  FORCE
)

