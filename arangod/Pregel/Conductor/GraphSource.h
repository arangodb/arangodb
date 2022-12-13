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
#include "Pregel/Collections/Graph/Source.h"
#include "Pregel/Collections/Collection.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel::conductor {

struct PregelGraphSource {
  std::unordered_map<collections::graph::VertexShardID,
                     std::vector<collections::graph::EdgeShardID>>
      edgeCollectionRestrictions;
  std::unordered_map<
      ServerID,
      std::map<CollectionID, std::vector<collections::graph::VertexShardID>>>
      vertexShards;
  std::unordered_map<
      ServerID,
      std::map<CollectionID, std::vector<collections::graph::EdgeShardID>>>
      edgeShards;
  std::vector<ShardID> allShards;
  std::unordered_map<CollectionID, std::string> planIds;
};
template<typename Inspector>
auto inspect(Inspector& f, PregelGraphSource& x) {
  return f.object(x).fields(
      f.field("edgeCollectionRestrictions", x.edgeCollectionRestrictions),
      f.field("vertexShards", x.vertexShards),
      f.field("edgeShards", x.edgeShards), f.field("allShards", x.allShards),
      f.field("planIds", x.planIds));
}

struct GraphSettings {
  collections::graph::GraphSource graphSource;
  std::string shardKeyAttribute;
  bool storeResults;

  auto getSource(TRI_vocbase_t& vocbase) -> ResultT<PregelGraphSource>;

 private:
  auto isShardingCorrect(
      std::shared_ptr<collections::Collection> const& collection) -> Result;
};
template<typename Inspector>
auto inspect(Inspector& f, GraphSettings& x) {
  return f.object(x).fields(f.field("graphDataSource", x.graphSource),
                            f.field("shardKeyAttribute", x.shardKeyAttribute),
                            f.field("storeResults", x.storeResults));
}

}  // namespace arangodb::pregel::conductor
