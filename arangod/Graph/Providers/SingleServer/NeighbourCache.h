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

#include "Graph/Providers/SingleServer/ExpansionInfo.h"
#include "Graph/Providers/TypeAliases.h"

namespace arangodb::graph {

struct NeighbourCache;

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

struct NeighbourCache {
  using Neighbours =
      containers::FlatHashMap<VertexType,       // vertex ID
                              std::tuple<bool,  // true if all neighbours of
                                                // this vertex are in cache
                                         std::vector<NeighbourBatch>>>;

  ~NeighbourCache() { clear(); }
  auto rearm(VertexType vertexId) -> std::optional<NeighbourIterator>;
  auto update(NeighbourBatch const& batch, bool isLastBatch = false) -> size_t;
  auto clear() -> size_t;

  Neighbours _neighbours;
  typename Neighbours::iterator _currentEntry;
  size_t _memoryUsageVertexCache = 0;
};

}  // namespace arangodb::graph
