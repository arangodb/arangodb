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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

#define TRI_WITHIN_COMMON 1
// clang-format off
#include "Basics/Result.h"
#include "Basics/operating-system.h"
#include "Basics/application-exit.h"
// clang-format on
#undef TRI_WITHIN_COMMON

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define TRI_WITHIN_COMMON 1
// clang-format off
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"
// clang-format on
#undef TRI_WITHIN_COMMON

#ifdef _WIN32
// some Windows headers define macros named free and small,
// leading to follow-up compile errors
#undef free
#undef small
// Windows debug mode also seems to define DEBUG preproc symbol
#undef DEBUG
#endif


#ifdef ARANGODB_USE_GOOGLE_TESTS
#define TEST_VIRTUAL virtual
#else
#define TEST_VIRTUAL
#endif

/// @brief helper macro for calculating strlens for static strings at
/// a compile-time (unless compiled with fno-builtin-strlen etc.)
#define TRI_CHAR_LENGTH_PAIR(value) (value), strlen(value)

/// @brief assert
#ifndef TRI_ASSERT

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define TRI_ASSERT(expr)                             \
  do {                                               \
    if (!(ADB_LIKELY(expr))) {                       \
      TRI_FlushDebugging(__FILE__, __LINE__, #expr); \
      TRI_PrintBacktrace();                          \
      std::abort();                                  \
    }                                                \
  } while (0)

#else

#define TRI_ASSERT(expr) \
  while (0) {            \
    (void)(expr);        \
  }                      \
  do {                   \
  } while (0)

#endif

#endif

/// @brief aborts program execution, returning an error code
/// if backtraces are enabled, a backtrace will be printed before
#define FATAL_ERROR_EXIT_CODE(code)                           \
  do {                                                        \
    TRI_LogBacktrace();                                       \
    ::arangodb::basics::CleanupFunctions::run(code, nullptr); \
    ::arangodb::Logger::flush();                              \
    ::arangodb::Logger::shutdown();                           \
    TRI_EXIT_FUNCTION(code, nullptr);                         \
    exit(code);                                               \
  } while (0)

/// @brief aborts program execution, returning an error code
/// if backtraces are enabled, a backtrace will be printed before
#define FATAL_ERROR_EXIT(...)            \
  do {                                   \
    FATAL_ERROR_EXIT_CODE(EXIT_FAILURE); \
  } while (0)

/// @brief aborts program execution, calling std::abort
/// if backtraces are enabled, a backtrace will be printed before
#define FATAL_ERROR_ABORT(...)                             \
  do {                                                     \
    TRI_LogBacktrace();                                    \
    arangodb::basics::CleanupFunctions::run(500, nullptr); \
    arangodb::Logger::flush();                             \
    arangodb::Logger::shutdown();                          \
    std::abort();                                          \
  } while (0)

#ifdef _WIN32
#include "Basics/win-utils.h"
#else
inline void ADB_WindowsEntryFunction() {}
inline void ADB_WindowsExitFunction(int, void*) {}
#endif

#undef TRI_SHOW_LOCK_TIME
#define TRI_SHOW_LOCK_THRESHOLD 0.000199

#ifdef sleep
#undef sleep
#endif
#define sleep ERROR_USE_std_this_thread_sleep_for

#endif
