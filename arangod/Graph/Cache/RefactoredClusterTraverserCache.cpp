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

#include "RefactoredClusterTraverserCache.h"

#include "Basics/StaticStrings.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;

namespace {
constexpr size_t costPerPersistedString = sizeof(void*) + sizeof(arangodb::velocypack::HashedStringRef);
constexpr size_t costPerVertexOrEdgeStringRefSlice = sizeof(velocypack::Slice) + sizeof(arangodb::velocypack::HashedStringRef);
constexpr size_t heapBlockSize = 4096;
};

RefactoredClusterTraverserCache::RefactoredClusterTraverserCache(ResourceMonitor& resourceMonitor)
    : _resourceMonitor{resourceMonitor},
      _stringHeap(resourceMonitor, heapBlockSize), /* arbitrary block-size may be adjusted for performance */
      _datalake(resourceMonitor) {}

RefactoredClusterTraverserCache::~RefactoredClusterTraverserCache() {
  clear();
}

void RefactoredClusterTraverserCache::clear() {
  _resourceMonitor.decreaseMemoryUsage(_persistedStrings.size() * ::costPerPersistedString);
  _resourceMonitor.decreaseMemoryUsage(_vertexData.size() * ::costPerVertexOrEdgeStringRefSlice);
  _resourceMonitor.decreaseMemoryUsage(_edgeData.size() * ::costPerVertexOrEdgeStringRefSlice);
  _stringHeap.clear();
  _persistedStrings.clear();
  _vertexData.clear();
  _edgeData.clear();
}

auto RefactoredClusterTraverserCache::cacheVertex(VertexType const& vertexId,
                                                  velocypack::Slice vertexSlice) -> void {
  auto [it, inserted] = _vertexData.try_emplace(vertexId, vertexSlice);
  if (inserted) {
    // If we have added something into the cache, we need to account for it.
    _resourceMonitor.increaseMemoryUsage(costPerVertexOrEdgeStringRefSlice);
  }
}

auto RefactoredClusterTraverserCache::isVertexCached(VertexType const& vertexKey) const
    -> bool {
  return _vertexData.find(vertexKey) != _vertexData.end();
}

auto RefactoredClusterTraverserCache::isEdgeCached(EdgeType const& edgeKey) const -> bool {
  return _edgeData.find(edgeKey) != _edgeData.end();
}

auto RefactoredClusterTraverserCache::getCachedVertex(VertexType const& vertex) const -> VPackSlice {
  if (!isVertexCached(vertex)) {
    return VPackSlice::nullSlice();
  }
  return _vertexData.at(vertex);
}

auto RefactoredClusterTraverserCache::getCachedEdge(EdgeType const& edge) const
    -> VPackSlice {
  if (!isEdgeCached(edge)) {
    return VPackSlice::nullSlice();
  }
  return _edgeData.at(edge);
}

auto RefactoredClusterTraverserCache::persistString(arangodb::velocypack::HashedStringRef idString) -> arangodb::velocypack::HashedStringRef {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  auto res = _stringHeap.registerString(idString);
  {
    ResourceUsageScope guard(_resourceMonitor, ::costPerPersistedString);
   
    _persistedStrings.emplace(res);
    
    // now make the TraverserCache responsible for memory tracking
    guard.steal();
  }
  return res;
}

auto RefactoredClusterTraverserCache::persistEdgeData(velocypack::Slice edgeSlice)
    -> std::pair<velocypack::Slice, bool> {
  arangodb::velocypack::HashedStringRef edgeIdRef(edgeSlice.get(StaticStrings::IdString));
  auto const [it, inserted] = _edgeData.try_emplace(edgeIdRef, edgeSlice);
  if (inserted) {
    _resourceMonitor.increaseMemoryUsage(costPerVertexOrEdgeStringRefSlice);
  }
  return std::make_pair(it->second, inserted);
}