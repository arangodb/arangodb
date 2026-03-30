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

#include "VectorIndexShardStates.h"

#include "Basics/StaticStrings.h"
#include "Cluster/CollectionInfoCurrent.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {

containers::FlatHashMap<ShardID, VectorIndexShardState>
getVectorIndexShardStates(CollectionInfoCurrent const& collCurrent,
                          ShardMap const& shardIds,
                          std::string_view bareIndexId) {
  containers::FlatHashMap<ShardID, VectorIndexShardState> result;
  result.reserve(shardIds.size());

  for (auto const& [shardId, servers] : shardIds) {
    VectorIndexShardState state{"unknown", ""};

    VPackSlice const shardIndexes = collCurrent.getIndexes(shardId);
    if (shardIndexes.isArray()) {
      for (auto const& idx : VPackArrayIterator(shardIndexes)) {
        if (!idx.isObject()) {
          continue;
        }
        auto const idSlice = idx.get("id");
        if (!idSlice.isString()) {
          continue;
        }
        if (idSlice.stringView() == bareIndexId) {
          if (auto const errSlice = idx.get(StaticStrings::Error);
              errSlice.isBoolean() && errSlice.getBoolean()) {
            if (auto const msgSlice = idx.get(StaticStrings::ErrorMessage);
                msgSlice.isString()) {
              state.error = msgSlice.copyString();
            }
          }
          if (auto const tsSlice = idx.get(StaticStrings::IndexTrainingState);
              tsSlice.isString()) {
            state.trainingState = tsSlice.copyString();
          }
          break;
        }
      }
    }

    result.emplace(shardId, std::move(state));
  }

  return result;
}

}  // namespace arangodb
