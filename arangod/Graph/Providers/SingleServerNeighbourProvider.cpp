////////////////////////////////////////////////////////////////////////////////
/// DISCLAIM#include "Aql/TraversalStats.h"
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

#include "Graph/Providers/SingleServerNeighbourProvider.h"
#include <s2/base/integral_types.h>
#include "Basics/ResourceUsage.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Logger/LogMacros.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

using namespace arangodb::graph;

template<class Step>
auto SingleServerNeighbourProvider<Step>::NeighbourCache::rearm(
    VertexType vertexId) -> bool {
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

template<class Step>
auto SingleServerNeighbourProvider<Step>::NeighbourCache::next()
    -> std::optional<NeighbourBatch> {
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

template<class Step>
auto SingleServerNeighbourProvider<Step>::NeighbourCache::update(
    NeighbourBatch const& batch, bool isLastBatch) -> void {
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

template<class Step>
auto SingleServerNeighbourProvider<Step>::NeighbourCache::clear() -> void {
  _neighbours.clear();
  _resourceMonitor.decreaseMemoryUsage(_memoryUsageVertexCache);
  _memoryUsageVertexCache = 0;
}

template<class Step>
SingleServerNeighbourProvider<Step>::SingleServerNeighbourProvider(
    SingleServerBaseProviderOptions& opts, transaction::Methods* trx,
    ResourceMonitor& resourceMonitor, aql::TraversalStats& stats)
    : _cursor{std::make_unique<RefactoredSingleServerEdgeCursor<Step>>(
          resourceMonitor, trx, opts.tmpVar(), opts.indexInformations().first,
          opts.indexInformations().second, opts.expressionContext(),
          /*requiresFullDocument*/ opts.hasWeightMethod(), opts.useCache())},
      _stats{}  // TODO should hand in stats here to use it as a reference
{
  if (opts.indexInformations().second.empty()) {
    // If we have depth dependent filters, we must not use the cache,
    // otherwise, we do:
    _neighbourCache.emplace(resourceMonitor);
  }
}
template<class Step>
auto SingleServerNeighbourProvider<Step>::rearm(Step const& step) -> void {
  _currentStep = step;
  auto const& vertex = step.getVertex();
  if (_neighbourCache && _neighbourCache->rearm(vertex.getID())) {
    _useCache = true;
  } else {
    _useCache = false;
    TRI_ASSERT(_cursor != nullptr);
    _cursor->rearm(vertex.getID(), step.getDepth(), _stats);
    ++_rearmed;
  }
}
template<class Step>
auto SingleServerNeighbourProvider<Step>::next(
    SingleServerProvider<Step>& provider)
    -> std::shared_ptr<std::vector<ExpansionInfo>> {
  TRI_ASSERT(_currentStep != std::nullopt);
  TRI_ASSERT(hasMore(_currentStep->getDepth()));

  if (_useCache) {
    TRI_ASSERT(_neighbourCache != std::nullopt);
    if (auto batch = _neighbourCache->next(); batch != std::nullopt) {
      return *batch;
    }
  }

  NeighbourBatch newNeighbours = std::make_shared<std::vector<ExpansionInfo>>();
  _cursor->readAll(
      provider, _stats, _currentStep->getDepth(),
      [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorID) -> void {
        ++_readSomething;
        newNeighbours->emplace_back(std::move(eid), edge, cursorID);
      });
  if (_neighbourCache.has_value()) {
    _neighbourCache->update(newNeighbours, !hasMore(_currentStep->getDepth()));
  }
  return newNeighbours;
}

template<typename Step>
auto SingleServerNeighbourProvider<Step>::clear() -> void {
  if (_neighbourCache.has_value()) {
    _neighbourCache->clear();
  }

  LOG_TOPIC("65261", TRACE, Logger::GRAPHS)
      << "Rearmed edge index cursor: " << _rearmed
      << " Read callback called: " << _readSomething;
  _rearmed = 0;
  _readSomething = 0;
}

template<typename Step>
auto SingleServerNeighbourProvider<Step>::prepareIndexExpressions(aql::Ast* ast)
    -> void {
  TRI_ASSERT(_cursor != nullptr);
  _cursor->prepareIndexExpressions(ast);
}

template<typename Step>
auto SingleServerNeighbourProvider<Step>::hasDepthSpecificLookup(
    uint64_t depth) const noexcept -> bool {
  return _cursor->hasDepthSpecificLookup(depth);
}

template<typename Step>
auto SingleServerNeighbourProvider<Step>::hasMore(uint64_t depth) -> bool {
  if (_useCache) {
    TRI_ASSERT(_neighbourCache != std::nullopt);
    return _neighbourCache->hasMore();
  }
  return _cursor->hasMore(depth);
}

template struct arangodb::graph::SingleServerNeighbourProvider<
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template struct arangodb::graph::SingleServerNeighbourProvider<
    enterprise::SmartGraphStep>;
#endif
