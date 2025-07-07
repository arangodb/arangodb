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

#include <s2/base/integral_types.h>
#include "Aql/TraversalStats.h"
#include "Basics/ResourceUsage.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "Graph/EdgeDocumentToken.h"

namespace arangodb::graph {

struct SingleServerBaseProviderOptions;

template<class Step>
struct SingleServerNeighbourProvider {
  struct ExpansionInfo {
    EdgeDocumentToken eid;
    std::vector<uint8_t> edgeData;  // keeps allocation
    size_t cursorId;
    ExpansionInfo(EdgeDocumentToken eid, VPackSlice edge, size_t cursorId)
        : eid(eid), cursorId(cursorId) {
      edgeData.resize(edge.byteSize());
      memcpy(edgeData.data(), edge.start(), edge.byteSize());
    }
    ExpansionInfo(ExpansionInfo const& other) = delete;
    ExpansionInfo(ExpansionInfo&& other) = default;
    VPackSlice edge() const noexcept { return VPackSlice(edgeData.data()); }
    size_t size() const noexcept {
      return sizeof(ExpansionInfo) + edgeData.size();
    }
  };
  using NeighbourBatch = std::shared_ptr<std::vector<ExpansionInfo>>;
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

  SingleServerNeighbourProvider(SingleServerBaseProviderOptions& opts,
                                transaction::Methods* trx,
                                ResourceMonitor& resourceMonitor,
                                aql::TraversalStats& stats);
  SingleServerNeighbourProvider(SingleServerNeighbourProvider const&) = delete;
  SingleServerNeighbourProvider(SingleServerNeighbourProvider&&) = default;
  SingleServerNeighbourProvider& operator=(
      SingleServerNeighbourProvider const&) = delete;
  auto next(SingleServerProvider<Step>& provider)
      -> std::shared_ptr<std::vector<ExpansionInfo>>;
  auto rearm(Step const& step) -> void;
  auto clear() -> void;
  auto prepareIndexExpressions(aql::Ast* ast) -> void;
  auto hasDepthSpecificLookup(uint64_t depth) const noexcept -> bool;
  auto hasMore(uint64_t depth) -> bool;

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor<Step>> _cursor;

  std::optional<Step> _currentStep;
  bool _useCache = false;

  size_t _rearmed = 0;
  size_t _readSomething = 0;
  std::optional<NeighbourCache> _neighbourCache;
  arangodb::aql::TraversalStats
      _stats;  // TODO there is a problem with handing this provider a stats
               // reference, so currently it just creates a new one (which is
               // never used)
};

}  // namespace arangodb::graph
