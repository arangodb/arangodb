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

#include "NeighbourCache.h"

using namespace arangodb::graph;

auto NeighbourCache::rearm(VertexType vertexId) -> bool {
  _finished = false;
  auto it = _neighbours.find(vertexId);
  if (it != _neighbours.end()) {
    _currentEntry = it;
    // cache can only be used if vertex in cache includes all its neighbours
    if (std::get<0>(it->second)) {
      return true;
    }
    return false;
  }
  auto const& [iter, _inserted] = _neighbours.insert(  // insert empty
      {vertexId,
       std::make_tuple(
           false, std::vector<std::shared_ptr<std::vector<ExpansionInfo>>>{})});
  _currentEntry = iter;
  return false;
}

auto NeighbourCache::next() -> std::optional<NeighbourBatch> {
  TRI_ASSERT(_currentEntry != _neighbours.end());
  auto& batches = std::get<1>(_currentEntry->second);
  TRI_ASSERT(_currentBatchInCache != std::nullopt);
  // give next batch
  auto batch = *_currentBatchInCache;
  if (batch + 1 == batches.size()) {
    _finished = true;  // there are no more batches for vertex in the cache
  } else {
    _currentBatchInCache = batch + 1;  // next time give next batch
  }
  TRI_ASSERT(batch < batches.size());
  return batches[batch];
}

auto NeighbourCache::update(NeighbourBatch const& batch, bool isLastBatch)
    -> void {
  TRI_ASSERT(_currentEntry != _neighbours.end());
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
  _resourceMonitor.increaseMemoryUsage(newMemoryUsage);
  _memoryUsageVertexCache += newMemoryUsage;
}

auto NeighbourCache::clear() -> void {
  _neighbours.clear();
  _resourceMonitor.decreaseMemoryUsage(_memoryUsageVertexCache);
  _memoryUsageVertexCache = 0;
}
