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

#include "ClusteringProperties.h"

#include "Basics/debugging.h"
#include "Basics/ResultT.h"
#include "VocBase/Properties/CollectionProperties.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

using namespace arangodb;

[[nodiscard]] arangodb::Result
ClusteringProperties::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  // DistributeShardsLike has the strongest binding. We have to handle this
  // first
  if (!config.defaultDistributeShardsLike.empty()) {
    if (!distributeShardsLike.has_value()) {
      distributeShardsLike = config.defaultDistributeShardsLike;
    } else {
      // NOTE: For the time being only oneShardDBs have a default
      // distributeShardsLikeValue. So this error message is good enough. If
      // this should ever change we need to adapt the message here. NOTE: The
      // assertion is a reminder to update the following error message.
      TRI_ASSERT(config.isOneShardDB);
      return {TRI_ERROR_BAD_PARAMETER,
              "Collection in a 'oneShardDatabase' cannot define "
              "distributeShardsLike"};
    }
  }
  if (distributeShardsLike.has_value()) {
    auto groupInfo =
        config.getCollectionGroupSharding(distributeShardsLike.value());
    if (!groupInfo.ok()) {
      return groupInfo.result();
    }
    // Copy the relevant attributes

    // We cannot have a cid set yet, this can only be set if we read from
    // agency which is not yet implemented using this path.
    TRI_ASSERT(!distributeShardsLikeCid.has_value());
    // Copy the CID value
    distributeShardsLikeCid = std::to_string(groupInfo->id.id());
    TRI_ASSERT(groupInfo->numberOfShards.has_value());
    if (numberOfShards.has_value()) {
      if (numberOfShards != groupInfo->numberOfShards) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different numberOfShards (" +
                    std::to_string(numberOfShards.value()) +
                    "), than the leading collection (" +
                    std::to_string(groupInfo->numberOfShards.value()) + ")"};
      }
    } else {
      numberOfShards = groupInfo->numberOfShards;
    }
    TRI_ASSERT(groupInfo->writeConcern.has_value());
    if (writeConcern.has_value()) {
      if (writeConcern != groupInfo->writeConcern) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different writeConcern (" +
                    std::to_string(writeConcern.value()) +
                    "), than the leading collection (" +
                    std::to_string(groupInfo->writeConcern.value()) + ")"};
      }
    } else {
      writeConcern = groupInfo->writeConcern;
    }
    TRI_ASSERT(groupInfo->replicationFactor.has_value());
    if (replicationFactor.has_value()) {
      if (replicationFactor != groupInfo->replicationFactor) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different replicationFactor (" +
                    std::to_string(replicationFactor.value()) +
                    "), than the leading collection (" +
                    std::to_string(groupInfo->replicationFactor.value()) + ")"};
      }
    } else {
      replicationFactor = groupInfo->replicationFactor;
    }
    TRI_ASSERT(groupInfo->shardingStrategy.has_value());
    if (shardingStrategy.has_value()) {
      // TODO: Externalize, and add EE exceptions
      if (shardingStrategy != groupInfo->shardingStrategy) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different sharding strategy (" +
                    shardingStrategy.value() +
                    "), than the leading collection (" +
                    groupInfo->shardingStrategy.value() + ")"};
      }
    } else {
      shardingStrategy = groupInfo->shardingStrategy;
    }
    if (shardKeys.size() != groupInfo->shardKeys.size()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Cannot have a different number of shardKeys (" +
                  std::to_string(shardKeys.size()) +
                  "), than the leading collection (" +
                  std::to_string(groupInfo->shardKeys.size()) + ")."};
    }
  } else {
    // Apply database defaults
    ClusteringMutableProperties::applyDatabaseDefaults(config);
    ClusteringConstantProperties::applyDatabaseDefaults(config);
  }

  // Validate that necessary values are now filled
  TRI_ASSERT(replicationFactor.has_value());
  TRI_ASSERT(writeConcern.has_value());
  TRI_ASSERT(numberOfShards.has_value());

  // Make sure we do not violate conditions anymore
  auto validationResult =
      ClusteringMutableProperties::validateDatabaseConfiguration(config);
  if (!validationResult.ok()) {
    return validationResult;
  }
  validationResult =
      ClusteringConstantProperties::validateDatabaseConfiguration(config);
  if (!validationResult.ok()) {
    return validationResult;
  }

  if (isSatellite()) {
    if (numberOfShards.value() != 1) {
      return {TRI_ERROR_BAD_PARAMETER,
              "A satellite collection can only have a single shard"};
    }
  }
  return TRI_ERROR_NO_ERROR;
}
