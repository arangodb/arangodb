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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Inspection/VPackWithErrorT.h>

namespace arangodb::pregel {

struct PregelShard {
  PregelShard() : _shard(InvalidPregelShard){};
  explicit PregelShard(uint16_t shard) : _shard(shard) {}

  [[nodiscard]] auto isValid() const { return _shard != InvalidPregelShard; };

  [[nodiscard]] auto operator<=>(const PregelShard& rhs) const = default;

  static const std::uint16_t InvalidPregelShard = -1;
  std::uint16_t _shard = InvalidPregelShard;
};

template<typename Inspector>
auto inspect(Inspector& f, PregelShard& x) {
  return f.object(x).fields(f.field("shard", x._shard));
}

}  // namespace arangodb::pregel

namespace std {
template<>
struct hash<arangodb::pregel::PregelShard> {
  std::size_t operator()(
      const arangodb::pregel::PregelShard& k) const noexcept {
    return std::hash<std::uint16_t>()(k._shard);
  }
};

}  // namespace std
