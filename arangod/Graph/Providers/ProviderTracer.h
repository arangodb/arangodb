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

#ifndef ARANGOD_GRAPH_PROVIDER_TRACER_H
#define ARANGOD_GRAPH_PROVIDER_TRACER_H 1

#include "Graph/EdgeDocumentToken.h"
#include "Graph/Helpers/TraceEntry.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/TypeAliases.h"

#include "Basics/ResourceUsage.h"

#include <unordered_map>
#include <vector>

namespace arangodb {

namespace aql {
class QueryContext;
class TraversalStats;
}

namespace graph {

template <class ProviderImpl>
class ProviderTracer {
 public:
  using Step = typename ProviderImpl::Step;
  using Options = typename ProviderImpl::Options;

 public:
  ProviderTracer(arangodb::aql::QueryContext& queryContext, Options opts,
                 arangodb::ResourceMonitor& resourceMonitor);

  ProviderTracer(ProviderTracer const&) = delete;
  ProviderTracer(ProviderTracer&&) = default;
  ~ProviderTracer();

  ProviderTracer& operator=(ProviderTracer const&) = delete;
  ProviderTracer& operator=(ProviderTracer&&) = default;

  auto startVertex(VertexType vertex) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;                           // rocks
  auto expand(Step const& from, size_t previous, std::function<void(Step)> callback) -> void; // index

  void addVertexToBuilder(typename Step::Vertex const& vertex,
                          arangodb::velocypack::Builder& builder);
  void addEdgeToBuilder(typename Step::Edge const& edge,
                        arangodb::velocypack::Builder& builder);

  // Note: ClusterProvider will need to implement destroyEngines
  void destroyEngines();

  aql::TraversalStats stealStats();

  [[nodiscard]] transaction::Methods* trx();

 private:
  ProviderImpl _impl;

  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable std::unordered_map<std::string, TraceEntry> _stats;
};

}  // namespace graph
}  // namespace arangodb

#endif
