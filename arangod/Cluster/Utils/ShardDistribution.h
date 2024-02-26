////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/IShardDistributionFactory.h"

#include <vector>

namespace arangodb {
struct ShardID;

struct ShardDistribution {
  ShardDistribution(std::vector<ShardID> shardNames,
                    std::shared_ptr<IShardDistributionFactory> distributeType);

  /**
   * @brief Get a full map of shard to Server Distribution, using the given
   * Shards list and the current shardToServerMapping.
   * @param index
   */
  auto getDistributionForShards() const -> PlanShardToServerMapping;

 private:
  std::vector<ShardID> _shardNames;
  std::shared_ptr<IShardDistributionFactory> _distributeType;
};
}  // namespace arangodb
