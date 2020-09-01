////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_SYSTEM__COMPILER_H
#define ARANGODB_BASICS_SYSTEM__COMPILER_H 1

#include "Basics/operating-system.h"

// warn if return value is unused
#if defined(__GNUC__) || defined(__GNUG__)
#define ADB_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(__clang__)
#define ADB_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define ADB_WARN_UNUSED_RESULT /* unused */
#endif

// suppress unused variable warning
#if defined(__GNUC__)
#define ADB_IGNORE_UNUSED __attribute__((unused))
#else
#define ADB_IGNORE_UNUSED /* unused */
#endif

// unreachable code marker
#if defined(_MSC_VER)
#define ADB_UNREACHABLE __assume(false)
#elif defined(__GNUC__) || defined(__GNUG__)
#define ADB_UNREACHABLE __builtin_unreachable()
#elif defined(__clang__)
#define ADB_UNREACHABLE __builtin_unreachable()
#else
#define ADB_UNREACHABLE \
  TRI_ASSERT(false);    \
  std::abort()
#endif

// likely/unlikely branch indicator
// macro definitions similar to the ones at
// https://kernelnewbies.org/FAQ/LikelyUnlikely
#if defined(__GNUC__) || defined(__GNUG__)
#define ADB_LIKELY(v) __builtin_expect(!!(v), 1)
#define ADB_UNLIKELY(v) __builtin_expect(!!(v), 0)
#else
#define ADB_LIKELY(v) v
#define ADB_UNLIKELY(v) v
#endif

// sizetint_t
#if defined(TRI_OVERLOAD_FUNCS_SIZE_T)
#if TRI_SIZEOF_SIZE_T == 8
#define sizetint_t uint64_t
#else
#define sizetint_t uint32_t
#endif
#endif

#endif
