////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ProviderTracer.h"

#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Futures/Future.h"
#include "Logger/LogMacros.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Steps/ClusterProviderStep.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#endif

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderImpl>
ProviderTracer<ProviderImpl>::ProviderTracer(
    arangodb::aql::QueryContext& queryContext, Options opts,
    arangodb::ResourceMonitor& resourceMonitor)
    : _impl{queryContext, std::move(opts), resourceMonitor} {}

template<class ProviderImpl>
ProviderTracer<ProviderImpl>::~ProviderTracer() {
  LOG_TOPIC("6dbdf", INFO, Logger::GRAPHS) << "Provider Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("9c1db", INFO, Logger::GRAPHS) << "  " << name << ": " << trace;
  }
}

template<class ProviderImpl>
typename ProviderImpl::Step ProviderTracer<ProviderImpl>::startVertex(
    VertexType vertex, size_t depth, double weight) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["startVertex"].addTiming(TRI_microtime() - start);
  });
  return _impl.startVertex(vertex, depth, weight);
}

template<class ProviderImpl>
futures::Future<std::vector<typename ProviderImpl::Step*>>
ProviderTracer<ProviderImpl>::fetchVertices(
    std::vector<typename ProviderImpl::Step*> const& looseEnds) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["fetchVertices"].addTiming(TRI_microtime() - start);
  });
  return _impl.fetchVertices(std::move(looseEnds));
}

template<class ProviderImpl>
Result ProviderTracer<ProviderImpl>::fetchEdges(
    std::vector<typename ProviderImpl::Step*> const& fetchedVertices) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["fetchEdges"].addTiming(TRI_microtime() - start);
  });
  return _impl.fetchEdges(fetchedVertices);
}

template<class ProviderImpl>
futures::Future<std::vector<typename ProviderImpl::Step*>>
ProviderTracer<ProviderImpl>::fetch(
    std::vector<typename ProviderImpl::Step*> const& looseEnds) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { _stats["fetch"].addTiming(TRI_microtime() - start); });
  return _impl.fetch(std::move(looseEnds));
}

template<class ProviderImpl>
auto ProviderTracer<ProviderImpl>::expand(Step const& from, size_t previous,
                                          std::function<void(Step)> callback)
    -> void {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { _stats["expand"].addTiming(TRI_microtime() - start); });
  _impl.expand(from, previous, std::move(callback));
}

template<class ProviderImpl>
auto ProviderTracer<ProviderImpl>::clear() -> void {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { _stats["clear"].addTiming(TRI_microtime() - start); });
  _impl.clear();
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::addVertexToBuilder(
    typename Step::Vertex const& vertex,
    arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["addVertexToBuilder"].addTiming(TRI_microtime() - start);
  });
  return _impl.addVertexToBuilder(vertex, builder);
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::addEdgeToBuilder(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["addEdgeToBuilder"].addTiming(TRI_microtime() - start);
  });
  return _impl.addEdgeToBuilder(edge, builder);
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::addEdgeIDToBuilder(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["addEdgeToIDBuilder"].addTiming(TRI_microtime() - start);
  });
  return _impl.addEdgeIDToBuilder(edge, builder);
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::addEdgeToLookupMap(
    typename Step::Edge const& edge, arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["addEdgeToLookupMap"].addTiming(TRI_microtime() - start);
  });
  return _impl.addEdgeToLookupMap(edge, builder);
}

template<class ProviderImpl>
auto ProviderTracer<ProviderImpl>::getEdgeId(typename Step::Edge const& edge)
    -> std::string {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["getEdgeId"].addTiming(TRI_microtime() - start);
  });
  return _impl.getEdgeId(edge);
}

template<class ProviderImpl>
auto ProviderTracer<ProviderImpl>::getEdgeIdRef(typename Step::Edge const& edge)
    -> EdgeType {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["getEdgeIdRef"].addTiming(TRI_microtime() - start);
  });
  return _impl.getEdgeIdRef(edge);
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::destroyEngines() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["destroyEngines"].addTiming(TRI_microtime() - start);
  });
  return _impl.destroyEngines();
}

template<class ProviderImpl>
aql::TraversalStats ProviderTracer<ProviderImpl>::stealStats() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["stealStats"].addTiming(TRI_microtime() - start);
  });
  return _impl.stealStats();
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::prepareIndexExpressions(aql::Ast* ast) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["prepareIndexExpressions"].addTiming(TRI_microtime() - start);
  });
  return _impl.prepareIndexExpressions(ast);
}

template<class ProviderImpl>
transaction::Methods* ProviderTracer<ProviderImpl>::trx() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { _stats["trx"].addTiming(TRI_microtime() - start); });
  return _impl.trx();
}

template<class ProviderImpl>
TRI_vocbase_t const& ProviderTracer<ProviderImpl>::vocbase() const {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard(
      [&]() noexcept { _stats["vocbase"].addTiming(TRI_microtime() - start); });
  return _impl.vocbase();
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::prepareContext(aql::InputAqlItemRow input) {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["prepareContext"].addTiming(TRI_microtime() - start);
  });
  _impl.prepareContext(input);
}

template<class ProviderImpl>
void ProviderTracer<ProviderImpl>::unPrepareContext() {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["unPrepareContext"].addTiming(TRI_microtime() - start);
  });
  _impl.unPrepareContext();
}

template<class ProviderImpl>
bool ProviderTracer<ProviderImpl>::hasDepthSpecificLookup(
    uint64_t depth) const noexcept {
  return _impl.hasDepthSpecificLookup(depth);
}

template<class ProviderImpl>
bool ProviderTracer<ProviderImpl>::isResponsible(const Step& step) const {
  double start = TRI_microtime();
  auto sg = arangodb::scopeGuard([&]() noexcept {
    _stats["isResponsible"].addTiming(TRI_microtime() - start);
  });
  return _impl.isResponsible(step);
}
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::ProviderTracer<
    arangodb::graph::SingleServerProvider<SingleServerProviderStep>>;

#ifdef USE_ENTERPRISE
template class ::arangodb::graph::ProviderTracer<
    arangodb::graph::SingleServerProvider<
        arangodb::graph::enterprise::SmartGraphStep>>;

template class ::arangodb::graph::ProviderTracer<
    arangodb::graph::enterprise::SmartGraphProvider<
        arangodb::graph::ClusterProviderStep>>;
#endif

template class ::arangodb::graph::ProviderTracer<
    arangodb::graph::ClusterProvider<ClusterProviderStep>>;
