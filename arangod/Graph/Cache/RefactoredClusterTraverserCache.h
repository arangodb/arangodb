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

#ifndef ARANGOD_GRAPH_REFACTORED_CLUSTER_TRAVERSER_CACHE_H
#define ARANGOD_GRAPH_REFACTORED_CLUSTER_TRAVERSER_CACHE_H 1

#include "Aql/types.h"
#include "Cluster/ClusterInfo.h"
#include "Graph/ClusterGraphDatalake.h"
#include "Graph/Providers/TypeAliases.h"
#include "Graph/TraverserCache.h"

#include <velocypack/Buffer.h>
#include <velocypack/StringRef.h>

#include <map>

namespace arangodb {
struct ResourceMonitor;

namespace aql {
struct AqlValue;
class QueryContext;
}  // namespace aql

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
class HashedStringRef;
class StringRef;
}  // namespace velocypack

namespace graph {
struct BaseOptions;

class RefactoredClusterTraverserCache {
 public:
  RefactoredClusterTraverserCache(std::unordered_map<ServerID, aql::EngineId> const* engines,
                                  ResourceMonitor& resourceMonitor);

  ~RefactoredClusterTraverserCache() = default;

  void clear();

  [[nodiscard]] std::unordered_map<ServerID, aql::EngineId> const* engines() const {
    return _engines;
  }

  arangodb::graph::ClusterGraphDatalake& datalake() noexcept {
    return _datalake;
  }

  size_t& insertedDocuments() { return _insertedDocuments; }

  size_t& filteredDocuments() { return _filteredDocuments; }

  auto cacheVertex(VertexType const& vertexId, velocypack::Slice vertexSlice) -> void;
  auto cacheEdge(VertexType origin, EdgeType edgeId, velocypack::Slice edgeSlice, bool backward) -> void;

  auto isVertexCached(VertexType const& vertexKey) -> bool;
  auto isVertexRelationCached(VertexType const& vertex, bool backward) -> bool;
  auto isEdgeCached(EdgeType const& edge, bool backward) -> bool;

  auto getVertexRelations(VertexType const& vertex, bool backward)
      -> std::vector<std::pair<EdgeType, VertexType>>;

  auto getCachedVertex(VertexType const& vertex) -> VPackSlice;
  auto getCachedEdge(EdgeType const& edge, bool backward) -> VPackSlice;
  auto persistString(arangodb::velocypack::HashedStringRef idString) -> arangodb::velocypack::HashedStringRef;
  

 private:
  size_t _insertedDocuments;
  size_t _filteredDocuments;

  arangodb::ResourceMonitor& _resourceMonitor;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Stringheap to take care of _id strings, s.t. they stay valid
  ///        during the entire traversal.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::StringHeap _stringHeap;
  std::unordered_set<VertexType> _persistedStrings;

  /// @brief dump for our edge and vertex documents
  arangodb::graph::ClusterGraphDatalake _datalake;

  /// @brief maps to organize data access
  /// @brief vertex reference to vertex data slice
  std::unordered_map<VertexType, velocypack::Slice> _vertexData;

  /// @brief vertex reference to all connected edges including the edges target
  std::unordered_map<VertexType, std::vector<std::pair<EdgeType, VertexType>>> _vertexConnectedEdgesForward;
  std::unordered_map<VertexType, std::vector<std::pair<EdgeType, VertexType>>> _vertexConnectedEdgesBackward;

  /// @brief edge reference to edge data slice (TODO: one map will be enough here)
  std::unordered_map<EdgeType, velocypack::Slice> _edgeDataForward;
  std::unordered_map<EdgeType, velocypack::Slice> _edgeDataBackward;

  std::unordered_map<ServerID, aql::EngineId> const* _engines;
};

}  // namespace graph
}  // namespace arangodb
#endif
