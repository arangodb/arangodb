////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_OPERATING_SYSTEM_H
#define TRIAGENS_BASICS_C_OPERATING_SYSTEM_H 1

#ifndef TRI_WITHIN_COMMON
#error use <BasicsC/common.h>
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                            global
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#define GLOBAL_TIMEZONE                     timezone

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#undef TRI_PADDING_32
#else
#define TRI_PADDING_32                      1
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --Section--                                                             apple
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef __APPLE__

#define TRI_DIR_SEPARATOR_CHAR              '/'
#define TRI_DIR_SEPARATOR_STR               "/"

#define TRI_HAVE_POSIX                      1
#define TRI_HAVE_MACH                       1

#define TRI_HAVE_DLFCN_H                    1

#include <stdint.h>

#define TRI_ENABLE_SYSLOG                   1

#define TRI_HAVE_DIRENT_H                   1
#define TRI_HAVE_GETRLIMIT                  1
#define TRI_HAVE_FORK                       1
#define TRI_HAVE_SIGNAL_H                   1
#define TRI_HAVE_STDBOOL_H                  1
#define TRI_HAVE_SYS_RESOURCE_H             1
#define TRI_HAVE_SYS_TIME_H                 1
#define TRI_HAVE_SYS_TYPES_H                1
#define TRI_HAVE_SYS_WAIT_H                 1
#define TRI_HAVE_UNISTD_H                   1
#define TRI_HAVE_TERMIOS_H                  1
#define TRI_HAVE_SYS_IOCTL_H                1
#define TRI_HAVE_SCHED_H                    1

#define TRI_HAVE_LINUX_SOCKETS              1
#define TRI_HAVE_MACOS_SPIN                 1
#define TRI_HAVE_POSIX_THREADS              1
#define TRI_HAVE_POSIX_MMAP                 1
#define TRI_HAVE_POSIX_PWD_GRP              1

#define TRI_HAVE_GETPPID                    1
#define TRI_HAVE_GETRUSAGE                  1
#define TRI_HAVE_GETTIMEOFDAY               1
#define TRI_HAVE_GMTIME_R                   1

#define TRI_HAVE_SETGID                     1
#define TRI_HAVE_SETUID                     1
#define TRI_HAVE_GETGRGID                   1
#define TRI_HAVE_GETPWNAM                   1
#define TRI_HAVE_GETPWUID                   1
#define TRI_HAVE_GETGRNAM                   1


#define TRI_HAVE_STRTOLL                    1
#define TRI_HAVE_STRTOULL                   1

// TODO: add a feature check in configure
#define TRI_HAVE_ANONYMOUS_MMAP             1

#define TRI_OVERLOAD_FUNCS_SIZE_T           1
#define TRI_MISSING_MEMRCHR                 1

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
#define TRI_HAVE_GETLINE                    1
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
#define TRI_HAVE_GETLINE                    1
#endif

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T                   8
#define TRI_ALIGNOF_VOIDP                   8
#else
#define TRI_SIZEOF_SIZE_T                   4
#define TRI_ALIGNOF_VOIDP                   4
#endif

#ifndef SIZE_MAX
#if __WORDSIZE == 64
#define SIZE_MAX (18446744073709551615UL)
#else
#define SIZE_MAX (4294967295U)
#endif
#endif

#define TRI_CHDIR                       chdir
#define TRI_CLOSE                       close
#define TRI_CLOSE_SOCKET                TRI_closesocket
#define TRI_CREATE(a,b,c)               open((a), (b), (c))
#define TRI_GETCWD                      getcwd
#define TRI_LSEEK                       lseek
#define TRI_MKDIR(a,b)                  mkdir((a), (b))
#define TRI_OPEN(a,b)                   open((a), (b))
#define TRI_READ                        read
#define TRI_READ_SOCKET(a,b,c,d)        TRI_readsocket((a), (b), (c), (d))
#define TRI_RMDIR                       rmdir
#define TRI_UNLINK                      unlink
#define TRI_WRITE                       write
#define TRI_WRITE_SOCKET(a,b,c,d)       TRI_writesocket((a), (b), (c), (d))

#define TRI_LAST_ERROR_STR              strerror(errno)

