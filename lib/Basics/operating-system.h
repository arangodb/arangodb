////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_OPERATING__SYSTEM_H
#define ARANGODB_BASICS_OPERATING__SYSTEM_H 1

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

// -----------------------------------------------------------------------------
// --Section--                                                processor features
// -----------------------------------------------------------------------------

// padding

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || \
    defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
#undef TRI_PADDING_32
#else
#define TRI_PADDING_32 1
#endif

// aligned / unaligned access

#if defined(__sparc__) || defined(__arm__)
/* unaligned accesses not allowed */
#undef TRI_UNALIGNED_ACCESS
#elif defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC)
/* unaligned accesses are slow */
#undef TRI_UNALIGNED_ACCESS
#elif defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
/* unaligned accesses should work */
#define TRI_UNALIGNED_ACCESS 1
#else
/* unknown platform. better not use unaligned accesses */
#undef TRI_UNALIGNED_ACCESS
#endif

// -----------------------------------------------------------------------------
// --Section--                                                       v8 features
// -----------------------------------------------------------------------------

#if defined(__arm__) || defined(__aarch64__)
#define TRI_V8_MAXHEAP 1 * 1024
#elif TRI_PADDING_32
#define TRI_V8_MAXHEAP 1 * 1024
#else
#define TRI_V8_MAXHEAP 3 * 1024
#endif

// -----------------------------------------------------------------------------
// --Section--                                                             apple
// -----------------------------------------------------------------------------

#ifdef __APPLE__

#define TRI_PLATFORM "darwin"

// necessary defines and includes

#include <stdint.h>

#define ARANGODB_GETRUSAGE_MAXRSS_UNIT 1

// enabled features

#define ARANGODB_ENABLE_SYSLOG 1

// available include files

#define TRI_HAVE_ARPA_INET_H 1
#define TRI_HAVE_DIRENT_H 1
#define TRI_HAVE_DLFCN_H 1
#define TRI_HAVE_GETRLIMIT 1
#define TRI_HAVE_NETDB_H 1
#define TRI_HAVE_NETINET_STAR_H 1
#define TRI_HAVE_POLL_H 1
#define TRI_HAVE_SCHED_H 1
#define TRI_HAVE_SIGNAL_H 1
#define TRI_HAVE_STDBOOL_H 1
#define TRI_HAVE_SYS_FILE_H 1
#define TRI_HAVE_SYS_IOCTL_H 1
#define TRI_HAVE_SYS_RESOURCE_H 1
#define TRI_HAVE_SYS_SOCKET_H 1
#define TRI_HAVE_SYS_TIME_H 1
#define TRI_HAVE_SYS_TYPES_H 1
#define TRI_HAVE_SYS_WAIT_H 1
#define TRI_HAVE_TERMIOS_H 1
#define TRI_HAVE_UNISTD_H 1

// available functions

#define ARANGODB_HAVE_FORK 1
#define ARANGODB_HAVE_GETGRGID 1
#define ARANGODB_HAVE_GETGRNAM 1
#define ARANGODB_HAVE_GETPPID 1
#define ARANGODB_HAVE_GETPWNAM 1
#define ARANGODB_HAVE_GETPWUID 1
#define ARANGODB_HAVE_GETRUSAGE 1
#define ARANGODB_HAVE_GMTIME_R 1
#undef ARANGODB_HAVE_GMTIME_S
#define ARANGODB_HAVE_INITGROUPS 1
#define ARANGODB_HAVE_LOCALTIME_R 1
#undef ARANGODB_HAVE_LOCALTIME_S
#define ARANGODB_HAVE_SETGID 1
#define ARANGODB_HAVE_SETUID 1

#define TRI_random random
#define TRI_srandom srandom

// available features

#define TRI_HAVE_POSIX 1

