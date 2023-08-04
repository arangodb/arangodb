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

#pragma once

#include "Cluster/ClusterTypes.h"
#include <vector>
#include <memory>

struct TRI_vocbase_t;

namespace arangodb {

template<typename T>
class ResultT;

struct AgencyIsBuildingFlags;
struct CreateCollectionBody;
struct PlanCollectionEntry;
struct PlanCollectionEntryReplication2;
class LogicalCollection;
class ClusterInfo;
struct IShardDistributionFactory;

namespace replication2 {
struct CollectionGroupUpdates;
}

struct ClusterCollectionMethods {
  // static only class, never initialize
  ClusterCollectionMethods() = delete;
  ~ClusterCollectionMethods() = delete;

  /// @brief Create many new collections on coordinator from a vector of
  /// planCollections
  /// @param vocbase the actual database
  /// @param parametersOfCollections vector of parameters of collections to be
  /// created
  /// @param ignoreDistributeShardsLikeErrors
  /// @param waitForSyncReplication
  /// @param enforceReplicationFactor
  /// @param isNewDatabase

  [[nodiscard]] static auto createCollectionsOnCoordinator(
      TRI_vocbase_t& vocbase,
      std::vector<CreateCollectionBody> parametersOfCollections,
      bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
      bool enforceReplicationFactor, bool isNewDatabase)
      -> arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>>;

  [[nodiscard]] static auto toPlanEntry(
      CreateCollectionBody col, std::vector<ShardID> shardNames,
      std::shared_ptr<IShardDistributionFactory> distributeType,
      AgencyIsBuildingFlags isBuildingFlags) -> PlanCollectionEntry;

  [[nodiscard]] static auto toPlanEntryReplication2(
      CreateCollectionBody col, std::vector<ShardID> shardNames,
      std::shared_ptr<IShardDistributionFactory> distributeType,
      AgencyIsBuildingFlags isBuildingFlags) -> PlanCollectionEntryReplication2;

  [[nodiscard]] static auto generateShardNames(ClusterInfo& ci,
                                               uint64_t numberOfShards)
      -> std::vector<ShardID>;

  [[nodiscard]] static auto selectDistributeType(
      ClusterInfo& ci, std::string_view databaseName,
      CreateCollectionBody const& col, bool enforceReplicationFactor,
      std::unordered_map<std::string,
                         std::shared_ptr<IShardDistributionFactory>>&
          allUsedDistrbitions) -> std::shared_ptr<IShardDistributionFactory>;

  [[nodiscard]] static auto prepareCollectionGroups(
      ClusterInfo& ci, std::string_view databaseName,
      std::vector<CreateCollectionBody>& collections)
      -> ResultT<replication2::CollectionGroupUpdates>;
};

}  // namespace arangodb
