project(libev)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

#FIXME TEST AND CONFIGURE

add_library(ar_libev STATIC ev.c event.c)
target_compile_definitions(ar_libev PUBLIC "EV_CHILD_ENABLE=0")
target_compile_options(ar_libev PRIVATE "-O3") ## FIXME Windows
target_include_directories(ar_libev PUBLIC .)


include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)

### /* Define to 1 if you have the `clock_gettime' function. */
### #undef HAVE_CLOCK_GETTIME
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)





### /* Define to 1 to use the syscall interface for clock_gettime */
### #undef HAVE_CLOCK_SYSCALL
# FIXME FIXME FIXME FIXME FIXME FIXME take code from libev.m4





### /* Define to 1 if you have the <dlfcn.h> header file. */
### #undef HAVE_DLFCN_H
check_include_files(dlfcn.h HAVE_DLFCN_H)

### /* Define to 1 if you have the `epoll_ctl' function. */
### #undef HAVE_EPOLL_CTL
check_function_exists(epoll_ctl HAVE_EPOLL_CTL)

### /* Define to 1 if you have the `eventfd' function. */
### #undef HAVE_EVENTFD
check_function_exists(eventfd HAVE_EVENTFD)

### /* Define to 1 if the floor function is available */
### #undef HAVE_FLOOR
check_function_exists(floor HAVE_FLOOR)

### /* Define to 1 if you have the `inotify_init' function. */
### #undef HAVE_INOTIFY_INIT
check_function_exists(inotify_init HAVE_INOTIFY_INIT)

### /* Define to 1 if you have the <inttypes.h> header file. */
### #undef HAVE_INTTYPES_H
check_include_files(inttypes.h HAVE_INTTYPES_H)

### /* Define to 1 if you have the `kqueue' function. */
### #undef HAVE_KQUEUE
check_function_exists(kqueue HAVE_KQUEUE)

### /* Define to 1 if you have the `rt' library (-lrt). */
### #undef HAVE_LIBRT
check_library_exists(rt clock_gettime "" HAVE_LIBRT)

### /* Define to 1 if you have the <memory.h> header file. */
### #undef HAVE_MEMORY_H
check_include_files(memory.h HAVE_MEMORY_H)

### /* Define to 1 if you have the `nanosleep' function. */
### #undef HAVE_NANOSLEEP
check_function_exists(nanosleep HAVE_NANOSLEEP)

### /* Define to 1 if you have the `poll' function. */
### #undef HAVE_POLL
check_function_exists(poll HAVE_POLL)

### /* Define to 1 if you have the <poll.h> header file. */
### #undef HAVE_POLL_H
check_include_files(poll.h HAVE_POLL_H)

### /* Define to 1 if you have the `port_create' function. */
### #undef HAVE_PORT_CREATE
check_function_exists(port_create HAVE_PORT_CREATE)

### /* Define to 1 if you have the <port.h> header file. */
### #undef HAVE_PORT_H
check_include_files(port.h HAVE_PORT_H)

### /* Define to 1 if you have the `select' function. */
### #undef HAVE_SELECT
check_function_exists(select HAVE_SELECT)

### /* Define to 1 if you have the `signalfd' function. */
### #undef HAVE_SIGNALFD
check_function_exists(signalfd HAVE_SIGNALFD)

### /* Define to 1 if you have the <stdint.h> header file. */
### #undef HAVE_STDINT_H
check_include_files(stdint.h HAVE_STDINT_H)

### /* Define to 1 if you have the <stdlib.h> header file. */
### #undef HAVE_STDLIB_H
check_include_files(stdlib.h HAVE_STDLIB_H)

### /* Define to 1 if you have the <strings.h> header file. */
### #undef HAVE_STRINGS_H
check_include_files (strings.h HAVE_STRINGS_H)

### /* Define to 1 if you have the <string.h> header file. */
### #undef HAVE_STRING_H
check_include_files (string.h HAVE_STRING_H)

### /* Define to 1 if you have the <sys/epoll.h> header file. */
### #undef HAVE_SYS_EPOLL_H
check_include_files(sys/epoll.h HAVE_SYS_EPOLL_H)
 
### /* Define to 1 if you have the <sys/eventfd.h> header file. */
### #undef HAVE_SYS_EVENTFD_H
check_include_files(sys/eventfd.h HAVE_SYS_EVENTFD_H)
 
### /* Define to 1 if you have the <sys/event.h> header file. */
### #undef HAVE_SYS_EVENT_H
check_include_files(sys/event.h HAVE_SYS_EVENT_H)

### /* Define to 1 if you have the <sys/inotify.h> header file. */
### #undef HAVE_SYS_INOTIFY_H
check_include_files(sys/inotify.h HAVE_SYS_INOTIFY_H)

### /* Define to 1 if you have the <sys/select.h> header file. */
### #undef HAVE_SYS_SELECT_H
check_include_files(sys/select.h HAVE_SYS_SELECT_H)

### /* Define to 1 if you have the <sys/signalfd.h> header file. */
### #undef HAVE_SYS_SIGNALFD_H
check_include_files(sys/signalfd.h HAVE_SYS_SIGNALFD_H)

### /* Define to 1 if you have the <sys/stat.h> header file. */
### #undef HAVE_SYS_STAT_H
check_include_files(sys/stat.h HAVE_SYS_STAT_H)

### /* Define to 1 if you have the <sys/types.h> header file. */
### #undef HAVE_SYS_TYPES_H
check_include_files(sys/types.h HAVE_SYS_TYPES_H)

### /* Define to 1 if you have the <unistd.h> header file. */
### #undef HAVE_UNISTD_H
check_include_files(unistd.h HAVE_UNISTD_H)

### /* Define to the sub-directory in which libtool stores uninstalled libraries.
###    */
### #undef LT_OBJDIR
### 
### /* Name of package */
### #undef PACKAGE
### 
### /* Define to the address where bug reports for this package should be sent. */
### #undef PACKAGE_BUGREPORT
### 
### /* Define to the full name of this package. */
### #undef PACKAGE_NAME
### 
### /* Define to the full name and version of this package. */
### #undef PACKAGE_STRING
### 
### /* Define to the one symbol short name of this package. */
### #undef PACKAGE_TARNAME
### 
### /* Define to the home page for this package. */
### #undef PACKAGE_URL
### 
### /* Define to the version of this package. */
### #undef PACKAGE_VERSION
### 
### /* Define to 1 if you have the ANSI C header files. */
### #undef STDC_HEADERS
###/* Version number of package */
####undef VERSION

configure_file(config.h.cmake.in config.h)
