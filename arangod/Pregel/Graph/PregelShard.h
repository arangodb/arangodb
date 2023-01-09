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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <hash>

namespace arangodb::pregel {

struct PregelShard {
  PregelShard() : shard(InvalidSentinel) {}
  explicit PregelShard(uint16_t shard) : shard(shard) {}
  PregelShard(PregelShard const&) = default;
  PregelShard(PregelShard&&) = default;
  auto operator=(PregelShard const&) -> PregelShard& = default;
  auto operator=(PregelShard&&) -> PregelShard& = default;

  // TODO: This operator is just here to make transition easier;
  // once VPackValue is not called on PregelShard anymore it should
  // be removed
  explicit operator arangodb::velocypack::Value() const {
    return VPackValue(static_cast<uint32_t>(shard));
  }

  [[nodiscard]] auto isValid() const noexcept -> bool {
    return shard == InvalidSentinel;
  }

  [[nodiscard]] auto operator<=>(PregelShard const&) const = default;

  uint16_t shard;
  uint16_t constexpr static InvalidSentinel =
      std::numeric_limits<uint16_t>::max();
};

auto const InvalidPregelShard = PregelShard();

}  // namespace arangodb::pregel

namespace std {

template<>
struct hash<arangodb::pregel::PregelShard> {
  std::size_t operator()(
      const arangodb::pregel::PregelShard& k) const noexcept {
    using std::hash;
    return std::hash<std::uint16_t>()(k.shard);
  }
};

}  // namespace std
