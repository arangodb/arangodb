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
    -> std::vector<LoadableVertexShard> {
  auto result = std::vector<LoadableVertexShard>{};

  auto edgeShardMap = std::unordered_map<CollectionName, CollectionShardMap>{};

  for (auto&& edgeCollection : graphByCollections.edgeCollections) {
    edgeShardMap.emplace(edgeCollection, getCollectionShardMap(edgeCollection));
  }

  for (auto&& vertexCollection : graphByCollections.vertexCollections) {
    auto shardmap = getCollectionShardMap(vertexCollection);
    for (auto shardIdx = size_t{0}; shardIdx < shardmap.content.at(0).size();
         ++shardIdx) {
      auto vertexShard = shardmap.at(shardIdx).at(0);

      auto responsibleServers = clusterInfo.getResponsibleServer(vertexShard);

      ADB_PROD_ASSERT(not responsibleServers->empty());

      auto loadableVertexShard =
          LoadableVertexShard{.pregelShard = PregelShard(result.size()),
                              .vertexShard = vertexShard,
                              .responsibleServer = responsibleServers->at(0),
                              .collectionName = vertexCollection,
                              .edgeShards = {}};

      for (auto&& edgeCollection : graphByCollections.edgeCollections) {
        if (not graphByCollections.isRestricted(vertexCollection,
                                                edgeCollection)) {
          for (auto shard : edgeShardMap.at(edgeCollection).at(shardIdx)) {
            loadableVertexShard.edgeShards.emplace_back(shard);
          }
        }
      }
      result.emplace_back(loadableVertexShard);
    }
  }
  return result;
}

[[nodiscard]] auto GraphSerdeConfigBuilderCluster::getCollectionShardMap(
    CollectionName collectionName) const -> CollectionShardMap {
  auto result = CollectionShardMap{};

  auto logicalCollection =
      clusterInfo.getCollection(vocbase.name(), collectionName);

  if (logicalCollection->isSmart()) {
    for (auto&& l : logicalCollection->realNamesForRead()) {
      auto lc2 = clusterInfo.getCollection(vocbase.name(), l);
      auto shardIDs = clusterInfo.getShardList(std::to_string(lc2->id().id()));
      result.content.push_back(*shardIDs);
    }
  } else {
    auto shardIDs =
        clusterInfo.getShardList(std::to_string(logicalCollection->id().id()));
    result.content.push_back(*shardIDs);
  }
  return result;
}

}  // namespace arangodb::pregel