#define ARANGODB_HAVE_DOMAIN_SOCKETS 1
#define ARANGODB_HAVE_THREAD_POLICY 1
#define TRI_HAVE_MACH 1
#define TRI_HAVE_MACOS_MEM_STATS 1
#define TRI_HAVE_POSIX_MMAP 1
#define TRI_HAVE_POSIX_PWD_GRP 1
#define TRI_HAVE_POSIX_THREADS 1

#define TRI_HAVE_ANONYMOUS_MMAP 1

#define TRI_OVERLOAD_FUNCS_SIZE_T 1

#define ARANGODB_MISSING_MEMRCHR 1

#define TRI_SC_NPROCESSORS_ONLN 1

// alignment and limits

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T (8)
#define TRI_ALIGNOF_VOIDP (8)
#else
#define TRI_SIZEOF_SIZE_T (4)
#define TRI_ALIGNOF_VOIDP (4)
#endif

#ifndef SIZE_MAX
#if __WORDSIZE == 64
#define SIZE_MAX (18446744073709551615UL)
#else
#define SIZE_MAX (4294967295U)
#endif
#endif

// files

#define TRI_DIR_SEPARATOR_CHAR '/'
#define TRI_DIR_SEPARATOR_STR "/"

#define TRI_O_CLOEXEC O_CLOEXEC
#define TRI_NOATIME 0

#define TRI_CHDIR ::chdir
#define TRI_CLOSE ::close
#define TRI_CREATE(a, b, c) ::open((a), (b), (c))
#define TRI_FSTAT ::fstat
#define TRI_GETCWD ::getcwd
#define TRI_LSEEK ::lseek
#define TRI_MKDIR(a, b) ::mkdir((a), (b))
#define TRI_OPEN(a, b) ::open((a), (b))
#define TRI_FOPEN(a, b) ::fopen((a), (b))
#define TRI_READ ::read
#define TRI_DUP ::dup
#define TRI_RMDIR ::rmdir
#define TRI_STAT ::stat
#define TRI_STAT_ATIME_SEC(statbuf) statbuf.st_atimespec.tv_sec
#define TRI_STAT_MTIME_SEC(statbuf) statbuf.st_mtimespec.tv_sec
#define TRI_UNLINK ::unlink
#define TRI_WRITE ::write
#define TRI_FDOPEN(a, b) ::fdopen((a), (b))
#define TRI_IS_INVALID_PIPE(a) ((a) == 0)

#define TRI_lseek_t off_t
#define TRI_read_t size_t
#define TRI_read_return_t ssize_t
#define TRI_stat_t struct stat
#define TRI_write_t size_t

#define TRI_ERRORBUF \
  {}
#define TRI_GET_ERRORBUF ::strerror(errno)
#define TRI_LAST_ERROR_STR ::strerror(errno)
#define TRI_SYSTEM_ERROR() \
  {}
#define TRI_GET_ARGV(ARGC, ARGV)

// sockets

#define TRI_CONNECT_AI_FLAGS (AI_PASSIVE | AI_NUMERICSERV | AI_ALL)

#define TRI_INVALID_SOCKET -1

#define TRI_CLOSE_SOCKET TRI_closesocket
#define TRI_READ_SOCKET(a, b, c, d) TRI_readsocket((a), (b), (c), (d))
#define TRI_WRITE_SOCKET(a, b, c, d) TRI_writesocket((a), (b), (c), (d))

// user and group types

#define TRI_uid_t uid_t
#define TRI_gid_t gid_t

#endif

// -----------------------------------------------------------------------------
// --Section--                                                           freebsd
// -----------------------------------------------------------------------------

#ifdef __FreeBSD__

// necessary defines and includes

#define TRI_PLATFORM "freebsd"

#define _WITH_GETLINE

#include <stdint.h>

#ifndef __LONG_LONG_SUPPORTED
#define __LONG_LONG_SUPPORTED 1
#endif

#define ARANGODB_GETRUSAGE_MAXRSS_UNIT 1024

