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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <utility>
#include <cstdint>
#include <cstring>

#include "Basics/fasthash.h"

namespace arangodb::cache {

struct BinaryKeyHasher {
  inline std::uint32_t hashKey(void const* key,
                               std::size_t keySize) const noexcept {
    return (std::max)(static_cast<std::uint32_t>(1),
                      fasthash32(key, keySize, 0xdeadbeefUL));
  }

  inline bool sameKey(void const* key1, std::size_t keySize1, void const* key2,
                      std::size_t keySize2) const noexcept {
    return (keySize1 == keySize2 && (0 == std::memcmp(key1, key2, keySize1)));
  }
};

}  // end namespace arangodb::cache
