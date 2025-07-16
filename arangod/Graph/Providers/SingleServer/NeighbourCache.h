////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/ResourceUsage.h"
#include "Graph/Providers/SingleServer/ExpansionInfo.h"
#include "Graph/Providers/TypeAliases.h"

namespace arangodb::graph {

struct NeighbourCache;

/**
   Iterator over neighbour batches of a specific vertex in the cache
 */
struct NeighbourIterator {
  NeighbourIterator(std::vector<NeighbourBatch>& batches)
      : _batches{batches},
        _nextOutputBatch{
            std::optional<typename std::vector<NeighbourBatch>::iterator>{
                batches.begin()}} {}
  auto next() -> std::optional<NeighbourBatch>;
  auto hasMore() -> bool { return _nextOutputBatch != _batches.end(); }

  std::vector<NeighbourBatch> const& _batches;
  std::optional<typename std::vector<NeighbourBatch>::iterator>
      _nextOutputBatch;  // current batch in vertex
};

/**
   Caches neighbours of vertices

   You need to call rearm first to define the vertex you want to handle in the
   following. If the cache includes all neighbours of the given vertex, the
   cache can be used and rearm returns an iterator of the neighbours. If the
   cache cannot be used for this vertex, rearm remembers the vertex and
   subsequent calls to update will update the neighbours for this vertex in the
   cache.

   A vertex cache entry can only be used when the entry includes all neighbours
   of the vertex.
 */
struct NeighbourCache {
  using Neighbours =
      containers::FlatHashMap<VertexType,       // vertex ID
                              std::tuple<bool,  // true if all neighbours of
                                                // this vertex are in cache
                                         std::vector<NeighbourBatch>>>;

  /**
     Defines the vertex that the cache should handle

     If the cache includes all neighbours of the given vertex, the
   cache can be used and rearm returns an iterator of the neighbours. If the
   cache cannot be used for this vertex, rearm remembers the vertex and
   subsequent calls to update will update the neighbours for this vertex in the
   cache.
   */
  auto rearm(VertexType vertexId) -> std::optional<NeighbourIterator>;

  /**
     Add batch to current vertex in cache

     Last batch needs to identify itself to let the cache know that the current
     vertex entry is complete and can in a subsequent cache request be read.
   */
  template<typename Monitor>
  auto update(NeighbourBatch const& batch, Monitor& monitor,
              bool isLastBatch = false) -> void {
    TRI_ASSERT(_currentEntry !=
               _neighbours.end());  // current entry exists in cache
    TRI_ASSERT(std::get<0>(_currentEntry->second) ==
               false);  // current entry is not yet complete
    auto& vec = std::get<1>(_currentEntry->second);
    vec.emplace_back(batch);
    if (isLastBatch) {
      std::get<0>(_currentEntry->second) =
          true;  // cache for this vertex finished
    }
    size_t newMemoryUsage = 0;
    for (auto const& neighbour : *batch) {
      newMemoryUsage += neighbour.size();
    }
    monitor.increaseMemoryUsage(newMemoryUsage);
    _memoryUsageVertexCache += newMemoryUsage;
  }

  template<typename Monitor>
  auto clear(Monitor& monitor) -> void {
    monitor.decreaseMemoryUsage(_memoryUsageVertexCache);
    _neighbours.clear();
    _currentEntry = _neighbours.begin();
    _memoryUsageVertexCache = 0;
  }

  Neighbours _neighbours;
  typename Neighbours::iterator _currentEntry;
  size_t _memoryUsageVertexCache = 0;
};

}  // namespace arangodb::graph