// enabled features

#define ARANGODB_ENABLE_SYSLOG 1

// available include files

#define TRI_HAVE_DIRENT_H 1
#define TRI_HAVE_DLFCN_H 1
#define ARANGODB_HAVE_FORK 1
#define TRI_HAVE_GETRLIMIT 1
#define TRI_HAVE_LIMITS_H 1
#define TRI_HAVE_POLL_H 1
#define TRI_HAVE_SCHED_H 1
#define TRI_HAVE_SIGNAL_H 1
#define TRI_HAVE_STDBOOL_H 1
#define TRI_HAVE_STRINGS_H 1
#define TRI_HAVE_TERMIOS_H 1
#define TRI_HAVE_UNISTD_H 1

#define TRI_HAVE_SYS_FILE_H 1
#define TRI_HAVE_SYS_IOCTL_H 1
#define TRI_HAVE_SYS_RESOURCE_H 1
#define TRI_HAVE_SYS_TIME_H 1
#define TRI_HAVE_SYS_TYPES_H 1
#define TRI_HAVE_SYS_WAIT_H 1

// available functions

#define ARANGODB_HAVE_GETGRGID 1
#define ARANGODB_HAVE_GETGRNAM 1
#define ARANGODB_HAVE_GETPPID 1
#define ARANGODB_HAVE_GETPWNAM 1
#define ARANGODB_HAVE_GETPWUID 1
#define ARANGODB_HAVE_GETRUSAGE 1
#define ARANGODB_HAVE_GMTIME_R 1
#undef ARANGODB_HAVE_GMTIME_S
#undef ARANGODB_HAVE_INITGROUPS
#define ARANGODB_HAVE_LOCALTIME_R 1
#undef ARANGODB_HAVE_LOCALTIME_S
#define ARANGODB_HAVE_SETGID 1
#define ARANGODB_HAVE_SETUID 1

#define TRI_random ::rand
#define TRI_srandom ::srand

// available features

#define TRI_HAVE_POSIX 1

#define TRI_HAVE_LINUX_PROC 1
#define ARANGODB_HAVE_DOMAIN_SOCKETS 1
#define TRI_HAVE_POSIX_MMAP 1
#define TRI_HAVE_POSIX_PWD_GRP 1
#define TRI_HAVE_POSIX_THREADS 1

#define TRI_HAVE_ANONYMOUS_MMAP 1

// alignment and limits

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T (8)
#define TRI_ALIGNOF_VOIDP (8)
#else
#define TRI_SIZEOF_SIZE_T (4)
#define TRI_ALIGNOF_VOIDP (4)
#endif

// files

#define TRI_DIR_SEPARATOR_CHAR '/'
#define TRI_DIR_SEPARATOR_STR "/"

#define TRI_O_CLOEXEC O_CLOEXEC
#define TRI_NOATIME 0

#define TRI_CHDIR ::chdir
#define TRI_CLOSE ::close
#define TRI_CREATE(a, b, c) ::open((a), (b), (c))
#define TRI_FSTAT ::fstat
#define TRI_GETCWD ::getcwd
#define TRI_LSEEK ::lseek
#define TRI_MKDIR(a, b) ::mkdir((a), (b))
#define TRI_OPEN(a, b) ::open((a), (b))
#define TRI_FOPEN(a, b) ::fopen((a), (b))
#define TRI_READ ::read
#define TRI_DUP ::dup
#define TRI_RMDIR ::rmdir
#define TRI_STAT ::stat
#define TRI_STAT_ATIME_SEC(statbuf) statbuf.st_atimespec.tv_sec
#define TRI_STAT_MTIME_SEC(statbuf) statbuf.st_mtimespec.tv_sec
#define TRI_UNLINK ::unlink
#define TRI_WRITE ::write
#define TRI_FDOPEN(a, b) ::fdopen((a), (b))
#define TRI_IS_INVALID_PIPE(a) ((a) == 0)

