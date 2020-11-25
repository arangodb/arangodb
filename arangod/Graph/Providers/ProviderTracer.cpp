////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/Providers/SingleServerProvider.h"

#include <iomanip>

using namespace arangodb;
using namespace arangodb::graph;

TraceEntry::TraceEntry() {}
TraceEntry::~TraceEntry() = default;
void TraceEntry::addTiming(double timeTaken) {
  _count++;
  _total += timeTaken;
  if (_min > timeTaken) {
    _min = timeTaken;
  }
  if (_max < timeTaken) {
    _max = timeTaken;
  }
}

namespace arangodb {
namespace graph {
auto operator<<(std::ostream& out, TraceEntry const& entry) -> std::ostream& {
  if (entry._count == 0) {
    out << "not called";
  } else {
    out << "calls: " << entry._count << " min: " << std::setprecision(2)
        << std::fixed << entry._min / 1000 << "ms max: " << entry._max / 1000
        << "ms avg: " << entry._total / entry._count / 1000
        << "ms total: " << entry._total / 1000 << "ms";
  }
  return out;
}
}  // namespace graph
}  // namespace arangodb

template <class ProviderImpl>
ProviderTracer<ProviderImpl>::ProviderTracer(arangodb::aql::QueryContext& queryContext,
                                             BaseProviderOptions opts)
    : _impl{queryContext, opts} {}

template <class ProviderImpl>
ProviderTracer<ProviderImpl>::~ProviderTracer() {
  LOG_TOPIC("f39e8", TRACE, Logger::GRAPHS) << "Provider Trace report:";
  for (auto const& [name, trace] : _stats) {
    LOG_TOPIC("f39e9", TRACE, Logger::GRAPHS) << "  " << name << ": " << trace;
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
std::vector<typename ProviderImpl::Step> ProviderTracer<ProviderImpl>::expand(Step const& from,
                                                                              size_t previous) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["expand"].addTiming(TRI_microtime() - start));
  return _impl.expand(from, previous);
}

template <class ProviderImpl>
void ProviderTracer<ProviderImpl>::insertVertexIntoResult(VertexType vertex,
                                                          arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["insertVertexIntoResult"].addTiming(TRI_microtime() - start));
  return _impl.insertVertexIntoResult(vertex, builder);
}

template <class ProviderImpl>
void ProviderTracer<ProviderImpl>::insertEdgeIntoResult(EdgeDocumentToken edge,
                                                        arangodb::velocypack::Builder& builder) {
  double start = TRI_microtime();
  TRI_DEFER(_stats["insertEdgeIntoResult"].addTiming(TRI_microtime() - start));
  return _impl.insertEdgeIntoResult(edge, builder);
}

template <class ProviderImpl>
transaction::Methods* ProviderTracer<ProviderImpl>::trx() {
  double start = TRI_microtime();
  TRI_DEFER(_stats["trx"].addTiming(TRI_microtime() - start));
  return _impl.trx();
}

template class ::arangodb::graph::ProviderTracer<arangodb::graph::SingleServerProvider>;