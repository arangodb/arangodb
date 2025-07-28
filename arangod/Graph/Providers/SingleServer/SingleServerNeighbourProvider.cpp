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

#include "SingleServerNeighbourProvider.h"

#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/SingleServer/ExpansionInfo.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Logger/LogMacros.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

using namespace arangodb::graph;

template<class Step>
SingleServerNeighbourProvider<Step>::SingleServerNeighbourProvider(
    SingleServerBaseProviderOptions& opts, transaction::Methods* trx,
    ResourceMonitor& resourceMonitor, uint64_t batchSize, bool useCache)
    : _cursor{std::make_unique<RefactoredSingleServerEdgeCursor<Step>>(
          resourceMonitor, trx, opts.tmpVar(), opts.indexInformations().first,
          opts.indexInformations().second, opts.expressionContext(),
          /*requiresFullDocument*/ opts.hasWeightMethod(), opts.useCache())},
      _batchSize{batchSize},
      _resourceMonitor{resourceMonitor} {
  if (useCache && opts.indexInformations().second.empty()) {
    // If we have depth dependent filters, we must not use the cache,
    // otherwise, we do:
    _neighbourCache.emplace();
  }
}
template<class Step>
auto SingleServerNeighbourProvider<Step>::rearm(Step const& step,
                                                aql::TraversalStats& stats)
    -> void {
  _currentStep = step;
  auto const& vertex = step.getVertex();
  if (_neighbourCache) {
    auto iterator = _neighbourCache->rearm(vertex.getID());
    if (iterator) {
      _currentStepNeighbourCacheIterator.emplace(std::move(iterator.value()));
      return;
    }
  }
  TRI_ASSERT(_cursor != nullptr);
  _currentStepNeighbourCacheIterator = std::nullopt;
  _cursor->rearm(vertex.getID(), step.getDepth(), stats);
  ++_rearmed;
}
template<class Step>
auto SingleServerNeighbourProvider<Step>::next(
    SingleServerProvider<Step>& provider, aql::TraversalStats& stats)
    -> std::shared_ptr<std::vector<ExpansionInfo>> {
  TRI_ASSERT(_currentStep != std::nullopt);
  TRI_ASSERT(hasMore(_currentStep->getDepth()));

  if (_currentStepNeighbourCacheIterator) {
    TRI_ASSERT(_neighbourCache != std::nullopt);
    if (auto batch = _currentStepNeighbourCacheIterator->next();
        batch != std::nullopt) {
      return *batch;
    }
  }

  NeighbourBatch newNeighbours = std::make_shared<std::vector<ExpansionInfo>>();
  newNeighbours->reserve(_batchSize);
  _cursor->readNext(
      _batchSize, provider, stats, _currentStep->getDepth(),
      [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorID) -> void {
        ++_readSomething;
        newNeighbours->emplace_back(std::move(eid), edge, cursorID);
      });
  if (_neighbourCache.has_value()) {
    _neighbourCache->update(newNeighbours, _resourceMonitor,
                            !hasMore(_currentStep->getDepth()));
  }
  return newNeighbours;
}

template<typename Step>
auto SingleServerNeighbourProvider<Step>::clear() -> void {
  if (_neighbourCache.has_value()) {
    _neighbourCache->clear(_resourceMonitor);
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
  if (_neighbourCache) {
    if (_currentStepNeighbourCacheIterator) {
      return _currentStepNeighbourCacheIterator->hasMore();
    }
  }
  TRI_ASSERT(_cursor != nullptr);
  return _cursor->hasMore(depth);
}

template struct arangodb::graph::SingleServerNeighbourProvider<
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template struct arangodb::graph::SingleServerNeighbourProvider<
    enterprise::SmartGraphStep>;
#endif
