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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "OneSidedEnumeratorInterface.h"

#include "Aql/QueryContext.h"
#include "Graph/algorithm-aliases.h"
#include "Graph/Enumerators/OneSidedEnumerator.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Steps/ClusterProviderStep.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#endif

using namespace arangodb::graph;

#define INSTANTIATE_ENUMERATOR(BaseEnumeratorType, ProviderType,           \
                               VertexUniqueness, EdgeUniqueness)           \
  return std::make_unique<                                                 \
      BaseEnumeratorType<ProviderType, VertexUniqueness, EdgeUniqueness>>( \
      typename BaseEnumeratorType<                                         \
          ProviderType, VertexUniqueness,                                  \
          EdgeUniqueness>::Provider(query, std::move(baseProviderOptions), \
                                    query.resourceMonitor()),              \
      std::move(enumeratorOptions), std::move(pathValidatorOptions),       \
      query.resourceMonitor());

#define GENERATE_UNIQUENESS_SWITCH(BaseEnumeratorType, Provider)   \
  switch (uniqueVertices) {                                        \
    case traverser::TraverserOptions::UniquenessLevel::NONE:       \
      switch (uniqueEdges) {                                       \
        case traverser::TraverserOptions::UniquenessLevel::NONE:   \
          INSTANTIATE_ENUMERATOR(BaseEnumeratorType, Provider,     \
                                 VertexUniquenessLevel::NONE,      \
                                 EdgeUniquenessLevel::NONE)        \
          break;                                                   \
        case traverser::TraverserOptions::UniquenessLevel::PATH:   \
        case traverser::TraverserOptions::UniquenessLevel::GLOBAL: \
          INSTANTIATE_ENUMERATOR(BaseEnumeratorType, Provider,     \
                                 VertexUniquenessLevel::NONE,      \
                                 EdgeUniquenessLevel::PATH)        \
          break;                                                   \
      }                                                            \
      break;                                                       \
    case traverser::TraverserOptions::UniquenessLevel::PATH:       \
      INSTANTIATE_ENUMERATOR(BaseEnumeratorType, Provider,         \
                             VertexUniquenessLevel::PATH,          \
                             EdgeUniquenessLevel::PATH)            \
      break;                                                       \
    case traverser::TraverserOptions::UniquenessLevel::GLOBAL:     \
      INSTANTIATE_ENUMERATOR(BaseEnumeratorType, Provider,         \
                             VertexUniquenessLevel::GLOBAL,        \
                             EdgeUniquenessLevel::PATH)            \
      break;                                                       \
    default:                                                       \
      TRI_ASSERT(false);                                           \
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);                  \
  }

#define GENERATE_ORDER_SWITCH(Provider)                                   \
  switch (order) {                                                        \
    case traverser::TraverserOptions::Order::DFS:                         \
      GENERATE_UNIQUENESS_SWITCH(DFSEnumerator, Provider);                \
      break;                                                              \
    case traverser::TraverserOptions::Order::BFS:                         \
      GENERATE_UNIQUENESS_SWITCH(BFSEnumerator, Provider);                \
      break;                                                              \
    case traverser::TraverserOptions::Order::WEIGHTED:                    \
      GENERATE_UNIQUENESS_SWITCH(WeightedEnumeratorRefactored, Provider); \
      break;                                                              \
  }

#define GENERATE_TRACED_ORDER_SWITCH(Provider)                        \
  switch (order) {                                                    \
    case traverser::TraverserOptions::Order::DFS:                     \
      GENERATE_UNIQUENESS_SWITCH(TracedDFSEnumerator, Provider);      \
      break;                                                          \
    case traverser::TraverserOptions::Order::BFS:                     \
      GENERATE_UNIQUENESS_SWITCH(TracedBFSEnumerator, Provider);      \
      break;                                                          \
    case traverser::TraverserOptions::Order::WEIGHTED:                \
      GENERATE_UNIQUENESS_SWITCH(TracedWeightedEnumerator, Provider); \
      break;                                                          \
  }

template<class ProviderName>
auto TraversalEnumerator::createEnumerator(
    traverser::TraverserOptions::Order order,
    traverser::TraverserOptions::UniquenessLevel uniqueVertices,
    traverser::TraverserOptions::UniquenessLevel uniqueEdges,
    arangodb::aql::QueryContext& query,
    typename ProviderName::Options&& baseProviderOptions,
    arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
    arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
    bool useTracing) -> std::unique_ptr<TraversalEnumerator> {
  if (useTracing) {
    GENERATE_TRACED_ORDER_SWITCH(ProviderName)
  } else {
    GENERATE_ORDER_SWITCH(ProviderName)
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

#define INSTANTIATE_FACTORY(ProviderName)                              \
  template auto TraversalEnumerator::createEnumerator<ProviderName>(   \
      traverser::TraverserOptions::Order order,                        \
      traverser::TraverserOptions::UniquenessLevel uniqueVertices,     \
      traverser::TraverserOptions::UniquenessLevel uniqueEdges,        \
      arangodb::aql::QueryContext & query,                             \
      ProviderName::Options && baseProviderOptions,                    \
      arangodb::graph::PathValidatorOptions && pathValidatorOptions,   \
      arangodb::graph::OneSidedEnumeratorOptions && enumeratorOptions, \
      bool useTracing)                                                 \
      ->std::unique_ptr<TraversalEnumerator>;

INSTANTIATE_FACTORY(
    arangodb::graph::ClusterProvider<arangodb::graph::ClusterProviderStep>)

INSTANTIATE_FACTORY(arangodb::graph::SingleServerProvider<
                    arangodb::graph::SingleServerProviderStep>)

#ifdef USE_ENTERPRISE
INSTANTIATE_FACTORY(arangodb::graph::enterprise::SmartGraphProvider<
                    arangodb::graph::ClusterProviderStep>)
#endif
