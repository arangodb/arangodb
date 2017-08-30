////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_COMMON_H
#define ARANGODB_BASICS_COMMON_H 1

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

// debug malloc for Windows (only used when DEBUG is set)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

#define TRI_WITHIN_COMMON 1
#include "Basics/operating-system.h"
#include "Basics/application-exit.h"
#undef TRI_WITHIN_COMMON

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_DIRECT_H
#include <direct.h>
#endif

#ifdef TRI_HAVE_POSIX_THREADS
#ifdef _GNU_SOURCE
#include <pthread.h>
#else
#define _GNU_SOURCE
#include <pthread.h>
#undef _GNU_SOURCE
#endif
#endif

#ifdef TRI_HAVE_PROCESS_H
#include <process.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TRI_HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef TRI_HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef TRI_HAVE_NETINET_STAR_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef TRI_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef TRI_HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef TRI_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef TRI_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef TRI_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef TRI_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <sys/stat.h>

// .............................................................................
// The problem we have for visual studio is that if we include WinSock2.h here
// it may conflict later in some other source file. The conflict arises when
// windows.h is included BEFORE WinSock2.h -- this is a visual studio issue. For
// now be VERY careful to ensure that if you need windows.h, then you include
// this file AFTER common.h.
// .............................................................................

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
typedef long suseconds_t;
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define TRI_WITHIN_COMMON 1
#include "Basics/voc-errors.h"
#include "Basics/error.h"
#include "Basics/debugging.h"
#include "Basics/make_unique.h"
#include "Basics/memory.h"
#include "Basics/system-compiler.h"
#include "Basics/system-functions.h"
#undef TRI_WITHIN_COMMON

#ifdef _WIN32
// some Windows headers define macros named free and small, 
// leading to follow-up compile errors
#undef free
#undef small
#endif

/// @brief helper macro for calculating strlens for static strings at
/// a compile-time (unless compiled with fno-builtin-strlen etc.)
#define TRI_CHAR_LENGTH_PAIR(value) (value), strlen(value)

/// @brief assert
#ifndef TRI_ASSERT

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define TRI_ASSERT(expr)                             \
  do {                                               \
    if (!(TRI_LIKELY(expr))) {                       \
      TRI_FlushDebugging(__FILE__, __LINE__, #expr); \
      TRI_PrintBacktrace();                          \
      std::abort();                                  \
    }                                                \
  } while (0)

#else

#define TRI_ASSERT(expr) while (0) { (void) (expr); } do { } while (0)

#endif

#endif

/// @brief aborts program execution, returning an error code
/// if backtraces are enabled, a backtrace will be printed before
#define FATAL_ERROR_EXIT_CODE(code)           \
  do {                                        \
    TRI_LogBacktrace();                       \
    arangodb::Logger::flush();                \
    arangodb::Logger::shutdown();             \
    TRI_EXIT_FUNCTION(code, nullptr);         \
    exit(code);                               \
  } while (0)

/// @brief aborts program execution, returning an error code
/// if backtraces are enabled, a backtrace will be printed before
#define FATAL_ERROR_EXIT(...)                 \
  do {                                        \
    FATAL_ERROR_EXIT_CODE(EXIT_FAILURE);      \
  } while (0)

/// @brief aborts program execution, calling std::abort
/// if backtraces are enabled, a backtrace will be printed before
#define FATAL_ERROR_ABORT(...)                \
  do {                                        \
    TRI_LogBacktrace();                       \
    arangodb::Logger::flush();                \
    arangodb::Logger::shutdown();             \
    std::abort();                             \
  } while (0)

#ifdef _WIN32
#include "Basics/win-utils.h"
#else
inline void ADB_WindowsEntryFunction() {}
inline void ADB_WindowsExitFunction(int exitCode, void* data) {}
#endif

// -----------------------------------------------------------------------------
// --SECTIONS--                                               deferred execution
// -----------------------------------------------------------------------------

/// Use in a function (or scope) as:
///   TRI_DEFER( <ONE_STATEMENT> );
/// and the statement will be called regardless if the function throws or
/// returns regularly.
/// Do not put multiple TRI_DEFERs on a single source code line (will not
/// compile).
/// Multiple TRI_DEFERs in one scope will be executed in reverse order of
/// appearance.
/// The idea to this is from
///   http://blog.memsql.com/c-error-handling-with-auto/
#define TOKEN_PASTE_WRAPPED(x, y) x##y
#define TOKEN_PASTE(x, y) TOKEN_PASTE_WRAPPED(x, y)

template <typename T>
struct TRI_AutoOutOfScope {
  explicit TRI_AutoOutOfScope(T& destructor) : m_destructor(destructor) {}
  ~TRI_AutoOutOfScope() { try { m_destructor(); } catch (...) { } }

 private:
  T& m_destructor;
};


#define TRI_DEFER_INTERNAL(Destructor, funcname, objname) \
  auto funcname = [&]() { Destructor; };                  \
  TRI_AutoOutOfScope<decltype(funcname)> objname(funcname);

#define TRI_DEFER(Destructor)                                     \
  TRI_DEFER_INTERNAL(Destructor, TOKEN_PASTE(auto_fun, __LINE__), \
                     TOKEN_PASTE(auto_obj, __LINE__))

#undef TRI_SHOW_LOCK_TIME
#define TRI_SHOW_LOCK_THRESHOLD 0.000199

#endif
