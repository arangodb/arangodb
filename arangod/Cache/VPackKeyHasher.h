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

#include <cstdint>
#include <string_view>
#include <utility>

#include "Basics/VelocyPackHelper.h"

#include <velocypack/Slice.h>

namespace arangodb::cache {

struct VPackKeyHasher {
  static constexpr std::string_view name() { return "VPackKeyHasher"; }

  static std::uint32_t hashKey(void const* key,
                               std::size_t /*keySize*/) noexcept {
    return (std::max)(std::uint32_t{1},
                      VPackSlice(static_cast<std::uint8_t const*>(key))
                          .normalizedHash32(0xdeadbeefUL));
  }

  static bool sameKey(void const* key1, std::size_t /*keySize1*/,
                      void const* key2, std::size_t /*keySize2*/) noexcept {
    int res = arangodb::basics::VelocyPackHelper::compare(
        VPackSlice(static_cast<std::uint8_t const*>(key1)),
        VPackSlice(static_cast<std::uint8_t const*>(key2)), true);
    return res == 0;
  }
};

}  // end namespace arangodb::cache
