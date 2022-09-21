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

#include "ClusterCollectionMethods.h"

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/Utils/IShardDistributionFactory.h"
#include "Cluster/Utils/PlanCollectionEntry.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Properties/PlanCollection.h"
#include "VocBase/vocbase.h"

// TODO: TEMPORARY!
#include "Cluster/ClusterMethods.h"
#include "Cluster/Utils/SatelliteDistribution.h"
#include "Cluster/Utils/EvenDistribution.h"
#include "Cluster/Utils/DistributeShardsLike.h"

using namespace arangodb;

namespace {
ResultT<std::vector<std::shared_ptr<LogicalCollection>>>
persistCollectionsInAgency(ClusterFeature& feature,
                           std::vector<arangodb::PlanCollection>& collections,
                           bool ignoreDistributeShardsLikeErrors,
                           bool waitForSyncReplication,
                           bool enforceReplicationFactor, bool isNewDatabase) {

}
}

[[nodiscard]] auto ClusterCollectionMethods::toPlanEntry(PlanCollection col, std::vector<ShardID> shardNames, std::shared_ptr<IShardDistributionFactory> distributeType) -> PlanCollectionEntry {
  return {std::move(col), ShardDistribution{std::move(shardNames), std::move(distributeType)}};
}

[[nodiscard]] auto ClusterCollectionMethods::generateShardNames(ClusterInfo& ci, uint64_t numberOfShards) -> std::vector<ShardID> {
  if (numberOfShards == 0) {
    // If we do not have shards, we do only need an empty vector and no ids.
    return {};
  }
  // Reserve ourselves the next numberOfShards many ids to use them for shardNames
  uint64_t const id = ci.uniqid(numberOfShards);
  std::vector<ShardID> shardNames;
  shardNames.reserve(numberOfShards);
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    shardNames.emplace_back("s" + basics::StringUtils::itoa(id + i));
  }
  return shardNames;
}

[[nodiscard]] auto ClusterCollectionMethods::selectDistributeType(ClusterInfo& ci, PlanCollection const& col, std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>& allUsedDistrbitions) -> std::shared_ptr<IShardDistributionFactory> {
  if (col.constantProperties.distributeShardsLike.has_value()) {
    auto distLike = col.constantProperties.distributeShardsLike.value();
    // Empty value has to be rejected by invariants beforehand, assert here just in case.
    TRI_ASSERT(!distLike.empty());
    if (allUsedDistrbitions.contains(distLike)) {
      // We are already set, use the other one.
      return allUsedDistrbitions.at(distLike);
    }
    // Follow the given distribution
    auto distribution = std::make_shared<DistributeShardsLike>();
    // Add the Leader to the distribution List
    allUsedDistrbitions.emplace(distLike, distribution);
    return distribution;
  } else if (col.mutableProperties.replicationFactor == 0) {
    // We are a Satellite collection, use Satellite sharding
    auto distribution = std::make_shared<SatelliteDistribution>();
    allUsedDistrbitions.emplace(col.mutableProperties.name, distribution);
    return distribution;
  } else {
    // Just distribute evenly
    auto distribution = std::make_shared<EvenDistribution>(col.constantProperties.numberOfShards, col.mutableProperties.replicationFactor, col.options.avoidServers);
    allUsedDistrbitions.emplace(col.mutableProperties.name, distribution);
    return distribution;
  }
}


[[nodiscard]] arangodb::ResultT<std::vector<std::shared_ptr<LogicalCollection>>>
ClusterCollectionMethods::createCollectionsOnCoordinator(
    TRI_vocbase_t& vocbase, std::vector<PlanCollection> collections,
    bool ignoreDistributeShardsLikeErrors, bool waitForSyncReplication,
    bool enforceReplicationFactor, bool isNewDatabase) {

  TRI_ASSERT(!collections.empty());
  if (collections.empty()) {
    return Result{
        TRI_ERROR_INTERNAL,
        "Trying to create an empty list of collections on coordinator."};
  }

  auto& feature = vocbase.server().getFeature<ClusterFeature>();
  // List of all sharding prototypes.
  // We retain a reference here ourselfs in case we need to retry due to server
  // failure, this way we can just to create the shards on other servers.
  std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>> shardDistributionList;

  std::vector<arangodb::PlanCollectionEntry> collectionPlanEntries{};
  collectionPlanEntries.reserve(collections.size());

  /*
 NEED TO ACTIVATE THE FOLLOWING CODE

   if (warnAboutReplicationFactor) {
LOG_TOPIC("e16ec", WARN, Logger::CLUSTER)
   << "createCollectionCoordinator: replicationFactor is "
   << "too large for the number of DBservers";
}

   */


  for (auto& c : collections) {
    auto shards = generateShardNames(feature.clusterInfo(), c.constantProperties.numberOfShards);
    auto distributionType = selectDistributeType(feature.clusterInfo(), c, shardDistributionList);
    collectionPlanEntries.emplace_back(toPlanEntry(std::move(c), std::move(shards), distributionType));
  }
  // Protection, all entries have been moved
  collections.clear();

  while (true) {
    // Get list of available servers
    std::vector<ServerID> availableServers{};
    for (auto& [_, dist] : shardDistributionList) {
      // Select a distribution based on the available servers
      dist->shuffle(availableServers);
    }

  }


  /// Code from here is copy pasted from original create and
  /// has not been refactored yet.
  VPackBuilder builder =
      PlanCollection::toCreateCollectionProperties(collections);

  // TODO: Need to get rid of this collection. Distribute Shards like
  // is now denoted inside the PlanCollection
  std::shared_ptr<LogicalCollection> colToDistributeShardsLike;

  VPackSlice infoSlice = builder.slice();

  TRI_ASSERT(infoSlice.isArray());
  TRI_ASSERT(infoSlice.length() >= 1);
  TRI_ASSERT(infoSlice.length() == collections.size());

  std::vector<std::shared_ptr<LogicalCollection>> results;
  results.reserve(collections.size());

  try {
    results = ClusterMethods::createCollectionsOnCoordinator(
        vocbase, infoSlice, ignoreDistributeShardsLikeErrors,
        waitForSyncReplication, enforceReplicationFactor, isNewDatabase,
        colToDistributeShardsLike);

    if (collections.empty()) {
      for (auto const& info : collections) {
        events::CreateCollection(vocbase.name(), info.mutableProperties.name,
                                 TRI_ERROR_INTERNAL);
      }
      return Result(TRI_ERROR_INTERNAL, "createCollectionsOnCoordinator");
    }
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }
  return {std::move(results)};
}
