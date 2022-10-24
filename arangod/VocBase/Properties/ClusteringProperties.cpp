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