#define TRI_lseek_t off_t
#define TRI_read_t size_t
#define TRI_read_return_t ssize_t
#define TRI_stat_t struct stat
#define TRI_write_t size_t

#define TRI_ERRORBUF \
  {}
#define TRI_GET_ERRORBUF ::strerror(errno)
#define TRI_LAST_ERROR_STR ::strerror(errno)
#define TRI_SYSTEM_ERROR() \
  {}
#define TRI_GET_ARGV(ARGC, ARGV)

// sockets

#define TRI_CONNECT_AI_FLAGS (AI_PASSIVE | AI_NUMERICSERV)

#define TRI_INVALID_SOCKET -1

#define TRI_CLOSE_SOCKET TRI_closesocket
#define TRI_READ_SOCKET(a, b, c, d) TRI_readsocket((a), (b), (c), (d))
#define TRI_WRITE_SOCKET(a, b, c, d) TRI_writesocket((a), (b), (c), (d))

// user and group types

#define TRI_uid_t uid_t
#define TRI_gid_t gid_t

#endif

// -----------------------------------------------------------------------------
// --Section--                                                             linux
// -----------------------------------------------------------------------------

#ifdef __linux__

// necessary defines and includes

#define TRI_PLATFORM "linux"

// force posix source
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

// first include the features file and then define
#include <features.h>

// for INTxx_MIN and INTxx_MAX
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#define ARANGODB_GETRUSAGE_MAXRSS_UNIT 1024

// enabled features

#define ARANGODB_ENABLE_SYSLOG 1

// available include files

#define TRI_HAVE_ARPA_INET_H 1
#define TRI_HAVE_DIRENT_H 1
#define TRI_HAVE_DLFCN_H 1
#define TRI_HAVE_GETRLIMIT 1
#define TRI_HAVE_NETDB_H 1
#define TRI_HAVE_NETINET_STAR_H 1
#define TRI_HAVE_POLL_H 1
#define TRI_HAVE_SCHED_H 1
#define TRI_HAVE_SIGNAL_H 1
#define TRI_HAVE_STDBOOL_H 1
#define TRI_HAVE_SYS_FILE_H 1
#define TRI_HAVE_SYS_IOCTL_H 1
#define TRI_HAVE_SYS_PRCTL_H 1
#define TRI_HAVE_SYS_RESOURCE_H 1
#define TRI_HAVE_SYS_SOCKET_H 1
#define TRI_HAVE_SYS_TIME_H 1
#define TRI_HAVE_SYS_TYPES_H 1
#define TRI_HAVE_SYS_WAIT_H 1
#define TRI_HAVE_TERMIOS_H 1
#define TRI_HAVE_UNISTD_H 1

// available functions

#define ARANGODB_HAVE_FORK 1
#define ARANGODB_HAVE_GETGRGID 1
#define ARANGODB_HAVE_GETGRNAM 1
#define ARANGODB_HAVE_GETPPID 1
#define ARANGODB_HAVE_GETPWNAM 1
#define ARANGODB_HAVE_GETPWUID 1
#define ARANGODB_HAVE_GETRUSAGE 1
#define ARANGODB_HAVE_GMTIME_R 1
#undef ARANGODB_HAVE_GMTIME_S
#define ARANGODB_HAVE_INITGROUPS 1
#define ARANGODB_HAVE_LOCALTIME_R 1
#undef ARANGODB_HAVE_LOCALTIME_S
#define ARANGODB_HAVE_SETGID 1
#define ARANGODB_HAVE_SETUID 1

#define TRI_HAVE_PRCTL 1

// available features

#define TRI_HAVE_POSIX 1

