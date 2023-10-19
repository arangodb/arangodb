///////////////////////////////////////////////////////////////////////////////
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Graph/Enumerators/Dijkstra.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/Queues/WeightedQueue.h"
#include "Graph/Steps/ClusterProviderStep.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"

namespace arangodb::graph::dijkstra {

/*
 * Class: Ball (internally used in Dijkstra)
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
Dijkstra<QueueType, PathStoreType, ProviderType, PathValidator>::Dijkstra(
    ProviderType&& provider, PathValidatorOptions validatorOptions,
    arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _interior(resourceMonitor),
      _queue(resourceMonitor),
      _provider(std::move(provider)),
      _validator(_provider, _interior, std::move(validatorOptions)) {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
Dijkstra<QueueType, PathStoreType, ProviderType, PathValidator>::~Dijkstra() =
    default;

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void Dijkstra<QueueType, PathStoreType, ProviderType, PathValidator>::reset(
    VertexRef center) {
  clear();
  auto firstStep = _provider.startVertex(center, 0);
  _queue.append(std::move(firstStep));
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void Dijkstra<QueueType, PathStoreType, ProviderType, PathValidator>::clear() {
  _visitedNodes.clear();

  _queue.clear();
  _interior.reset();

  // Provider - Must be last one to be cleared(!)
  clearProvider();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::clearProvider() {
  // We need to make sure, no one holds references to _provider.
  // Guarantee that the used Queue is empty and we do not hold any reference to
  // PathStore. Info: Steps do contain VertexRefs which are hold in PathStore.
  TRI_ASSERT(_queue.isEmpty());

  // Guarantee that the used PathStore is cleared, before we clear the Provider.
  // The Provider does hold the StringHeap cache.
  TRI_ASSERT(_interior.size() == 0);

  _provider.clear();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::doneWithDepth() const -> bool {
  return _queue.isEmpty();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::noPathLeft() const -> bool {
  return doneWithDepth();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::peekQueue() const -> Step const& {
  return _queue.peek();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::isQueueEmpty() const -> bool {
  return _queue.isEmpty();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::hasBeenVisited(Step const& step) -> bool {
  return _visitedNodes.contains(step.getVertex().getID()));
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType,
              PathValidator>::computeNeighbourhoodOfNextVertex() -> double {
  TRI_ASSERT(!_queue.isEmpty());
  if (!_queue.hasProcessableElement()) {
    std::vector<Step*> looseEnds = _queue.getLooseEnds();
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);

    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.get();

    TRI_ASSERT(preparedEnds.size() != 0);
    TRI_ASSERT(_queue.hasProcessableElement());
  }

  auto step = _queue.pop();
  auto previous = _interior.append(step);

  if (hasBeenVisited(step)) {
    return -1.0;
  }

  double matchPathWeight = -1.0;

  TRI_ASSERT(!_visitedNodes.contains(step.getVertex().getID()));
  _visitedNodes.emplace(step.getVertex().getID(), std::vector{previous});

  _provider.expand(step, previous, [&](Step n) -> void {
    ValidationResult res = _validator.validatePath(n);

    if (!res.isFiltered() && !res.isPruned()) {
      // Add the step to our shell
      _queue.append(std::move(n));
    }
  });

  return matchPathWeight;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType, PathValidator>::buildPath(
    Step const& vertexInShell, PathResult<ProviderType, Step>& path) -> void {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto Dijkstra<QueueType, PathStoreType, ProviderType, PathValidator>::provider()
    -> ProviderType& {
  return _provider;
}

}  // namespace arangodb::graph::dijkstra

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::dijkstra::Dijkstra<
    ::arangodb::graph::WeightedQueue<SingleServerProviderStep>,
    ::arangodb::graph::PathStore<SingleServerProviderStep>,
    ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>,
        ::arangodb::graph::PathStore<SingleServerProviderStep>,
        ::arangodb::graph::VertexUniquenessLevel::PATH,
        ::arangodb::graph::EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::dijkstra::Dijkstra<
    ::arangodb::graph::QueueTracer<
        ::arangodb::graph::WeightedQueue<SingleServerProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<SingleServerProviderStep>>,
    ::arangodb::graph::ProviderTracer<
        ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::ProviderTracer<
            ::arangodb::graph::SingleServerProvider<SingleServerProviderStep>>,
        ::arangodb::graph::PathStoreTracer<
            ::arangodb::graph::PathStore<SingleServerProviderStep>>,
        ::arangodb::graph::VertexUniquenessLevel::PATH,
        ::arangodb::graph::EdgeUniquenessLevel::PATH>>;

/* ClusterProvider Section */

template class ::arangodb::graph::dijkstra::Dijkstra<
    ::arangodb::graph::WeightedQueue<::arangodb::graph::ClusterProviderStep>,
    ::arangodb::graph::PathStore<::arangodb::graph::ClusterProviderStep>,
    ::arangodb::graph::ClusterProvider<::arangodb::graph::ClusterProviderStep>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::ClusterProvider<
            ::arangodb::graph::ClusterProviderStep>,
        ::arangodb::graph::PathStore<::arangodb::graph::ClusterProviderStep>,
        ::arangodb::graph::VertexUniquenessLevel::PATH,
        ::arangodb::graph::EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::dijkstra::Dijkstra<
    ::arangodb::graph::QueueTracer<::arangodb::graph::WeightedQueue<
        ::arangodb::graph::ClusterProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<::arangodb::graph::ClusterProviderStep>>,
    ::arangodb::graph::ProviderTracer<::arangodb::graph::ClusterProvider<
        ::arangodb::graph::ClusterProviderStep>>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::ProviderTracer<::arangodb::graph::ClusterProvider<
            ::arangodb::graph::ClusterProviderStep>>,
        ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<
            ::arangodb::graph::ClusterProviderStep>>,
        ::arangodb::graph::VertexUniquenessLevel::PATH,
        ::arangodb::graph::EdgeUniquenessLevel::PATH>>;
