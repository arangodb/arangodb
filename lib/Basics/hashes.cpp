////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/hashes.h"

namespace arangodb {

static constexpr uint64_t kMagicPrime = 0x00000100000001b3ULL;

/// @brief the FNV hash work horse
static inline uint64_t FnvWork(uint8_t value, uint64_t hash) noexcept {
  return (hash ^ value) * kMagicPrime;
}

/// @brief computes a FNV hash for strings with a length
uint64_t FnvHashBlock(uint64_t hash, void const* buffer,
                      size_t length) noexcept {
  auto const* p = static_cast<uint8_t const*>(buffer);
  auto const* end = p + length;

  while (p < end) {
    hash = FnvWork(*p++, hash);
  }

  return hash;
}

uint64_t FnvHashPointer(void const* buffer, size_t length) noexcept {
  return FnvHashBlock(0xcbf29ce484222325ULL, buffer, length);
}

uint64_t FnvHashString(char const* buffer) noexcept {
  auto const* pFirst = reinterpret_cast<uint8_t const*>(buffer);
  uint64_t nHashVal = 0xcbf29ce484222325ULL;
  while (*pFirst) {
    nHashVal ^= *pFirst++;
    nHashVal *= kMagicPrime;
  }
  return nHashVal;
}

}  // namespace arangodb
