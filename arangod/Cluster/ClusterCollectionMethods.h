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
struct PlanCollection;
struct PlanCollectionEntry;
class LogicalCollection;
class ClusterInfo;
struct IShardDistributionFactory;

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

  [[nodiscard]] static auto
  createCollectionsOnCoordinator(
      TRI_vocbase_t& vocbase,
      std::vector<PlanCollection> parametersOfCollections,
      bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
      bool enforceReplicationFactor, bool isNewDatabase) -> arangodb::ResultT<
      std::vector<std::shared_ptr<LogicalCollection>>>;

  [[nodiscard]] static auto toPlanEntry(
      PlanCollection col, std::vector<ShardID> shardNames,
      std::shared_ptr<IShardDistributionFactory> distributeType,
      AgencyIsBuildingFlags isBuildingFlags) -> PlanCollectionEntry;

  [[nodiscard]] static auto generateShardNames(ClusterInfo& ci, uint64_t numberOfShards) -> std::vector<ShardID>;

  [[nodiscard]] static auto selectDistributeType(
      ClusterInfo& ci, std::string_view databaseName, PlanCollection const& col,
      std::unordered_map<std::string,
                         std::shared_ptr<IShardDistributionFactory>>&
          allUsedDistrbitions) -> std::shared_ptr<IShardDistributionFactory>;
};

}
