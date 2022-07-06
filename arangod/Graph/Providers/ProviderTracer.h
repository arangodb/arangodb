////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Graph/EdgeDocumentToken.h"
#include "Graph/Helpers/TraceEntry.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/TypeAliases.h"

#include "Basics/ResourceUsage.h"

#include "Containers/FlatHashMap.h"

#include <vector>

namespace arangodb {

namespace aql {
class QueryContext;
class TraversalStats;
}  // namespace aql

namespace graph {

template<class ProviderImpl>
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

  auto startVertex(VertexType vertex, size_t depth = 0, double weight = 0.0)
      -> Step;
  auto fetchVertices(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;
  auto fetchEdges(const std::vector<Step*>& fetchedVertices) -> Result;
  auto fetch(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;
  auto expand(Step const& from, size_t previous,
              std::function<void(Step)> callback) -> void;

  auto clear() -> void;

  void addVertexToBuilder(typename Step::Vertex const& vertex,
                          arangodb::velocypack::Builder& builder);
  void addEdgeToBuilder(typename Step::Edge const& edge,
                        arangodb::velocypack::Builder& builder);
  void addEdgeIDToBuilder(typename Step::Edge const& edge,
                          arangodb::velocypack::Builder& builder);

  void addEdgeToLookupMap(typename Step::Edge const& edge,
                          arangodb::velocypack::Builder& builder);

  auto getEdgeId(typename Step::Edge const& edge) -> std::string;

  auto getEdgeIdRef(typename Step::Edge const& edge) -> EdgeType;

  void prepareIndexExpressions(aql::Ast* ast);

  // Note: ClusterProvider will need to implement destroyEngines
  void destroyEngines();

  aql::TraversalStats stealStats();

  [[nodiscard]] transaction::Methods* trx();
  [[nodiscard]] TRI_vocbase_t const& vocbase() const;

  void prepareContext(aql::InputAqlItemRow input);
  void unPrepareContext();
  bool isResponsible(Step const& step) const;
  [[nodiscard]] bool hasDepthSpecificLookup(uint64_t depth) const noexcept;

 private:
  ProviderImpl _impl;

  // Mapping MethodName => Statistics
  // We make this mutable to not violate the captured API
  mutable containers::FlatHashMap<std::string, TraceEntry> _stats;
};

}  // namespace graph
}  // namespace arangodb
