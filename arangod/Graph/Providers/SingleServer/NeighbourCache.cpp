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

auto NeighbourIterator::next() -> std::optional<NeighbourBatch> {
  TRI_ASSERT(_nextOutputBatch != std::nullopt);
  // give next batch
  if (!hasMore()) {
    return std::nullopt;
  }
  auto value = *_nextOutputBatch;
  _nextOutputBatch = value + 1;
  return *value;
}

auto NeighbourCache::rearm(VertexType vertexId)
    -> std::optional<NeighbourIterator> {
  auto it = _neighbours.find(vertexId);
  if (it != _neighbours.end()) {
    _currentEntry = it;
    // cache can only be used if vertex in cache includes all its neighbours
    if (std::get<0>(it->second)) {
      auto& batches = std::get<1>(it->second);
      return NeighbourIterator{batches};
    }
    return std::nullopt;
  }
  auto const& [iter, _inserted] = _neighbours.insert(  // insert empty
      {vertexId,
       std::make_tuple(
           false, std::vector<std::shared_ptr<std::vector<ExpansionInfo>>>{})});
  _currentEntry = iter;
  return std::nullopt;
}
