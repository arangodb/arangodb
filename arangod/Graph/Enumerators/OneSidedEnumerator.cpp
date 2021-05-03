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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "OneSidedEnumerator.h"

#include "Basics/debugging.h"
#include "Basics/system-compiler.h"

#include "Futures/Future.h"
#include "Graph/Options/OneSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/QueueTracer.h"

#include <Graph/Types/ValidationResult.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::OneSidedEnumerator(
    ProviderType&& forwardProvider, OneSidedEnumeratorOptions&& options,
    arangodb::ResourceMonitor& resourceMonitor)
    : _options(std::move(options)),
      _queue(resourceMonitor),
      _provider(std::move(forwardProvider)),
      _validator(_interior),
      _interior(resourceMonitor),
      _resultPath{_provider, _provider} {}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::~OneSidedEnumerator() {
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::destroyEngines()
    -> void {
  _provider.destroyEngines();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::clear() {
  _interior.reset();
  _queue.clear();
  _currentDepth = 0;
  _results.clear();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::computeNeighbourhoodOfNextVertex()
    -> void {
  // Pull next element from Queue
  // Do 1 step search
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
  auto posPrevious = _interior.append(step);

  if (!step.isFirst()) {
    ValidationResult res = _validator.validatePath(step);
    if ((step.getDepth() >= _options.getMinDepth()) && !res.isFiltered() && !res.isPruned()) {
      // Include it in results.
      _results.push_back(std::make_pair(_interior.get(posPrevious), step));
    }
  }

  if (step.getDepth() < _options.getMaxDepth()) {
    // DFS Path Check (will need different check for BFS traversal - global depth vs. specific step depth)
    _provider.expand(step, posPrevious, [&](Step n) -> void {
      _queue.append(n); });
  }
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
bool OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::isDone() const {
  return _results.empty() && searchDone();
}

/**
 * @brief Reset to new source and target vertices.
 * This API uses string references, this class will not take responsibility
 * for the referenced data. It is caller's responsibility to retain the
 * underlying data and make sure the StringRefs stay valid until next
 * call of reset.
 *
 * @param source The source vertex to start the paths
 */
template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::reset(VertexRef source) {
  clear();
  auto firstStep = _provider.startVertex(source);
  _queue.append(std::move(firstStep));
}

/**
 * @brief Get the next path, if available written into the result build.
 * The given builder will be not be cleared, this function requires a
 * prepared builder to write into.
 *
 * @param result Input and output, this needs to be an open builder,
 * where the path can be placed into.
 * Can be empty, or an openArray, or the value of an object.
 * Guarantee: Every returned path matches the conditions handed in via
 * options. No path is returned twice, it is intended that paths overlap.
 * @return true Found and written a path, result is modified.
 * @return false No path found, result has not been changed.
 */
template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
bool OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::getNextPath(VPackBuilder& result) {
  while (!isDone()) {
    searchMoreResults();

    while (!_results.empty()) {
      auto const& [unused, rightVertex] = _results.back();

      // Performance Optimization:
      // It seems to be pointless to first push
      // everything in to the _resultPath object
      // and then iterate again to return the path
      // we should be able to return the path in the first go.
      _resultPath.clear();
      _interior.buildPath(rightVertex, _resultPath);
      TRI_ASSERT(!_resultPath.isEmpty());
      _results.pop_back();
      _resultPath.toVelocyPack(result);
      return true;
    }
  }
  return false;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::searchMoreResults() {
  while (_results.empty() && !searchDone()) { // TODO: check && !_queue.isEmpty()
    _resultsFetched = false;
    computeNeighbourhoodOfNextVertex();
  }

  fetchResults();
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
bool OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::skipPath() {
  while (!isDone()) {
    searchMoreResults();

    while (!_results.empty()) {
      // just drop one result for skipping
      _results.pop_back();
      return true;
    }
  }
  return false;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::searchDone() const
    -> bool {
  return _queue.isEmpty();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::fetchResults()
    -> void {
  if (!_resultsFetched && !_results.empty()) {
    std::vector<Step*> looseEnds{};

    for (auto& [from, to] : _results) {
      // TODO: Check ifs (discuss direction<->forward/backward)
      TRI_ASSERT(from.isProcessable());
      if (!from.isProcessable()) {
        looseEnds.emplace_back(&from);
      }
      if (!to.isProcessable()) {
        looseEnds.emplace_back(&to);
      }
    }

    if (!looseEnds.empty()) {
      // Will throw all network errors here
      futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
      futureEnds.get();
      // Notes for the future:
      // Vertices are now fetched. Think about other less-blocking and
      // batch-wise fetching (e.g. re-fetch at some later point).
      // TODO: Discuss how to optimize here. Currently we'll mark looseEnds in
      // fetch as fetched. This works, but we might create a batch limit here in
      // the future. Also discuss: Do we want (re-)fetch logic here?
      // TODO: maybe we can combine this with prefetching of paths
    }
  }

  _resultsFetched = true;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto OneSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::stealStats()
    -> aql::TraversalStats {
  aql::TraversalStats stats = _provider.stealStats();
  return stats;
}

/* SingleServerProvider Section */

template class ::arangodb::graph::OneSidedEnumerator<
    ::arangodb::graph::FifoQueue<::arangodb::graph::SingleServerProvider::Step>,
    ::arangodb::graph::PathStore<SingleServerProvider::Step>, SingleServerProvider,
    ::arangodb::graph::PathValidator<PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::PATH>>;

template class ::arangodb::graph::OneSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::graph::SingleServerProvider::Step>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<SingleServerProvider::Step>>,
    ::arangodb::graph::ProviderTracer<SingleServerProvider>,
    ::arangodb::graph::PathValidator<::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<SingleServerProvider::Step>>, VertexUniquenessLevel::PATH>>;