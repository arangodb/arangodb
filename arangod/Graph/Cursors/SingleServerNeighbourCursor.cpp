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

#include "SingleServerNeighbourCursor.h"

#include "Graph/Cache/RefactoredTraverserCache.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Logger/LogMacros.h"
#include "Transaction/Helpers.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

using namespace arangodb::graph;

template<class Step>
SingleServerNeighbourCursor<Step>::SingleServerNeighbourCursor(
    Step const& step, size_t position, aql::Ast* ast,
    SingleServerProvider<Step>& provider, SingleServerBaseProviderOptions& opts,
    transaction::Methods* trx, ResourceMonitor& resourceMonitor,
    std::shared_ptr<aql::TraversalStats> stats,
    RefactoredTraverserCache& traverserCache, uint64_t batchSize)
    : _cursor{std::make_unique<SingleServerEdgeCursor<Step>>(
          resourceMonitor, trx, opts.tmpVar(), opts.indexInformations().first,
          opts.indexInformations().second, opts.expressionContext(),
          /*requiresFullDocument*/ opts.hasWeightMethod(), opts.useCache())},
      _step{step},
      _position{position},
      _batchSize{batchSize},
      _resourceMonitor{resourceMonitor},
      _stats{stats},
      _provider{provider},
      _traverserCache{traverserCache},
      _opts{opts} {
  TRI_ASSERT(_cursor != nullptr);
  _cursor->rearm(step.getVertex().getID(), step.getDepth(), *_stats);
  if (ast != nullptr) {
    _cursor->prepareIndexExpressions(ast);
  }
}

template<class Step>
auto SingleServerNeighbourCursor<Step>::next() -> std::vector<Step> {
  TRI_ASSERT(_cursor != nullptr);
  TRI_ASSERT(hasMore());

  auto newSteps = std::vector<Step>{};
  newSteps.reserve(_batchSize);
  _cursor->readNext(
      _batchSize, _provider, *_stats, _step.getDepth(),
      [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorID) -> void {
        VertexType vid = _traverserCache.persistString(([&]() -> auto {
          if (edge.isString()) {
            return VertexType(edge);
          } else {
            VertexType other(
                transaction::helpers::extractFromFromDocument(edge));
            if (other ==
                _step.getVertex().getID()) {  // TODO: Check getId - discuss
              other =
                  VertexType(transaction::helpers::extractToFromDocument(edge));
            }
            return other;
          }
        })());
        newSteps.emplace_back(Step{
            VertexRef{vid}, std::move(eid), _position, _step.getDepth() + 1,
            _opts.weightEdge(_step.getWeight(), edge), cursorID});
      });
  return newSteps;
}

template<typename Step>
auto SingleServerNeighbourCursor<Step>::hasDepthSpecificLookup(
    uint64_t depth) const noexcept -> bool {
  TRI_ASSERT(_cursor != nullptr);
  return _cursor->hasDepthSpecificLookup(depth);
}

template<typename Step>
auto SingleServerNeighbourCursor<Step>::hasMore() -> bool {
  TRI_ASSERT(_cursor != nullptr);
  return _cursor->hasMore(_step.getDepth());
}

template struct arangodb::graph::SingleServerNeighbourCursor<
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template struct arangodb::graph::SingleServerNeighbourCursor<
    enterprise::SmartGraphStep>;
#endif
