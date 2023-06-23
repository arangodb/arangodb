////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "ShardDistribution.h"
#include "PlanShardToServerMappping.h"

using namespace arangodb;

ShardDistribution::ShardDistribution(
    std::vector<ShardID> shardNames,
    std::shared_ptr<IShardDistributionFactory> distributeType)
    : _shardNames{std::move(shardNames)},
      _distributeType{std::move(distributeType)} {}

/**
 * @brief Get a full map of shard to Server Distribution, using the given Shards
 * list and the current shardToServerMapping.
 * @param shardIds list of shardIds, expected to be in correct alphabetical
 * order.
 */
auto ShardDistribution::getDistributionForShards() const
    -> PlanShardToServerMapping {
  PlanShardToServerMapping res;
  for (size_t i = 0; i < _shardNames.size(); ++i) {
    res.shards.emplace(_shardNames[i],
                       _distributeType->getServersForShardIndex(i));
  }
  return res;
}
