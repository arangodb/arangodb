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
////////////////////////////////////////////////////////////////////////////////

#include "ProviderTracer.h"

#include "Basics/ScopeGuard.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"

#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/SingleServerProvider.h"

using namespace arangodb;
using namespace arangodb::graph;

template <class ProviderImpl>
ProviderTracer<ProviderImpl>::ProviderTracer(arangodb::aql::QueryContext& queryContext,
                                             Options opts,
                                             arangodb::ResourceMonitor& resourceMonitor)
    : _impl{queryContext, opts, resourceMonitor} {}

template <class ProviderImpl>
ProviderTracer<ProviderImpl>::~ProviderTracer() {
  LOG_TOPIC("6dbdf", INFO, Logger::GRAPHS) << "Provider Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("9c1db", INFO, Logger::GRAPHS) << "  " << name << ": " << trace;
  }
}

template <class ProviderImpl>
typename ProviderImpl::Step ProviderTracer<ProviderImpl>::startVertex(VertexType vertex) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["startVertex"].addTiming(TRI_microtime() - start));
  return _impl.startVertex(vertex);
}

template <class ProviderImpl>
futures::Future<std::vector<typename ProviderImpl::Step*>> ProviderTracer<ProviderImpl>::fetch(
    std::vector<typename ProviderImpl::Step*> const& looseEnds) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["fetch"].addTiming(TRI_microtime() - start));
  return _impl.fetch(std::move(looseEnds));
}

template <class ProviderImpl>
auto ProviderTracer<ProviderImpl>::expand(Step const& from, size_t previous,
                                          std::function<void(Step)> callback) -> void {
  double start = TRI_microtime();
  TRI_DEFER(_stats["expand"].addTiming(TRI_microtime() - start));
  _impl.expand(from, previous, std::move(callback));
}

template <class ProviderImpl>
void ProviderTracer<ProviderImpl>::addVertexToBuilder(typename Step::Vertex const& vertex,
                                                      arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["addVertexToBuilder"].addTiming(TRI_microtime() - start));
  return _impl.addVertexToBuilder(vertex, builder);
}

template <class ProviderImpl>
void ProviderTracer<ProviderImpl>::addEdgeToBuilder(typename Step::Edge const& edge,
                                                    arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["addEdgeToBuilder"].addTiming(TRI_microtime() - start));
  return _impl.addEdgeToBuilder(edge, builder);
}

template <class ProviderImpl>
void ProviderTracer<ProviderImpl>::destroyEngines() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["destroyEngines"].addTiming(TRI_microtime() - start));
  return _impl.destroyEngines();
}

template <class ProviderImpl>
aql::TraversalStats ProviderTracer<ProviderImpl>::stealStats() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["stealStats"].addTiming(TRI_microtime() - start));
  return _impl.stealStats();
}

template <class ProviderImpl>
transaction::Methods* ProviderTracer<ProviderImpl>::trx() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["trx"].addTiming(TRI_microtime() - start));
  return _impl.trx();
}

template class ::arangodb::graph::ProviderTracer<arangodb::graph::SingleServerProvider>;
template class ::arangodb::graph::ProviderTracer<arangodb::graph::ClusterProvider>;