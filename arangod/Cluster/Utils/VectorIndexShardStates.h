////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <string_view>

#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ShardID.h"
#include "Containers/FlatHashMap.h"

namespace arangodb {

class CollectionInfoCurrent;
using ShardMap = containers::FlatHashMap<ShardID, std::vector<ServerID>>;

struct VectorIndexShardState {
  std::string trainingState;
  std::string error;
};

/// Gathers the per-shard training state and error info for a vector index
/// from CollectionInfoCurrent.
containers::FlatHashMap<ShardID, VectorIndexShardState>
getVectorIndexShardStates(CollectionInfoCurrent const& collCurrent,
                          ShardMap const& shardIds,
                          std::string_view bareIndexId);

}  // namespace arangodb
