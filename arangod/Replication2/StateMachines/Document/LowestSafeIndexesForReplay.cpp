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

namespace arangodb {
namespace replication2 {
namespace replicated_state {
namespace document {

bool LowestSafeIndexesForReplay::isSafeForReplay(ShardID shardId,
                                                 LogIndex logIndex) {
  auto it = map.find(shardId);
  if (it == map.end()) {
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
  map = std::move(newMap);
}

LowestSafeIndexesForReplay::LowestSafeIndexesForReplay(
    DocumentStateMetadata const& metadata) {
  setFromMetadata(metadata);
}
}  // namespace document
}  // namespace replicated_state
}  // namespace replication2
}  // namespace arangodb