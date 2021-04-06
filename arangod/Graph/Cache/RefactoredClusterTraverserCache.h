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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_REFACTORED_CLUSTER_TRAVERSER_CACHE_H
#define ARANGOD_GRAPH_REFACTORED_CLUSTER_TRAVERSER_CACHE_H 1

#include "Aql/types.h"
#include "Basics/StringHeap.h"
#include "Graph/ClusterGraphDatalake.h"
#include "Graph/Providers/TypeAliases.h"

#include <map>
#include <velocypack/Slice.h>

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Slice;
class HashedStringRef;
}  // namespace velocypack

namespace graph {

class RefactoredClusterTraverserCache {
 public:
  RefactoredClusterTraverserCache(ResourceMonitor& resourceMonitor);

  ~RefactoredClusterTraverserCache();

  void clear();

  arangodb::graph::ClusterGraphDatalake& datalake() noexcept {
    return _datalake;
  }

  auto persistString(arangodb::velocypack::HashedStringRef idString)
      -> arangodb::velocypack::HashedStringRef;

  auto cacheVertex(VertexType const& vertexId, velocypack::Slice vertexSlice) -> void;
  auto isVertexCached(VertexType const& vertexKey) const -> bool;
  auto getCachedVertex(VertexType const& vertex) const -> velocypack::Slice;

/**
 * @brief
 * 
 * Returns: first entry is the vpack that is inside the cache and stays valid during computation
 * The second entry indicates if the caller need to retain the handed in slice buffer.
 */
  auto persistEdgeData(velocypack::Slice edgeSlice) -> std::pair<velocypack::Slice, bool>;
  auto isEdgeCached(EdgeType const& edge) const -> bool;
  auto getCachedEdge(EdgeType const& edge) const -> velocypack::Slice;

 private:
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

  /// @brief edge reference to edge data slice
  std::unordered_map<EdgeType, velocypack::Slice> _edgeData;

};

}  // namespace graph
}  // namespace arangodb
#endif
