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

#include "Aql/ExecutionBlock.h"
#include "Aql/QueryContext.h"
#include "Aql/TraversalStats.h"
#include "Futures/Future.h"
#include "Futures/Utilities.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Logger/LogMacros.h"
#include "Transaction/Helpers.h"
#include "VocBase/vocbase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/QueryRegistryFeature.h"

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
    _edgeLookup.insertEdgeIntoLookupMap(edge.getID(), builder);
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
      _stats(std::make_unique<aql::TraversalStats>()),
      _cache(resourceMonitor),
      _vertexLookup(_trx.get(), &queryContext, _opts.getVertexProjections(),
                    _opts.collectionToShardMap(), _stats,
                    ServerState::instance()->isSingleServer() &&
                        !queryContext.vocbase()
                             .server()
                             .getFeature<QueryRegistryFeature>()
                             .requireWith(),
                    _opts.produceVertices()),
      _edgeLookup(_trx.get(), _opts.getEdgeProjections()),
      _neighbours{_opts, _trx.get(), _monitor,
                  aql::ExecutionBlock::DefaultBatchSize} {}

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
auto SingleServerProvider<Step>::expand(
    Step const& step, size_t previous,
    std::function<void(Step)> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  LOG_TOPIC("c9169", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Expanding " << vertex.getID();

  _neighbours.rearm(step, _stats);

  // TODO (in a later PR) return each batch of neighbours instead of iterating
  // over all of them
  while (_neighbours.hasMore(step.getDepth())) {
    auto batch = _neighbours.next(*this, _stats);
    for (auto const& neighbour : *batch) {
      VPackSlice edge = neighbour.edge();
      VertexType id = _cache.persistString(([&]() -> auto {
        if (edge.isString()) {
          return VertexType(edge);
        } else {
          VertexType other(transaction::helpers::extractFromFromDocument(edge));
          if (other == vertex.getID()) {  // TODO: Check getId - discuss
            other =
                VertexType(transaction::helpers::extractToFromDocument(edge));
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
}

template<class Step>
auto SingleServerProvider<Step>::expandToNextBatch(
    Step const& step, size_t previous,
    std::function<void(Step)> const& callback) -> bool {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  TRI_ASSERT(not _neighboursStack.empty());
  auto& last = _neighboursStack.back();

  LOG_TOPIC("c9179", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Expanding (next batch) " << vertex.getID();

  if (not last.hasMore(step.getDepth())) {
    _neighboursStack.pop_back();
    return false;
  }

  auto count = 0;
  auto batch = last.next(*this, _stats);
  for (auto const& neighbour : *batch) {
    count++;
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
    LOG_TOPIC("c916e", TRACE, Logger::GRAPHS)
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
  if (count == 0) {
    _neighboursStack.pop_back();
    return false;
  }
  return true;
}

template<class Step>
auto SingleServerProvider<Step>::addExpansionIterator(
    Step const& step, std::function<void()> const& callback) -> void {
  TRI_ASSERT(!step.isLooseEnd());
  auto const& vertex = step.getVertex();

  LOG_TOPIC("c9189", TRACE, Logger::GRAPHS)
      << "<SingleServerProvider> Add expansion iterator " << vertex.getID();

  // create neighbour provider without using cache because:
  // 1. cache does not make sense when we create a new provider and with it a
  // new cache for each vertex (what we do here)
  // 2. cache results currently in a resource manager crash when used with this
  // stack
  _neighboursStack.emplace_back(_opts, _trx.get(), _monitor,
                                aql::ExecutionBlock::DefaultBatchSize, false);
  auto& last = _neighboursStack.back();
  last.rearm(step, _stats);
  callback();
}

template<class Step>
void SingleServerProvider<Step>::addVertexToBuilder(
    typename Step::Vertex const& vertex, arangodb::velocypack::Builder& builder,
    bool writeIdIfNotFound) {
  if (_opts.produceVertices()) {
    _vertexLookup.insertVertexIntoResult(vertex.getID(), builder,
                                         writeIdIfNotFound);
  } else {
    builder.add(VPackSlice::nullSlice());
  }
}

template<class Step>
auto SingleServerProvider<Step>::clear() -> void {
  // Clear the cache - this cache does contain StringRefs
  // We need to make sure that no one holds references to the cache (!)
  _cache.clear();
  _neighbours.clear();
  _neighboursStack.clear();
}

template<class Step>
void SingleServerProvider<Step>::insertEdgeIntoResult(
    EdgeDocumentToken edge, arangodb::velocypack::Builder& builder) {
  _edgeLookup.insertEdgeIntoResult(edge, builder);
}

template<class Step>
void SingleServerProvider<Step>::insertEdgeIdIntoResult(
    EdgeDocumentToken edge, arangodb::velocypack::Builder& builder) {
  _edgeLookup.insertEdgeIdIntoResult(edge, builder);
}

template<class Step>
std::string SingleServerProvider<Step>::getEdgeId(
    typename Step::Edge const& edge) {
  return _edgeLookup.getEdgeId(edge.getID());
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
  _neighbours.prepareIndexExpressions(ast);
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
  auto t = *_stats;
  _stats->clear();
  return t;
}

template<class StepType>
auto SingleServerProvider<StepType>::fetchVertices(
    const std::vector<Step*>& looseEnds) -> std::vector<Step*> {
  // We will never need to fetch anything
  TRI_ASSERT(false);
  return std::vector<Step*>{};
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
  return _neighbours.hasDepthSpecificLookup(depth);
}

template class arangodb::graph::SingleServerProvider<SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class arangodb::graph::SingleServerProvider<
    enterprise::SmartGraphStep>;
#endif