#define TRI_uid_t                       uid_t
#define TRI_gid_t                       gid_t

#define INVALID_SOCKET                  -1
#define SOCKET_ERROR                    -1

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            cygwin
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef __CYGWIN__

#define TRI_DIR_SEPARATOR_CHAR              '/'
#define TRI_DIR_SEPARATOR_STR               "/"

#define TRI_HAVE_POSIX                      1

#define TRI_HAVE_DLFCN_H                    1

#include <bits/wordsize.h>
#include <io.h>

#define TRI_ENABLE_SYSLOG                   1

#define TRI_HAVE_DIRENT_H                   1
#define TRI_HAVE_GETRLIMIT                  1
#define TRI_HAVE_FORK                       1
#define TRI_HAVE_STDBOOL_H                  1
#define TRI_HAVE_SYS_RESOURCE_H             1
#define TRI_HAVE_UNISTD_H                   1
#define TRI_HAVE_SYS_TYPES_H                1
#define TRI_HAVE_SYS_WAIT_H                 1

#define TRI_HAVE_LINUX_SOCKETS              1
#define TRI_HAVE_POSIX_SPIN                 1
#define TRI_HAVE_POSIX_THREADS              1
#define TRI_HAVE_POSIX_MMAP                 1
#define TRI_HAVE_POSIX_PWD_GRP              1
#define TRI_HAVE_GETLINE                    1
#define TRI_HAVE_GETTIMEOFDAY               1
#define TRI_HAVE_GMTIME_R                   1

#define TRI_HAVE_SETGID                     1
#define TRI_HAVE_SETUID                     1

#define TRI_HAVE_STRTOLL                    1
#define TRI_HAVE_STRTOULL                   1

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T                   8
#define TRI_ALIGNOF_VOIDP                   8
#else
#define TRI_SIZEOF_SIZE_T                   4
#define TRI_ALIGNOF_VOIDP                   4
#endif

#define TRI_CHDIR                       chdir
#define TRI_CLOSE                       close
#define TRI_CLOSE_SOCKET                TRI_closesocket
#define TRI_CREATE(a,b,c)               open((a), (b), (c))
#define TRI_LSEEK                       lseek
#define TRI_GETCWD                      getcwd
#define TRI_MKDIR(a,b)                  mkdir((a), (b))
#define TRI_OPEN(a,b)                   open((a), (b))
#define TRI_READ                        read
#define TRI_READ_SOCKET(a,b,c,d)        TRI_readsocket((a), (b), (c), (d))
#define TRI_RMDIR                       rmdir
#define TRI_UNLINK                      unlink
#define TRI_WRITE                       write
#define TRI_WRITE_SOCKET(a,b,c,d)       TRI_writesocket((a), (b), (c), (d))

#define TRI_LAST_ERROR_STR              strerror(errno)

#define TRI_uid_t                       uid_t
#define TRI_gid_t                       gid_t

#define INVALID_SOCKET                  -1
#define SOCKET_ERROR                    -1

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             linux
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef __linux__

#define TRI_DIR_SEPARATOR_CHAR              '/'
#define TRI_DIR_SEPARATOR_STR               "/"

#define TRI_HAVE_POSIX                      1

#define TRI_HAVE_DLFCN_H                    1

// force posix source
#if ! defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

// first include the features file and then define
#include <features.h>

// for INTxx_MIN and INTxx_MAX
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

// for usleep
#ifndef __USE_BSD
#define __USE_BSD
#endif

// for pthread_sigmask
#ifndef __cplusplus
#define __USE_UNIX98
#endif

#define TRI_ENABLE_SYSLOG                   1

#define TRI_GCC_THREAD_LOCAL_STORAGE        1

#define TRI_HAVE_DIRENT_H                   1
#define TRI_HAVE_GETRLIMIT                  1
#define TRI_HAVE_FORK                       1
#define TRI_HAVE_SIGNAL_H                   1
#define TRI_HAVE_STDBOOL_H                  1
#define TRI_HAVE_SYS_FILE_H                 1
#define TRI_HAVE_SYS_PRCTL_H                1
#define TRI_HAVE_SYS_RESOURCE_H             1
#define TRI_HAVE_SYS_TIME_H                 1
#define TRI_HAVE_SYS_TYPES_H                1
#define TRI_HAVE_SYS_WAIT_H                 1
#define TRI_HAVE_UNISTD_H                   1
#define TRI_HAVE_TERMIOS_H                  1
#define TRI_HAVE_SYS_IOCTL_H                1
#define TRI_HAVE_SCHED_H                    1

