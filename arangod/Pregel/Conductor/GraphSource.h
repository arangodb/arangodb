////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Cluster/ClusterTypes.h"
#include "Pregel/Collections/Collections.h"
#include "Pregel/PregelOptions.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel::conductor {

using VertexShardID = ShardID;
using EdgeShardID = ShardID;

struct GraphCollections {
  collections::Collections vertexCollections;
  collections::Collections edgeCollections;
  static auto from(GraphCollectionNames const& names, TRI_vocbase_t& vocbase)
      -> ResultT<GraphCollections>;
  auto convertToShards(EdgeCollectionRestrictions const& restrictions) const
      -> std::unordered_map<VertexShardID, std::vector<EdgeShardID>>;
  auto all() const -> collections::Collections;
};

struct PregelGraphSource {
  std::unordered_map<VertexShardID, std::vector<EdgeShardID>>
      edgeCollectionRestrictions;
  std::unordered_map<ServerID,
                     std::map<CollectionID, std::vector<VertexShardID>>>
      vertexShards;
  std::unordered_map<ServerID, std::map<CollectionID, std::vector<EdgeShardID>>>
      edgeShards;
  std::vector<ShardID> allShards;
  std::unordered_map<CollectionID, std::string> planIds;
};

struct GraphSourceSettings {
  GraphDataSource graphDataSource;
  std::string shardKeyAttribute;
  bool storeResults;

  auto getSource(TRI_vocbase_t& vocbase) -> ResultT<PregelGraphSource>;

 private:
  auto isShardingCorrect(
      std::shared_ptr<collections::Collection> const& collection) -> Result;
};

}  // namespace arangodb::pregel::conductor
