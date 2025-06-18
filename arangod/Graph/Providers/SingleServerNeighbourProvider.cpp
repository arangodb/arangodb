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
SingleServerNeighbourProvider<Step>::SingleServerNeighbourProvider(
    SingleServerBaseProviderOptions& opts, transaction::Methods* trx,
    ResourceMonitor& resourceMonitor, aql::TraversalStats& stats)
    : _cursor{std::make_unique<RefactoredSingleServerEdgeCursor<Step>>(
          resourceMonitor, trx, opts.tmpVar(), opts.indexInformations().first,
          opts.indexInformations().second, opts.expressionContext(),
          /*requiresFullDocument*/ opts.hasWeightMethod(), opts.useCache())},
      _resourceMonitor{resourceMonitor},
      _stats{stats} {
  // if (opts.indexInformations().second.empty()) {
  //   // If we have depth dependent filters, we must not use the cache,
  //   // otherwise, we do:
  //   _vertexCache.emplace();
  // }
}
template<class Step>
auto SingleServerNeighbourProvider<Step>::rearm(Step const& step) -> void {
  _currentStep = step;
  auto const& vertex = step.getVertex();
  TRI_ASSERT(_cursor != nullptr);
  _cursor->rearm(vertex.getID(), step.getDepth(), _stats);
  ++_rearmed;
}
template<class Step>
auto SingleServerNeighbourProvider<Step>::next(
    SingleServerProvider<Step>& provider)
    -> std::shared_ptr<std::vector<ExpansionInfo>> {
  TRI_ASSERT(_currentStep != std::nullopt);
  // TODO what to do with vertex cache? enter partly finished vertex?
  // // if cache includes vertex already: don't need to rearm
  // //                          full: directly return
  // //                          otherwise: continue
  // // First check the cache:
  // typename FoundVertexCache::iterator cacheEntry;
  // auto const& vertex = step.getVertex();
  // if (_vertexCache.has_value()) {
  //   auto it = _vertexCache->find(vertex.getID());
  //   if (it != _vertexCache->end()) {  // if vertex already exists in cache
  //     if (it->second /* is fully finished */) {
  //       // We have already expanded this vertex, so we can just use the
  //       // cached result:
  //       // return here batches of vertex cache instead
  //       return it->second;  // Return a copy of the shared_ptr
  //     } else {
  //       // check that cursor is currently doing this vertex
  //       cacheEntry = it;
  //     }
  //   } else {  // if vertex does not exist in cache, we have to rearm the
  //   cursor
  //     TRI_ASSERT(_cursor != nullptr);
  //     _cursor->rearm(vertex.getID(), step.getDepth(), _stats);
  //     ++_rearmed;
  //     auto [entry, _inserted] = _vertexCache->insert({vertex.getID(), {}});
  //     cacheEntry = entry;
  //   }
  // }

  std::shared_ptr<std::vector<ExpansionInfo>> newNeighbours =
      std::make_shared<std::vector<ExpansionInfo>>();
  _cursor->readAll(
      provider, _stats, _currentStep->getDepth(),
      [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorID) -> void {
        ++_readSomething;
        // Add to vector above:
        newNeighbours->emplace_back(std::move(eid), edge, cursorID);
      });
  if (_vertexCache.has_value()) {
    size_t newMemoryUsage = 0;
    for (auto const& neighbour : *newNeighbours) {
      newMemoryUsage += neighbour.size();
    }
    _resourceMonitor.increaseMemoryUsage(newMemoryUsage);
    // _memoryUsageVertexCache += newMemoryUsage;
    // TODO here add these new neighbours
    // cacheEntry->second->insert(cacheEntry->second->end(),
    //                            newNeighbours->begin(), newNeighbours->end());
  }
  return newNeighbours;

  // cursor->readBatch(
  //     [](arangodb::LocalDocumentId id) -> bool {
  //       // return true: has done something meaningful with document
  //       return false;
  //     },
  //     1000);
}

template<typename Step>
auto SingleServerNeighbourProvider<Step>::clear() -> void {
  if (_vertexCache.has_value()) {
    _vertexCache->clear();
  }
  _resourceMonitor.decreaseMemoryUsage(_memoryUsageVertexCache);
  _memoryUsageVertexCache = 0;

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

template struct arangodb::graph::SingleServerNeighbourProvider<
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template struct arangodb::graph::SingleServerNeighbourProvider<
    enterprise::SmartGraphStep>;
#endif
