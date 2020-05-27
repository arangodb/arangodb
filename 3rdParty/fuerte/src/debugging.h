////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_FUERTE_DEBUGGING_H
#define ARANGODB_FUERTE_DEBUGGING_H 1

#include <fuerte/FuerteLogger.h>


#ifndef ADB_LIKELY

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

#endif  // ADB_LIKELY


/// @brief assert
#ifndef FUERTE_ASSERT

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define FUERTE_ASSERT(expr)                          \
  do {                                               \
    if (!(ADB_LIKELY(expr))) {                       \
      std::cout << "broken assert (" << #expr        \
                << ") in " << __FILE__ << ":"        \
                << __LINE__ << std::endl;            \
      std::abort();                                  \
    }                                                \
  } while (0)

#else

#define FUERTE_ASSERT(expr) \
  while (0) {            \
    (void)(expr);        \
  }                      \
  do {                   \
  } while (0)

#endif  // #ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#endif  // #ifndef FUERTE_ASSERT


#endif