#define TRI_HAVE_LINUX_PROC                 1
#define TRI_HAVE_LINUX_SOCKETS              1
#define TRI_HAVE_POSIX_SPIN                 1
#define TRI_HAVE_POSIX_THREADS              1
#define TRI_HAVE_POSIX_MMAP                 1
#define TRI_HAVE_POSIX_PWD_GRP              1

#define TRI_HAVE_GETLINE                    1
#define TRI_HAVE_GETPPID                    1
#define TRI_HAVE_GETRUSAGE                  1
#define TRI_HAVE_GETTIMEOFDAY               1
#define TRI_HAVE_GMTIME_R                   1
#define TRI_HAVE_PRCTL                      1

#define TRI_HAVE_SETGID                     1
#define TRI_HAVE_SETUID                     1
#define TRI_HAVE_GETGRGID                   1
#define TRI_HAVE_GETPWNAM                   1
#define TRI_HAVE_GETPWUID                   1
#define TRI_HAVE_GETGRNAM                   1


#define TRI_HAVE_STRTOLL                    1
#define TRI_HAVE_STRTOULL                   1

// TODO: add a feature check in configure
#define TRI_HAVE_ANONYMOUS_MMAP             1

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T                   8
#define TRI_ALIGNOF_VOIDP                   8
#else
#define TRI_SIZEOF_SIZE_T                   4
#define TRI_ALIGNOF_VOIDP                   4
#endif

#define TRI_CHDIR                       chdir
#define TRI_CLOSE                       close
#define TRI_CLOSE_SOCKET                TRI_closesocket
#define TRI_CREATE(a,b,c)               open((a), (b), (c))
#define TRI_LSEEK                       lseek
#define TRI_GETCWD                      getcwd
#define TRI_MKDIR(a,b)                  mkdir((a), (b))
#define TRI_OPEN(a,b)                   open((a), (b))
#define TRI_READ                        read
#define TRI_READ_SOCKET(a,b,c,d)        TRI_readsocket((a), (b), (c), (d))
#define TRI_RMDIR                       rmdir
#define TRI_UNLINK                      unlink
#define TRI_WRITE                       write
#define TRI_WRITE_SOCKET(a,b,c,d)       TRI_writesocket((a), (b), (c), (d))

#define TRI_LAST_ERROR_STR              strerror(errno)

#define TRI_uid_t                       uid_t
#define TRI_gid_t                       gid_t

#define INVALID_SOCKET                  -1
#define SOCKET_ERROR                    -1

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           windows
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32) && defined(_MSC_VER)

#define TRI_DIR_SEPARATOR_CHAR              '\\'
#define TRI_DIR_SEPARATOR_STR               "\\"

// ..............................................................................
// This directive below suppresses warnings about using the 'new' more secure CRT 
// functions.
// ..............................................................................

#define _CRT_SECURE_NO_WARNINGS                     1

// ..............................................................................
// This directive below provides a manner in which the 'new' more secure functions
// for example, strcpy is automatically converted to strcpy_s. This is enabled
// by default. We have disabled it here.
// ..............................................................................

//#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES     1
#include <stdio.h>
#include <io.h>
#include <WinSock2.h>

#define TRI_WIN32_CONSOLE                   1
#define TRI_WIN32_THREAD_LOCAL_STORAGE      1

#define TRI_HAVE_DIRECT_H                   1
#define TRI_HAVE_PROCESS_H                  1
#define TRI_HAVE_SIGNAL_H                   1
#define TRI_HAVE_WINSOCK2_H                 1

