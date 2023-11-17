////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <compare>
#include <cstdint>

namespace arangodb {

struct ShardID {
  explicit ShardID(uint64_t id) : id(id) {}

  uint64_t id;
  friend auto operator<=>(ShardID const&, ShardID const&) = default;
};
}  // namespace arangodb

template<>
struct std::hash<arangodb::ShardID> {
  [[nodiscard]] auto operator()(arangodb::ShardID const& v) const noexcept
      -> std::size_t {
    return std::hash<uint64_t>{}(v.id);
  }
};