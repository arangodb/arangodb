////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "./SingleServerProvider.h"

#include "Aql/QueryContext.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Transaction/Helpers.h"

#include "Futures/Future.h"
#include "Futures/Utilities.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include <vector>

using namespace arangodb;
using namespace arangodb::graph;

template <class Step>
void SingleServerProvider<Step>::addEdgeToBuilder(typename Step::Edge const& edge,
                                                  arangodb::velocypack::Builder& builder) {
  if (edge.isValid()) {
    insertEdgeIntoResult(edge.getID(), builder);
  } else {
    // We can never hand out invalid ids.
    // For production just be sure to add something sensible.
    builder.add(VPackSlice::nullSlice());
  }
};

template <class Step>
void SingleServerProvider<Step>::addEdgeIDToBuilder(typename Step::Edge const& edge,
                                                    arangodb::velocypack::Builder& builder) {
  if (edge.isValid()) {
    insertEdgeIntoResult(edge.getID(), builder);
  } else {
    // We can never hand out invalid ids.
    // For production just be sure to add something sensible.
    builder.add(VPackSlice::nullSlice());
  }
};

template <class Step>
SingleServerProvider<Step>::SingleServerProvider(arangodb::aql::QueryContext& queryContext,
                                                 BaseProviderOptions opts,
                                                 arangodb::ResourceMonitor& resourceMonitor)
    : _trx(std::make_unique<arangodb::transaction::Methods>(queryContext.newTrxContext())),
      _opts(std::move(opts)),
      _cache(_trx.get(), &queryContext, resourceMonitor, _stats,
             _opts.collectionToShardMap()),
      _stats{} {
  // TODO CHECK RefactoredTraverserCache (will be discussed in the future, need to do benchmarks if affordable)
  // activateCache(false);
  _cursor = buildCursor(opts.expressionContext());
}

template <class Step>
void SingleServerProvider<Step>::activateCache(bool enableDocumentCache) {
  // Do not call this twice.
  // TRI_ASSERT(_cache == nullptr);
  // TODO: enableDocumentCache check + opts check + cacheManager check
  /*
  if (enableDocumentCache) {
    auto cacheManager = CacheManagerFeature::MANAGER;
    if (cacheManager != nullptr) {
      // TODO CHECK: cacheManager functionality
      //  std::shared_ptr<arangodb::cache::Cache> cache = cacheManager->createCache(cache::CacheType::Plain);
      if (cache != nullptr) {
        TraverserCache(query, options) return new TraverserCache(query, std::move(cache));
      }
    }
    // fallthrough intentional
  }*/
  //  _cache = new RefactoredTraverserCache(query());
}

template <class Step>
auto SingleServerProvider<Step>::startVertex(VertexType vertex, size_t depth, double weight)
    -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Start Vertex:" << vertex;

  // Create default initial step
  // Note: Refactor naming, Strings in our cache here are not allowed to be removed.
  return Step(_cache.persistString(vertex), depth, weight);
}

template <class Step>
auto SingleServerProvider<Step>::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  // Should never be called in SingleServer case
  TRI_ASSERT(false);
  LOG_TOPIC("c9160", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Fetching...";
  std::vector<Step*> result{};
  result.reserve(looseEnds.size());

  return futures::makeFuture(std::move(result));
}

template <class Step>
auto SingleServerProvider<Step>::expand(Step const& step, size_t previous,
                                        std::function<void(Step)> const& callback)
    -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();
  TRI_ASSERT(_cursor != nullptr);
  LOG_TOPIC("c9169", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Expanding " << vertex.getID();
  _cursor->rearm(vertex.getID(), step.getDepth());
  _cursor->readAll(*this, _stats, step.getDepth(), [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorID) -> void {
    VertexType id = _cache.persistString(([&]() -> auto {
      if (edge.isString()) {
        return VertexType(edge);
      } else {
        VertexType other(transaction::helpers::extractFromFromDocument(edge));
        if (other == vertex.getID()) {  // TODO: Check getId - discuss
          other = VertexType(transaction::helpers::extractToFromDocument(edge));
        }
        return other;
      }
    })());
    // TODO: Adjust log output
    LOG_TOPIC("c9168", TRACE, Logger::GRAPHS)
        << "<SingleServerProvider> Neighbor of " << vertex.getID() << " -> " << id;

    callback(Step{id, std::move(eid), previous, step.getDepth() + 1, _opts.weightEdge(step.getWeight(), edge), cursorID});
  });
}

template <class Step>
void SingleServerProvider<Step>::addVertexToBuilder(typename Step::Vertex const& vertex,
                                                    arangodb::velocypack::Builder& builder,
                                                    bool writeIdIfNotFound) {
  _cache.insertVertexIntoResult(_stats, vertex.getID(), builder, writeIdIfNotFound);
}

template <class Step>
auto SingleServerProvider<Step>::clear() -> void {
  // Clear the cache - this cache does contain StringRefs
  // We need to make sure that no one holds references to the cache (!)
  _cache.clear();
}

template <class Step>
void SingleServerProvider<Step>::insertEdgeIntoResult(EdgeDocumentToken edge,
                                                      arangodb::velocypack::Builder& builder) {
  _cache.insertEdgeIntoResult(edge, builder);
}

template <class Step>
void SingleServerProvider<Step>::insertEdgeIdIntoResult(EdgeDocumentToken edge,
                                                        arangodb::velocypack::Builder& builder) {
  _cache.insertEdgeIdIntoResult(edge, builder);
}

template <class Step>
void SingleServerProvider<Step>::prepareIndexExpressions(aql::Ast* ast) {
  TRI_ASSERT(_cursor != nullptr);
  _cursor->prepareIndexExpressions(ast);
}

template <class Step>
std::unique_ptr<RefactoredSingleServerEdgeCursor<Step>> SingleServerProvider<Step>::buildCursor(
    arangodb::aql::FixedVarExpressionContext& expressionContext) {
  return std::make_unique<RefactoredSingleServerEdgeCursor<Step>>(
      trx(), _opts.tmpVar(), _opts.indexInformations().first,
      _opts.indexInformations().second, expressionContext, _opts.hasWeightMethod());
}

template <class Step>
arangodb::transaction::Methods* SingleServerProvider<Step>::trx() {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_trx->state() != nullptr);
  TRI_ASSERT(_trx->transactionContextPtr() != nullptr);
  return _trx.get();
}

template <class Step>
arangodb::aql::TraversalStats SingleServerProvider<Step>::stealStats() {
  auto t = _stats;
  // Placement new of stats, do not reallocate space.
  _stats.~TraversalStats();
  new (&_stats) aql::TraversalStats{};
  return t;
}

template class arangodb::graph::SingleServerProvider<SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class arangodb::graph::SingleServerProvider<enterprise::SmartGraphStep>;
#endif
