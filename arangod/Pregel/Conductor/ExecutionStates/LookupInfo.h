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

#include <string>

#include "Cluster/ClusterTypes.h"

namespace arangodb::pregel::conductor {

struct LookupInfo {
  virtual ~LookupInfo() = default;

  using CollectionPlanIDMapping = std::unordered_map<CollectionID, std::string>;
  using ServerMapping =
      std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>;
  using ShardsMapping = std::vector<ShardID>;

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
  [[nodiscard]] auto const& getServerMapVertices() const {
    return _serverMapVertices;
  }

  // edges related methods:
  [[nodiscard]] auto const& getServerMapEdges() const {
    return _serverMapEdges;
  }

  // both combined (vertices, edges) related methods:
  [[nodiscard]] auto&& getAllShards() const {
    ShardsMapping allShards;
    allShards.reserve(getAllShardsVertices().size() +
                      getAllShardsEdges().size());
    allShards.insert(allShards.end(), getAllShardsVertices().begin(),
                     getAllShardsVertices().end());
    allShards.insert(allShards.end(), getAllShardsEdges().begin(),
                     getAllShardsEdges().end());
    return std::move(allShards);
  }
  [[nodiscard]] auto&& getCollectionPlanIdMapAll() const {
    CollectionPlanIDMapping allMapping = getCollectionPlanIdMapVertices();
    allMapping.insert(getCollectionPlanIdMapEdges().begin(),
                      getCollectionPlanIdMapEdges().end());
    return std::move(allMapping);
  }

 protected:
  CollectionPlanIDMapping _collectionPlanIdMapVertices;
  ServerMapping _serverMapVertices;
  ShardsMapping _allShardsVertices;

  CollectionPlanIDMapping _collectionPlanIdMapEdges;
  ServerMapping _serverMapEdges;
  ShardsMapping _allShardsEdges;
};

}  // namespace arangodb::pregel::conductor
