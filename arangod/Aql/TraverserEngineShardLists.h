////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/ClusterTypes.h"

#include <set>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace aql {
class GraphNode;
class QueryContext;

// @brief Struct to create the
// information required to build traverser engines
// on DB servers.
class TraverserEngineShardLists {
 public:
  TraverserEngineShardLists(GraphNode const*, ServerID const& server,
                            std::unordered_map<ShardID, ServerID> const& shardMapping,
                            QueryContext& query);

  ~TraverserEngineShardLists() = default;

  void serializeIntoBuilder(arangodb::velocypack::Builder& infoBuilder) const;

  bool hasShard() const { return _hasShard; }

  /// inaccessible edge and verte collection names
#ifdef USE_ENTERPRISE
  std::set<CollectionID> inaccessibleCollNames() const { return _inaccessible; }
#endif

 private:
  std::vector<ShardID> getAllLocalShards(std::unordered_map<ShardID, ServerID> const& shardMapping,
                                         ServerID const& server,
                                         std::shared_ptr<std::vector<std::string>> shardIds,
                                         bool allowReadFromFollower);

 private:
  // The graph node we need to serialize
  GraphNode const* _node;

  // Flag if we found any shard for the given server.
  // If not serializeToBuilder will be a noop
  bool _hasShard;

  // Mapping for edge collections to shardIds.
  // We have to retain the ordering of edge collections, all
  // vectors of these in one run need to have identical size.
  // This is because the conditions to query those edges have the
  // same ordering.
  std::vector<std::vector<ShardID>> _edgeCollections;

  // Mapping for vertexCollections to shardIds.
  std::unordered_map<std::string, std::vector<ShardID>> _vertexCollections;

#ifdef USE_ENTERPRISE
  std::set<CollectionID> _inaccessible;
#endif
};

}  // namespace aql
}  // namespace arangodb
