////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/GraphStore/GraphSerdeConfigBuilderCluster.h"

#include "Containers/Enumerate.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterFeature.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::pregel {
GraphSerdeConfigBuilderCluster::GraphSerdeConfigBuilderCluster(
    TRI_vocbase_t& vocbase, GraphByCollections const& graphByCollections)
    : vocbase(vocbase),
      clusterInfo(vocbase.server().getFeature<ClusterFeature>().clusterInfo()),
      graphByCollections(graphByCollections) {}

[[nodiscard]] auto
GraphSerdeConfigBuilderCluster::edgeCollectionRestrictionsByShard() const
    -> ShardMap {
  std::unordered_map<std::string, std::vector<std::string>>
      edgeCollectionRestrictionsPerShard;

  for (auto const& [vertexCollection, edgeCollections] :
       graphByCollections.edgeCollectionRestrictions) {
    for (auto const& shardId : getShardIds(vertexCollection)) {
      // intentionally create key in map
      auto& restrictions = edgeCollectionRestrictionsPerShard[shardId];
      for (auto const& edgeCollection : edgeCollections) {
        for (auto const& edgeShardId : getShardIds(edgeCollection)) {
          restrictions.push_back(edgeShardId);
        }
      }
    }
  }
  return edgeCollectionRestrictionsPerShard;
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::checkVertexCollections()
    const -> Result {
  for (std::string const& name : graphByCollections.vertexCollections) {
    try {
      auto coll = clusterInfo.getCollection(vocbase.name(), name);

      if (coll->system()) {
        return Result{TRI_ERROR_BAD_PARAMETER,
                      "Cannot use pregel on system collection"};
      }

      if (coll->deleted()) {
        return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name};
      }
    } catch (...) {
      return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name};
    }
  }
  return Result{};
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::checkEdgeCollections() const
    -> Result {
  for (std::string const& name : graphByCollections.edgeCollections) {
    try {
      auto coll = clusterInfo.getCollection(vocbase.name(), name);

      if (coll->system()) {
        return Result{TRI_ERROR_BAD_PARAMETER,
                      "Cannot use pregel on system collection"};
      }

      if (!coll->isSmart()) {
        std::vector<std::string> eKeys = coll->shardKeys();

        if (eKeys.size() != 1 ||
            eKeys[0] != graphByCollections.shardKeyAttribute) {
          return Result{
              TRI_ERROR_BAD_PARAMETER,
              "Edge collection needs to be sharded "
              "by shardKeyAttribute parameter ('" +
                  graphByCollections.shardKeyAttribute +
                  "'), or use SmartGraphs. The current shardKey is: " +
                  (eKeys.empty() ? "undefined" : "'" + eKeys[0] + "'")

          };
        }
      }

      if (coll->deleted()) {
        Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name};
      }
    } catch (...) {
      return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name};
    }
  }
  return Result{};
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::loadableVertexShards() const
    -> LoadableVertexShards {
  auto result = LoadableVertexShards{};

  std::vector<std::vector<ShardID>> vertexShardTable;
  std::vector<std::vector<ShardID>> edgeShardTable;

  for (auto const& vertexCollection : graphByCollections.vertexCollections) {
    vertexShardTable.emplace_back(getShardIds(vertexCollection));
  }
  for (auto const& edgeCollection : graphByCollections.edgeCollections) {
    edgeShardTable.emplace_back(getShardIds(edgeCollection));
  }

  for (auto collIdx = size_t{0}; collIdx < vertexShardTable.size(); ++collIdx) {
    for (auto shardIdx = size_t{0}; shardIdx < vertexShardTable[collIdx].size();
         ++shardIdx) {
      auto loadableVertexShard = LoadableVertexShard{
          .pregelShard = PregelShard(result.loadableVertexShards.size()),
          .vertexShard = vertexShardTable[collIdx][shardIdx],
          .collectionName = graphByCollections.vertexCollections[collIdx],
          .edgeShards = {}};

      for (auto edgeCollIdx = size_t{0}; edgeCollIdx < edgeShardTable.size();
           ++edgeCollIdx) {
        if (not graphByCollections.isRestricted(
                graphByCollections.vertexCollections.at(collIdx),
                graphByCollections.edgeCollections.at(edgeCollIdx))) {
          loadableVertexShard.edgeShards.emplace_back(
              edgeShardTable[edgeCollIdx][shardIdx]);
        }
      }
      result.loadableVertexShards.emplace_back(loadableVertexShard);
    }
  }
  return result;
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::responsibleServerMap(
    LoadableVertexShards const& loadableVertexShards) const
    -> ResponsibleServerMap {
  auto result = ResponsibleServerMap{};

  for (auto const& [idx, shard] :
       enumerate(loadableVertexShards.loadableVertexShards)) {
    std::shared_ptr<std::vector<ServerID> const> servers =
        clusterInfo.getResponsibleServer(shard.vertexShard);
    ADB_PROD_ASSERT(servers->size() > 0);
    if (servers->size() > 0) {
      result.responsibleServerMap.push_back(servers->at(0));
    }
  }
  return result;
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::resolveCollectionNameToIds(
    CollectionName collectionName) const -> std::vector<DataSourceId> {
  auto logicalCollection =
      clusterInfo.getCollection(vocbase.name(), collectionName);

  if (logicalCollection->isSmart()) {
    auto collectionIds = std::vector<DataSourceId>{};
    for (auto&& l : logicalCollection->realNamesForRead()) {
      auto lc2 = clusterInfo.getCollection(vocbase.name(), l);
      collectionIds.push_back(lc2->id());
    }
    return collectionIds;
  } else {
    return {logicalCollection->id()};
  }
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::getShardIds(
    ShardID collection) const -> std::vector<ShardID> {
  std::vector<ShardID> result;

  auto collectionIds = resolveCollectionNameToIds(collection);
  for (auto&& collectionId : collectionIds) {
    auto shardIds = clusterInfo.getShardList(std::to_string(collectionId.id()));
    for (auto const& it : *shardIds) {
      result.emplace_back(it);
    }
  }
  return result;
}

}  // namespace arangodb::pregel
