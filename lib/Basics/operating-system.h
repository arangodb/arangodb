////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

// -----------------------------------------------------------------------------
// --Section--                                                       v8 features
// -----------------------------------------------------------------------------

#if defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
#define TRI_V8_MAXHEAP 1 * 1024
#else
#define TRI_V8_MAXHEAP 3 * 1024
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

// available features

#define TRI_HAVE_POSIX 1

#define TRI_HAVE_LINUX_PROC 1
#define ARANGODB_HAVE_DOMAIN_SOCKETS 1
#define TRI_HAVE_POSIX_PWD_GRP 1
#define TRI_HAVE_POSIX_THREADS 1

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
#define TRI_O_TMPFILE O_TMPFILE
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
#define TRI_STAT_ATIME_SEC(statbuf) (statbuf).st_atimespec.tv_sec
#define TRI_STAT_MTIME_SEC(statbuf) (statbuf).st_mtimespec.tv_sec
#define TRI_UNLINK ::unlink
#define TRI_WRITE ::write
#define TRI_FDOPEN(a, b) ::fdopen((a), (b))
#define TRI_IS_INVALID_PIPE(a) ((a) == 0)

#define TRI_lseek_t off_t
#define TRI_read_t size_t
#define TRI_read_return_t ssize_t
#define TRI_stat_t struct stat
#define TRI_write_t size_t

#define TRI_LAST_ERROR_STR ::strerror(errno)
#define TRI_GET_ARGV(ARGC, ARGV)

// sockets

#define TRI_CONNECT_AI_FLAGS (AI_PASSIVE | AI_NUMERICSERV)

#define TRI_INVALID_SOCKET -1

#define TRI_CLOSE_SOCKET TRI_closesocket
#define TRI_READ_SOCKET(a, b, c, d) TRI_readsocket((a), (b), (c), (d))

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
#define TRI_HAVE_POSIX_PWD_GRP 1
#define TRI_HAVE_POSIX_THREADS 1
#define TRI_HAVE_SC_PHYS_PAGES 1

#define TRI_SC_NPROCESSORS_ONLN 1

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
#define TRI_O_TMPFILE O_TMPFILE
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
#define TRI_STAT_ATIME_SEC(statbuf) (statbuf).st_atim.tv_sec
#define TRI_STAT_MTIME_SEC(statbuf) (statbuf).st_mtim.tv_sec
#define TRI_UNLINK ::unlink
#define TRI_WRITE ::write
#define TRI_FDOPEN(a, b) ::fdopen((a), (b))
#define TRI_IS_INVALID_PIPE(a) ((a) == 0)

#define TRI_lseek_t off_t
#define TRI_read_t size_t
#define TRI_read_return_t ssize_t
#define TRI_stat_t struct stat
#define TRI_write_t size_t

#define TRI_LAST_ERROR_STR ::strerror(errno)
#define TRI_GET_ARGV(ARGC, ARGV)

// sockets

#define TRI_CONNECT_AI_FLAGS (AI_PASSIVE | AI_NUMERICSERV | AI_ALL)

#define TRI_INVALID_SOCKET -1

#define TRI_CLOSE_SOCKET TRI_closesocket
#define TRI_READ_SOCKET(a, b, c, d) TRI_readsocket((a), (b), (c), (d))

// user and group types

#include <sys/types.h>
#define TRI_uid_t uid_t
#define TRI_gid_t gid_t

#endif
