////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_COMMON_H
#define ARANGODB_CACHE_COMMON_H

#include "Basics/Common.h"

#include <stdint.h>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Common size for all bucket types.
////////////////////////////////////////////////////////////////////////////////
constexpr size_t BUCKET_SIZE = 128;

////////////////////////////////////////////////////////////////////////////////
/// @brief Enum to specify cache types.
////////////////////////////////////////////////////////////////////////////////
enum CacheType { Plain, Transactional };

////////////////////////////////////////////////////////////////////////////////
/// @brief Enum to allow easy statistic recording across classes.
////////////////////////////////////////////////////////////////////////////////
enum class Stat : uint8_t { findHit = 1, findMiss = 2 };

////////////////////////////////////////////////////////////////////////////////
/// @brief Function to let CPU relax inside spinlock.
////////////////////////////////////////////////////////////////////////////////
// TODO use <boost/fiber/detail/cpu_relax.hpp> when available (>1.65.0?)
#if defined(__i386) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
  #if defined __WIN32__
    #define cpu_relax() YieldProcessor();
  #else 
    #define cpu_relax() asm volatile ("pause" ::: "memory");
  #endif
#else
  #define cpu_relax() { \
    static constexpr std::chrono::microseconds us0{ 0 }; \
    std::this_thread::sleep_for( us0); \
  }
#endif


};  // end namespace cache
};  // end namespace arangodb

#endif
