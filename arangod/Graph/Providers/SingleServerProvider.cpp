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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SingleServerProvider.h"

#include "Aql/QueryContext.h"
#include "Futures/Future.h"
#include "Futures/Utilities.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Logger/LogMacros.h"
#include "Transaction/Helpers.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

#include <vector>

using namespace arangodb;
using namespace arangodb::graph;

template<class Step>
void SingleServerProvider<Step>::addEdgeToBuilder(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  if (edge.isValid()) {
    insertEdgeIntoResult(edge.getID(), builder);
  } else {
    // We can never hand out invalid ids.
    // For production just be sure to add something sensible.
    builder.add(VPackSlice::nullSlice());
  }
};

template<class Step>
void SingleServerProvider<Step>::addEdgeIDToBuilder(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  if (edge.isValid()) {
    insertEdgeIdIntoResult(edge.getID(), builder);
  } else {
    // We can never hand out invalid ids.
    // For production just be sure to add something sensible.
    builder.add(VPackSlice::nullSlice());
  }
};

template<class Step>
void SingleServerProvider<Step>::addEdgeToLookupMap(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  if (edge.isValid()) {
    _cache.insertEdgeIntoLookupMap(edge.getID(), builder);
  }
}

template<class Step>
SingleServerProvider<Step>::SingleServerProvider(
    arangodb::aql::QueryContext& queryContext,
    SingleServerBaseProviderOptions opts,
    arangodb::ResourceMonitor& resourceMonitor)
    : _monitor(resourceMonitor),
      _trx(std::make_unique<arangodb::transaction::Methods>(
          queryContext.newTrxContext())),
      _opts(std::move(opts)),
      _cache(_trx.get(), &queryContext, resourceMonitor, _stats,
             _opts.collectionToShardMap(), _opts.getVertexProjections(),
             _opts.getEdgeProjections(), _opts.produceVertices()),
      _stats{} {
  _cursor = buildCursor(opts.expressionContext());
  if (_opts.indexInformations().second.empty()) {
    // If we have depth dependent filters, we must not use the cache,
    // otherwise, we do:
    _vertexCache.emplace();
  }
}

template<class Step>
auto SingleServerProvider<Step>::startVertex(VertexType vertex, size_t depth,
                                             double weight) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Start Vertex:" << vertex;

  // Create default initial step
  // Note: Refactor naming, Strings in our cache here are not allowed to be
  // removed.
  return Step(_cache.persistString(vertex), depth, weight);
}

template<class Step>
auto SingleServerProvider<Step>::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  // Should never be called in SingleServer case
  TRI_ASSERT(false);
  LOG_TOPIC("c9160", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Fetching...";
  std::vector<Step*> result{};
  return futures::makeFuture(std::move(result));
}

template<class Step>
std::shared_ptr<std::vector<typename SingleServerProvider<Step>::ExpansionInfo>>
SingleServerProvider<Step>::getNeighbours(Step const& step) {
  // First check the cache:
  auto const& vertex = step.getVertex();
  if (_vertexCache.has_value()) {
    auto it = _vertexCache->find(vertex.getID());
    if (it != _vertexCache->end()) {
      // We have already expanded this vertex, so we can just use the
      // cached result:
      return it->second;  // Return a copy of the shared_ptr
    }
  }

  // We actually have to run the cursor:
  TRI_ASSERT(_cursor != nullptr);
  std::shared_ptr<std::vector<ExpansionInfo>> newNeighbours =
      std::make_shared<std::vector<ExpansionInfo>>();

  _cursor->rearm(vertex.getID(), step.getDepth(), _stats);
  ++_rearmed;
  _cursor->readAll(
      *this, _stats, step.getDepth(),
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
    _monitor.increaseMemoryUsage(newMemoryUsage);
    _memoryUsageVertexCache += newMemoryUsage;
    _vertexCache->insert({vertex.getID(), newNeighbours});
  }
  return newNeighbours;
}