#define ARANGODB_HAVE_DOMAIN_SOCKETS 1
#define ARANGODB_HAVE_THREAD_AFFINITY 1
#define TRI_HAVE_LINUX_PROC 1
#define TRI_HAVE_POSIX_MMAP 1
#define TRI_HAVE_POSIX_PWD_GRP 1
#define TRI_HAVE_POSIX_THREADS 1
#define TRI_HAVE_SC_PHYS_PAGES 1

#define TRI_HAVE_ANONYMOUS_MMAP 1

#define TRI_SC_NPROCESSORS_ONLN 1

#define TRI_random ::rand
#define TRI_srandom ::srand

// alignment and limits

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T (8)
#define TRI_ALIGNOF_VOIDP (8)
#else
#define TRI_SIZEOF_SIZE_T (4)
#define TRI_ALIGNOF_VOIDP (4)
#endif

// files

#define TRI_DIR_SEPARATOR_CHAR '/'
#define TRI_DIR_SEPARATOR_STR "/"

#define TRI_O_CLOEXEC O_CLOEXEC
#define TRI_NOATIME O_NOATIME

#define TRI_CHDIR ::chdir
#define TRI_CLOSE ::close
#define TRI_CREATE(a, b, c) ::open((a), (b), (c))
#define TRI_FSTAT ::fstat
#define TRI_GETCWD ::getcwd
#define TRI_LSEEK ::lseek
#define TRI_MKDIR(a, b) ::mkdir((a), (b))
#define TRI_OPEN(a, b) ::open((a), (b))
#define TRI_FOPEN(a, b) ::fopen((a), (b))
#define TRI_READ ::read
#define TRI_DUP ::dup
#define TRI_RMDIR ::rmdir
#define TRI_STAT ::stat
#define TRI_STAT_ATIME_SEC(statbuf) statbuf.st_atim.tv_sec
#define TRI_STAT_MTIME_SEC(statbuf) statbuf.st_mtim.tv_sec
#define TRI_UNLINK ::unlink
#define TRI_WRITE ::write
#define TRI_FDOPEN(a, b) ::fdopen((a), (b))
#define TRI_IS_INVALID_PIPE(a) ((a) == 0)

#define TRI_lseek_t off_t
#define TRI_read_t size_t
#define TRI_read_return_t ssize_t
#define TRI_stat_t struct stat
#define TRI_write_t size_t

#define TRI_ERRORBUF \
  {}
#define TRI_GET_ERRORBUF ::strerror(errno)
#define TRI_LAST_ERROR_STR ::strerror(errno)
#define TRI_SYSTEM_ERROR() \
  {}
#define TRI_GET_ARGV(ARGC, ARGV)

// sockets

#define TRI_CONNECT_AI_FLAGS (AI_PASSIVE | AI_NUMERICSERV | AI_ALL)

#define TRI_INVALID_SOCKET -1

#define TRI_CLOSE_SOCKET TRI_closesocket
#define TRI_READ_SOCKET(a, b, c, d) TRI_readsocket((a), (b), (c), (d))
#define TRI_WRITE_SOCKET(a, b, c, d) TRI_writesocket((a), (b), (c), (d))

// user and group types

#include <sys/types.h>
#define TRI_uid_t uid_t
#define TRI_gid_t gid_t

#endif

// -----------------------------------------------------------------------------
// --Section--                                                           windows
// -----------------------------------------------------------------------------

#if defined(_WIN32) && defined(_MSC_VER)

// necessary defines and includes

#ifdef _WIN64
#define TRI_PLATFORM "win64"
#else
#define TRI_PLATFORM "win32"
#endif
// Visual Studio 2013 does not support noexcept, higher versions do
#if defined(_MSC_FULL_VER) && _MSC_FULL_VER > 180031101
#else
#define noexcept throw()
#endif

// This directive below suppresses warnings about 'inline'
#define _ALLOW_KEYWORD_MACROS 1

// This directive below suppresses warnings about using the 'new' more secure
// CRT functions.
#define _CRT_SECURE_NO_WARNINGS 1

