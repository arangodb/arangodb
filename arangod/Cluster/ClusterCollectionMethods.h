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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace arangodb {

template<typename T>
class ResultT;

struct AgencyIsBuildingFlags;
struct CollectionDescriptor;
struct CollectionCreateOptions;
struct Database;
struct PlanCollectionEntry;
struct PlanCollectionEntryReplication2;
class LogicalCollection;
class ClusterInfo;
struct IShardDistributionFactory;
struct ShardID;

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
      Database& vocbase,
      std::vector<CollectionDescriptor> parametersOfCollections,
      bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
      bool enforceReplicationFactor, bool isNewDatabase,
      CollectionCreateOptions const& options)
      -> arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>>;

  [[nodiscard]] static auto toPlanEntry(
      CollectionDescriptor col, std::vector<ShardID> shardNames,
      std::shared_ptr<IShardDistributionFactory> distributeType,
      AgencyIsBuildingFlags isBuildingFlags) -> PlanCollectionEntry;

  [[nodiscard]] static auto toPlanEntryReplication2(
      CollectionDescriptor col, std::vector<ShardID> shardNames,
      std::shared_ptr<IShardDistributionFactory> distributeType,
      AgencyIsBuildingFlags isBuildingFlags) -> PlanCollectionEntryReplication2;

  [[nodiscard]] static auto generateShardNames(ClusterInfo& ci,
                                               uint64_t numberOfShards)
      -> std::vector<ShardID>;

  [[nodiscard]] static auto selectDistributeType(
      ClusterInfo& ci, std::string_view databaseName,
      CollectionDescriptor const& col, bool enforceReplicationFactor,
      std::unordered_map<std::string,
                         std::shared_ptr<IShardDistributionFactory>>&
          allUsedDistrbitions,
      CollectionCreateOptions const& options)
      -> std::shared_ptr<IShardDistributionFactory>;

  [[nodiscard]] static auto prepareCollectionGroups(
      ClusterInfo& ci, std::string_view databaseName,
      std::vector<CollectionDescriptor>& collections)
      -> ResultT<replication2::CollectionGroupUpdates>;

  [[nodiscard]] static auto updateCollectionProperties(
      Database& database, LogicalCollection const& col) -> Result;
};

}  // namespace arangodb
