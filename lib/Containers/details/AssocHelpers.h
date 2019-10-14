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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CONTAINERS_ASSOC_HELPERS_H
#define ARANGODB_CONTAINERS_ASSOC_HELPERS_H 1

#include <cstdint>

/// @brief incrementing a uint64_t modulo a number with wraparound
static inline uint64_t TRI_IncModU64(uint64_t i, uint64_t len) {
  // Note that the dummy variable gives the compiler a (good) chance to
  // use a conditional move instruction instead of a branch. This actually
  // works on modern gcc.
  uint64_t dummy;
  dummy = (++i) - len;
  return i < len ? i : dummy;
}

/// @brief a trivial hash function for uint64_t to uint32_t
static inline uint32_t TRI_64To32(uint64_t x) {
  return static_cast<uint32_t>(x >> 32) ^ static_cast<uint32_t>(x);
}

#endif