// This directive below provides a manner in which the 'new' more
// secure functions.  For example, strcpy is automatically converted
// to strcpy_s. This is enabled by default. We have disabled it here.

//#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES     1

#include <WinSock2.h>  // must be before windows.h
#include <io.h>
#include <stdio.h>
// available include files

#define TRI_HAVE_DIRECT_H 1
#define TRI_HAVE_PROCESS_H 1
#define TRI_HAVE_SIGNAL_H 1
#define TRI_HAVE_WINSOCK2_H 1

// available functions

#undef ARANGODB_HAVE_GETGRGID
#undef ARANGODB_HAVE_GETGRNAM
#undef ARANGODB_HAVE_GETPPID
#undef ARANGODB_HAVE_GETPWNAM
#undef ARANGODB_HAVE_GETPWUID
#undef ARANGODB_HAVE_GETRUSAGE
#undef ARANGODB_HAVE_GMTIME_R
#define ARANGODB_HAVE_GMTIME_S 1
#undef ARANGODB_HAVE_INITGROUPS
#undef ARANGODB_HAVE_LOCALTIME_R
#define ARANGODB_HAVE_LOCALTIME_S 1
#undef ARANGODB_HAVE_SETGID
#undef ARANGODB_HAVE_SETUID

#define TRI_HAVE_WIN32_GLOBAL_MEMORY_STATUS 1

#define TRI_random ::rand
#define TRI_srandom ::srand

#if ( defined(_MSC_VER) && _MSC_VER < 1900 ) || ( defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR) )
#define snprintf _snprintf
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#define fileno _fileno
#define fsync _commit
#define isatty _cyg_isatty
#define putenv _putenv
#define tzset _tzset

// available features

#define YY_NO_UNISTD_H 1

#define TRI_WIN32_CONSOLE 1

#define TRI_HAVE_WIN32_CLOSE_ON_EXEC 1
#define TRI_HAVE_WIN32_FILE_LOCKING 1
#define TRI_HAVE_WIN32_GETTIMEOFDAY 1
#define TRI_HAVE_WIN32_LIST_FILES 1
#define TRI_HAVE_WIN32_MMAP 1
#define TRI_HAVE_WIN32_NON_BLOCKING 1
#define TRI_HAVE_WIN32_PWD 1
#define TRI_HAVE_WIN32_SOCKETS 1
#define TRI_HAVE_WIN32_SYMBOLIC_LINK 1
#define TRI_HAVE_WIN32_THREADS 1

#define TRI_HAVE_ANONYMOUS_MMAP 1
#define ARANGODB_MISSING_MEMRCHR 1

typedef int ssize_t;

#ifndef va_copy
#define va_copy(d, s) ((d) = (s))
#endif

// typedef unsigned int bool; - this never ever going to work. Problem is
// sizeof(bool) in VS C++ is 1 byte and sizeof(bool) in VS C (C compiler) is --
// whatever you want. However, when structures are interchanged between C & C++
// (as in arango) all hell will break loose.

#ifndef __BOOL_DEFINED
typedef unsigned char bool;
#define true 1
#define false 0
#endif

// windows uses _alloca instead of alloca

#define alloca _alloca

// alignment and limits

#if __WORDSIZE == 64
#define TRI_SIZEOF_SIZE_T (8)
#define TRI_ALIGNOF_VOIDP (8)
#else
#define TRI_SIZEOF_SIZE_T (4)
#define TRI_ALIGNOF_VOIDP (4)
#endif

// files

#define TRI_DIR_SEPARATOR_CHAR '\\'
#define TRI_DIR_SEPARATOR_STR "\\"

// we do not have owner read and owner write under windows; so map these to
// global read, global write these are used when creating a file

#ifdef S_IRGRP
#undef S_IRGRP
#endif
#ifdef S_IRUSR
#undef S_IRUSR
#endif
#ifdef S_IWGRP
#undef S_IWGRP
#endif
#ifdef S_IWUSR
#undef S_IWUSR
#endif

