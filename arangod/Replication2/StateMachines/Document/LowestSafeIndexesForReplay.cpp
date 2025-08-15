////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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

#include "LowestSafeIndexesForReplay.h"

#include "Cluster/Utils/ShardID.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"

namespace arangodb::replication2::replicated_state::document {

bool LowestSafeIndexesForReplay::isSafeForReplay(ShardID shardId,
                                                 LogIndex logIndex) {
  auto it = _map.find(shardId);
  if (it == _map.end()) {
    return true;
  } else {
    return logIndex >= it->second;
  }
}

void LowestSafeIndexesForReplay::setFromMetadata(
    DocumentStateMetadata const& metadata) {
  auto newMap = std::map<ShardID, LogIndex>{};
  std::transform(metadata.lowestSafeIndexesForReplay.begin(),
                 metadata.lowestSafeIndexesForReplay.end(),
                 std::inserter(newMap, newMap.end()), [](auto const& kv) {
                   return std::pair{ShardID(kv.first), kv.second};
                 });
  _map = std::move(newMap);
}

LowestSafeIndexesForReplay::LowestSafeIndexesForReplay(
    DocumentStateMetadata const& metadata) {
  setFromMetadata(metadata);
}

auto LowestSafeIndexesForReplay::getMap() const noexcept
    -> std::map<ShardID, LogIndex> const& {
  return _map;
}

}  // namespace arangodb::replication2::replicated_state::document
