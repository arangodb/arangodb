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

#include "PregelShard.h"

namespace arangodb::pregel {

struct PregelID {
  PregelShard shard;
  std::string key;

  PregelID() = default;
  PregelID(PregelShard s, std::string k) : shard(s), key(std::move(k)) {}

  [[nodiscard]] bool operator==(const PregelID& rhs) const = default;

  bool operator<(const PregelID& rhs) const {
    return shard < rhs.shard || (shard == rhs.shard && key < rhs.key);
  }

  [[nodiscard]] bool isValid() const { return shard.isValid() && !key.empty(); }
};

}  // namespace arangodb::pregel

namespace std {
template<>
struct hash<arangodb::pregel::PregelID> {
  std::size_t operator()(const arangodb::pregel::PregelID& k) const noexcept {
    size_t h1 = std::hash<arangodb::pregel::PregelShard>()(k.shard);
    size_t h2 = std::hash<std::string>()(k.key);
    return h2 ^ (h1 << 1);
  }
};
}  // namespace std