#define S_IRGRP _S_IREAD
#define S_IRUSR _S_IREAD
#define S_IWGRP _S_IWRITE
#define S_IWUSR _S_IWRITE

#define TRI_O_CLOEXEC 0
#define TRI_NOATIME 0

#define O_RDONLY _O_RDONLY

#define TRI_CLOSE ::_close
#define TRI_CREATE(a, b, c) TRI_createFile((a), (b), (c))
#define TRI_FSTAT ::_fstat64
#define TRI_LSEEK ::_lseeki64
#define TRI_MKDIR(a, b) TRI_MKDIR_WIN32(a)
#define TRI_OPEN(a, b) TRI_OPEN_WIN32((a), (b))
#define TRI_READ(a, b, c) static_cast<ssize_t>(::_read(a, b, c))
#define TRI_DUP ::_dup
#define TRI_WRITE ::_write
#define TRI_FDOPEN(a, b) ::_fdopen((a), (b))
#define TRI_IS_INVALID_PIPE(a) ((a) == INVALID_HANDLE_VALUE)

#define TRI_lseek_t __int64
#define TRI_read_t unsigned int
#define TRI_read_return_t int

#define TRI_stat_t struct _stat64
#define TRI_write_t unsigned int

#define TRI_LAST_ERROR_STR ::strerror(errno)

#define TRI_ERRORBUF char windowsErrorBuf[256] = "";
#define TRI_GET_ERRORBUF windowsErrorBuf
#define TRI_GET_ARGV(ARGC, ARGV) TRI_GET_ARGV_WIN(ARGC, ARGV)

// Implemented wrappers in win-utils.cpp:
FILE* TRI_FOPEN(char const* filename, char const* mode);
int TRI_CHDIR(char const* dirname);
int TRI_STAT(char const* path, TRI_stat_t* buffer);
char* TRI_GETCWD(char* buffer, int maxlen);
int TRI_MKDIR_WIN32(char const* dirname);
int TRI_RMDIR(char const* dirname);
int TRI_UNLINK(char const* filename);
void TRI_GET_ARGV_WIN(int& argc, char** argv);

// system error string macro requires ERRORBUF to instantiate its buffer before.
#define TRI_SYSTEM_ERROR()                                                    \
  do {                                                                        \
    auto result = translateWindowsError(::GetLastError());                    \
    errno = static_cast<int>(result.errorNumber());                           \
    auto const& mesg = result.errorMessage();                                 \
    memset(windowsErrorBuf, 0, sizeof(windowsErrorBuf));                      \
    if (mesg.empty()) {                                                       \
      memcpy(&windowsErrorBuf[0], "unknown error", strlen("unknown error"));  \
    } else {                                                                  \
      memcpy(&windowsErrorBuf[0], mesg.data(),                                \
             (std::min)(sizeof(windowsErrorBuf) / sizeof(windowsErrorBuf[0]), \
                        mesg.size()));                                        \
    }                                                                         \
  } while (false)

#define STDERR_FILENO 2
#define STDIN_FILENO 0
#define STDOUT_FILENO 1

// sockets

#define TRI_CONNECT_AI_FLAGS (AI_PASSIVE | AI_NUMERICSERV | AI_ALL)

#define TRI_INVALID_SOCKET INVALID_SOCKET

#define TRI_CLOSE_SOCKET TRI_closesocket
#define TRI_READ_SOCKET(a, b, c, d) TRI_readsocket((a), (b), (c), (d))
#define TRI_WRITE_SOCKET(a, b, c, d) TRI_writesocket((a), (b), (c), (d))

// user and group types

// under windows group identifiers and user identifiers are
// security identifiers (SID) which is a variable length structure
// which can (should) not be accessed directly.

#define TRI_uid_t void*
#define TRI_gid_t void*

#endif

#endif
