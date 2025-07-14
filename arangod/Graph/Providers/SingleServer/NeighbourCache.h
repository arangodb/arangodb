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

struct NeighbourCache {
  using Neighbours =
      containers::FlatHashMap<VertexType,       // vertex ID
                              std::tuple<bool,  // true if all neighbours of
                                                // this vertex are in cache
                                         std::vector<NeighbourBatch>>>;

  NeighbourCache(ResourceMonitor& monitor) : _resourceMonitor{monitor} {}
  ~NeighbourCache() { clear(); }
  auto rearm(VertexType vertexId) -> bool;
  auto update(NeighbourBatch const& batch, bool isLastBatch) -> void;
  auto clear() -> void;
  Neighbours _neighbours;

  // iterator for a specific vertex (defined in _currentEntry)
  auto next() -> std::optional<NeighbourBatch>;
  auto hasMore() -> bool { return _finished == false; }
  typename Neighbours::iterator _currentEntry;
  std::optional<size_t> _currentBatchInCache =
      0;                   // batch number in current entry
  bool _finished = false;  // finished reading all batches of current entry
  size_t _memoryUsageVertexCache = 0;
  ResourceMonitor& _resourceMonitor;
};

}  // namespace arangodb::graph