template<class Step>
auto SingleServerProvider<Step>::expand(
    Step const& step, size_t previous,
    std::function<void(Step)> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  LOG_TOPIC("c9169", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Expanding " << vertex.getID();

  std::shared_ptr<std::vector<ExpansionInfo>> neighbours = getNeighbours(step);
  // Now do the work:
  for (auto const& neighbour : *neighbours) {
    VPackSlice edge = neighbour.edge();
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
    LOG_TOPIC("c9168", TRACE, Logger::GRAPHS)
        << "<SingleServerProvider> Neighbor of " << vertex.getID() << " -> "
        << id;

    EdgeDocumentToken edgeToken{neighbour.eid};
    callback(Step{id, std::move(edgeToken), previous, step.getDepth() + 1,
                  _opts.weightEdge(step.getWeight(), edge),
                  neighbour.cursorId});
    // TODO [GraphRefactor]: Why is cursorID set, but never used?
    // Note: There is one implementation that used, it, but there is a high
    // probability we do not need it anymore after refactoring is complete.
  }
}

template<class Step>
void SingleServerProvider<Step>::addVertexToBuilder(
    typename Step::Vertex const& vertex, arangodb::velocypack::Builder& builder,
    bool writeIdIfNotFound) {
  _cache.insertVertexIntoResult(_stats, vertex.getID(), builder,
                                writeIdIfNotFound);
}

template<class Step>
auto SingleServerProvider<Step>::clear() -> void {
  // Clear the cache - this cache does contain StringRefs
  // We need to make sure that no one holds references to the cache (!)
  _cache.clear();
  if (_vertexCache.has_value()) {
    _vertexCache->clear();
  }
  _monitor.decreaseMemoryUsage(_memoryUsageVertexCache);
  _memoryUsageVertexCache = 0;

  LOG_TOPIC("65261", TRACE, Logger::GRAPHS)
      << "Rearmed edge index cursor: " << _rearmed
      << " Read callback called: " << _readSomething;
  _rearmed = 0;
  _readSomething = 0;
}

template<class Step>
void SingleServerProvider<Step>::insertEdgeIntoResult(
    EdgeDocumentToken edge, arangodb::velocypack::Builder& builder) {
  _cache.insertEdgeIntoResult(edge, builder);
}

template<class Step>
void SingleServerProvider<Step>::insertEdgeIdIntoResult(
    EdgeDocumentToken edge, arangodb::velocypack::Builder& builder) {
  _cache.insertEdgeIdIntoResult(edge, builder);
}

template<class Step>
std::string SingleServerProvider<Step>::getEdgeId(
    typename Step::Edge const& edge) {
  return _cache.getEdgeId(edge.getID());
}

template<class Step>
EdgeType SingleServerProvider<Step>::getEdgeIdRef(
    typename Step::Edge const& edge) {
  // TODO: Maybe we can simplify this
  auto id = getEdgeId(edge);
  VPackHashedStringRef hashed{id.data(), static_cast<uint32_t>(id.size())};
  return _cache.persistString(hashed);
}

template<class Step>
void SingleServerProvider<Step>::prepareIndexExpressions(aql::Ast* ast) {
  TRI_ASSERT(_cursor != nullptr);
  _cursor->prepareIndexExpressions(ast);
}

template<class Step>
void SingleServerProvider<Step>::prepareContext(aql::InputAqlItemRow input) {
  _opts.prepareContext(std::move(input));
}

template<class Step>
void SingleServerProvider<Step>::unPrepareContext() {
  _opts.unPrepareContext();
}

#ifndef USE_ENTERPRISE
template<class Step>
bool SingleServerProvider<Step>::isResponsible(Step const& step) const {
  return true;
}
#else
// Include Enterprise part of the template implementation
#include "Enterprise/Graph/Providers/SingleServerProviderEE.tpp"
#endif

template<class Step>
std::unique_ptr<RefactoredSingleServerEdgeCursor<Step>>
SingleServerProvider<Step>::buildCursor(
    arangodb::aql::FixedVarExpressionContext& expressionContext) {
  return std::make_unique<RefactoredSingleServerEdgeCursor<Step>>(
      monitor(), trx(), _opts.tmpVar(), _opts.indexInformations().first,
      _opts.indexInformations().second, expressionContext,
      /*requiresFullDocument*/ _opts.hasWeightMethod(), _opts.useCache());
}

template<class Step>
ResourceMonitor& SingleServerProvider<Step>::monitor() {
  return _monitor;
}

template<class Step>
arangodb::transaction::Methods* SingleServerProvider<Step>::trx() {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_trx->state() != nullptr);
  TRI_ASSERT(_trx->transactionContextPtr() != nullptr);
  return _trx.get();
}

template<class Step>
TRI_vocbase_t const& SingleServerProvider<Step>::vocbase() const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_trx->state() != nullptr);
  TRI_ASSERT(_trx->transactionContextPtr() != nullptr);
  return _trx.get()->vocbase();
}

template<class Step>
arangodb::aql::TraversalStats SingleServerProvider<Step>::stealStats() {
  auto t = _stats;
  _stats.clear();
  return t;
}

template<class StepType>
auto SingleServerProvider<StepType>::fetchVertices(
    const std::vector<Step*>& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  // We will never need to fetch anything
  TRI_ASSERT(false);
  return fetch(looseEnds);
}

template<class StepType>
auto SingleServerProvider<StepType>::fetchEdges(
    const std::vector<Step*>& fetchedVertices) -> Result {
  // We will never need to fetch anything
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

template<class StepType>
bool SingleServerProvider<StepType>::hasDepthSpecificLookup(
    uint64_t depth) const noexcept {
  return _cursor->hasDepthSpecificLookup(depth);
}

template class arangodb::graph::SingleServerProvider<SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class arangodb::graph::SingleServerProvider<
    enterprise::SmartGraphStep>;
#endif
