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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb::pregel {

typedef uint16_t PregelShard;
const PregelShard InvalidPregelShard = -1;

struct PregelID {
  std::string key;    // std::string 24
  PregelShard shard;  // uint16_t

  PregelID() : shard(InvalidPregelShard) {}
  PregelID(PregelShard s, std::string k) : key(std::move(k)), shard(s) {}

  bool operator==(const PregelID& rhs) const {
    return shard == rhs.shard && key == rhs.key;
  }

  bool operator!=(const PregelID& rhs) const {
    return shard != rhs.shard || key != rhs.key;
  }

  bool operator<(const PregelID& rhs) const {
    return shard < rhs.shard || (shard == rhs.shard && key < rhs.key);
  }

  [[nodiscard]] bool isValid() const {
    return shard != InvalidPregelShard && !key.empty();
  }
};

}  // namespace arangodb::pregel
namespace std {
template<>
struct hash<arangodb::pregel::PregelID> {
  std::size_t operator()(const arangodb::pregel::PregelID& k) const noexcept {
    using std::hash;
    using std::size_t;
    using std::string;

    // Compute individual hash values for first,
    // second and third and combine them using XOR
    // and bit shifting:
    size_t h1 = std::hash<std::string>()(k.key);
    size_t h2 = std::hash<size_t>()(k.shard);
    return h2 ^ (h1 << 1);
  }
};
}  // namespace std