#define TRI_HAVE_GETTID                     1
#define TRI_HAVE_GMTIME_S                   1
#define TRI_HAVE_STRTOI64                   1
#define TRI_HAVE_STRTOUI64                  1
#define TRI_HAVE_WIN32_CLOSE_ON_EXEC        1
#define TRI_HAVE_WIN32_GETTIMEOFDAY         1
#define TRI_HAVE_WIN32_FILE_LOCKING         1
#define TRI_HAVE_WIN32_LIST_FILES           1
#define TRI_HAVE_WIN32_NON_BLOCKING         1
#define TRI_HAVE_WIN32_SOCKETS              1
#define TRI_HAVE_WIN32_SYMBOLIC_LINK        1
#define TRI_HAVE_WIN32_THREADS              1
#define TRI_HAVE_WIN32_MMAP                 1
#define TRI_HAVE_WIN32_PWD                  1

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T                   8
#define TRI_ALIGNOF_VOIDP                   8
#else
#define TRI_SIZEOF_SIZE_T                   4
#define TRI_ALIGNOF_VOIDP                   4
#endif

#define TRI_HAVE_ANONYMOUS_MMAP             1

#define strcasecmp                      _stricmp
#define strncasecmp                     _strnicmp
#define snprintf                        _snprintf

// ..............................................................
// usleep in POSIX is for microseconds - not milliseconds
// has been redefined in win-utils.h
// ..............................................................

#define usleep                          TRI_usleep
#define sleep                           TRI_sleep
#define srandom                         srand
#define fsync                           _commit
#define isatty                          _isatty
#define fileno                          _fileno
#define putenv                          _putenv
#define tzset                           _tzset

typedef int ssize_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif


#ifndef __BOOL_DEFINED
//typedef unsigned int bool; - this was never ever going to work. Problem is sizeof(bool) in VS C++ is 1 byte and
// sizeof(bool) in VS C (C compiler) is -- whatever you want. However, when structures are interchanged between
// C & C++ (as in arango) all hell will break loose.
typedef unsigned char bool;
#define true 1
#define false 0
#endif

#define va_copy(d,s) ((d) = (s))

// we do not have owner read and owner write under windows
// so map these to global read, global write
// these are used when creating a file
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IRGRP _S_IREAD
#define S_IWGRP _S_IWRITE

#define O_RDONLY                        _O_RDONLY
#define TRI_CHDIR                       _chdir
#define TRI_CLOSE                       _close
#define TRI_CLOSE_SOCKET                TRI_closesocket
/*  #define TRI_CREATE(a,b,c)               _open((a), (b), (c)) */
#define TRI_CREATE(a,b,c)               TRI_createFile((a), (b), (c))
#define TRI_GETCWD                      _getcwd
#define TRI_LSEEK                       _lseek
#define TRI_MKDIR(a,b)                  _mkdir((a))
/* #define TRI_OPEN(a,b)                   _open((a), (b)) */
#define TRI_OPEN(a,b)                   TRI_openFile((a), (b))
#define TRI_READ                        _read
#define TRI_READ_SOCKET(a,b,c,d)        TRI_readsocket((a), (b), (c), (d))
#define TRI_RMDIR                       _rmdir
#define TRI_UNLINK                      _unlink
#define TRI_WRITE                       _write
#define TRI_WRITE_SOCKET(a,b,c,d)       TRI_writesocket((a), (b), (c), (d))

#define TRI_LAST_ERROR_STR              strerror(errno)

// ...........................................................................
// under windows group identifiers and user identifiers are
// security identifiers (SID) which is a variable length structure
// which can (should) not be accessed directly.
// ...........................................................................

#define TRI_uid_t                       void*
#define TRI_gid_t                       void*

// ...........................................................................
// windows does not like the keyword inline -- but only if it uses the c compiler
// weird. _inline should work for both I hope
// ...........................................................................

#define inline                          _inline

// ...........................................................................
// windows uses _alloca instead of alloca
// ...........................................................................

#define alloca                          _alloca



#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              posix or windows I/O
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    GNU C compiler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#define TRI_HAVE_GCC_ATTRIBUTE          1
#define TRI_HAVE_GCC_BUILTIN            1
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                size_t overloading
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

#if defined(TRI_OVERLOAD_FUNCS_SIZE_T)
#if TRI_SIZEOF_SIZE_T == 8
#define sizetint_t                      uint64_t
#else
#define sizetint_t                      uint32_t
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
