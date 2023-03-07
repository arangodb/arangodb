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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Pregel/Conductor/ExecutionStates/CollectionLookup.h"
#include "VocBase/LogicalCollection.h"

struct TRI_vocbase_t;

namespace arangodb::pregel::conductor {

struct DatabaseCollectionLookup : CollectionLookup {
  DatabaseCollectionLookup(
      TRI_vocbase_t& vocbase,
      std::vector<CollectionID> const& verticesCollectionIDs,
      std::vector<CollectionID> const& edgesCollectionIDs) {
    ServerState* ss = ServerState::instance();
    auto createMappingHelper =
        [&](std::vector<CollectionID> const& collectionIDs,
            CollectionPlanIDMapping& collectionPlanIdMap,
            ServerMapping& serverMapping, ShardsMapping& shardsMapping) {
          for (CollectionID const& collectionID : collectionIDs) {
            if (!ss->isRunningInCluster()) {  // single server mode
              auto lc = vocbase.lookupCollection(collectionID);

              if (lc == nullptr || lc->deleted()) {
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, collectionID);
              }

              collectionPlanIdMap.try_emplace(
                  collectionID, std::to_string(lc->planId().id()));
              shardsMapping.push_back(collectionID);
              serverMapping[ss->getId()][collectionID].push_back(collectionID);
            } else if (ss->isCoordinator()) {  // we are in the cluster

              ClusterInfo& ci =
                  vocbase.server().getFeature<ClusterFeature>().clusterInfo();
              std::shared_ptr<LogicalCollection> lc =
                  ci.getCollection(vocbase.name(), collectionID);
              if (lc->deleted()) {
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, collectionID);
              }
              collectionPlanIdMap.try_emplace(
                  collectionID, std::to_string(lc->planId().id()));

              std::shared_ptr<std::vector<ShardID>> shardIDs =
                  ci.getShardList(std::to_string(lc->id().id()));
              shardsMapping.insert(shardsMapping.end(), shardIDs->begin(),
                                   shardIDs->end());

              for (auto const& shard : *shardIDs) {
                std::shared_ptr<std::vector<ServerID> const> servers =
                    ci.getResponsibleServer(shard);
                if (servers->size() > 0) {
                  serverMapping[(*servers)[0]][lc->name()].push_back(shard);
                }
              }
            } else {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
            }
          }
        };

    createMappingHelper(verticesCollectionIDs, _collectionPlanIdMapVertices,
                        _serverMapVertices, _allShardsVertices);
    createMappingHelper(edgesCollectionIDs, _collectionPlanIdMapEdges,
                        _serverMapEdges, _allShardsEdges);
  }

 private:
  // vertices related methods:
  [[nodiscard]] auto const& getCollectionPlanIdMapVertices() const {
    return _collectionPlanIdMapVertices;
  }
  [[nodiscard]] auto const& getAllShardsVertices() const {
    return _allShardsVertices;
  }

  // edges related methods:
  [[nodiscard]] auto const& getCollectionPlanIdMapEdges() const {
    return _collectionPlanIdMapEdges;
  }
  [[nodiscard]] auto const& getAllShardsEdges() const {
    return _allShardsEdges;
  }

 public:
  // vertices related methods:
  [[nodiscard]] auto getServerMapVertices() const -> ServerMapping override {
    return _serverMapVertices;
  }

  // edges related methods:
  [[nodiscard]] auto getServerMapEdges() const -> ServerMapping override {
    return _serverMapEdges;
  }

  // both combined (vertices, edges) related methods:
  [[nodiscard]] auto getAllShards() const -> ShardsMapping override {
    ShardsMapping allShards;
    allShards.reserve(getAllShardsVertices().size() +
                      getAllShardsEdges().size());
    allShards.insert(allShards.end(), getAllShardsVertices().begin(),
                     getAllShardsVertices().end());
    allShards.insert(allShards.end(), getAllShardsEdges().begin(),
                     getAllShardsEdges().end());
    return allShards;
  }

  [[nodiscard]] auto getCollectionPlanIdMapAll() const
      -> CollectionPlanIDMapping override {
    CollectionPlanIDMapping allMapping = getCollectionPlanIdMapVertices();
    allMapping.insert(getCollectionPlanIdMapEdges().begin(),
                      getCollectionPlanIdMapEdges().end());
    return allMapping;
  }

 private:
  CollectionPlanIDMapping _collectionPlanIdMapVertices;
  ServerMapping _serverMapVertices;
  ShardsMapping _allShardsVertices;

  CollectionPlanIDMapping _collectionPlanIdMapEdges;
  ServerMapping _serverMapEdges;
  ShardsMapping _allShardsEdges;
};

}  // namespace arangodb::pregel::conductor
