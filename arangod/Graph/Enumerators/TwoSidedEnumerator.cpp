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

#include "TwoSidedEnumerator.h"

#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"
#include "Basics/voc-errors.h"

#include "Futures/Future.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/algorithm-aliases.h"

#include <Graph/Types/ValidationResult.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::Ball(
    Direction dir, ProviderType&& provider, GraphOptions const& options,
    arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _interior(resourceMonitor),
      _queue(resourceMonitor),
      _provider(std::move(provider)),
      _validator(_interior),
      _direction(dir),
      _minDepth(options.getMinDepth()) {}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::~Ball() = default;

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::reset(VertexRef center) {
  clear();
  auto firstStep = _provider.startVertex(center);
  _shell.emplace(std::move(firstStep));
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::clear() {
  _shell.clear();
  _interior.reset();
  _queue.clear();
  _depth = 0;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::doneWithDepth() const
    -> bool {
  return _queue.isEmpty();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::noPathLeft() const
    -> bool {
  return doneWithDepth() && _shell.empty();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::getDepth() const
    -> size_t {
  return _depth;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::shellSize() const
    -> size_t {
  return _shell.size();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::startNextDepth()
    -> void {
  // We start the next depth, build a new queue
  // based on the shell contents.
  TRI_ASSERT(_queue.isEmpty());
  for (auto& step : _shell) {
    _queue.append(std::move(step));
  }
  _shell.clear();
  _depth++;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::fetchResults(
    ResultList& results) -> void {
  std::vector<Step*> looseEnds{};

  if (_direction == Direction::FORWARD) {
    for (auto& [step, unused] : results) {
      if (!step.isProcessable()) {
        looseEnds.emplace_back(&step);
      }
    }
  } else {
    for (auto& [unused, step] : results) {
      if (!step.isProcessable()) {
        looseEnds.emplace_back(&step);
      }
    }
  }

  if (!looseEnds.empty()) {
    // Will throw all network errors here
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    futureEnds.get();
    // Notes for the future:
    // Vertices are now fetched. Thnink about other less-blocking and batch-wise
    // fetching (e.g. re-fetch at some later point).
    // TODO: Discuss how to optimize here. Currently we'll mark looseEnds in fetch as fetched.
    // This works, but we might create a batch limit here in the future.
    // Also discuss: Do we want (re-)fetch logic here?
    // TODO: maybe we can combine this with prefetching of paths
  }
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::computeNeighbourhoodOfNextVertex(
    Ball& other, ResultList& results) -> void {
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
  auto previous = _interior.append(step);
  _provider.expand(step, previous, [&](Step n) -> void {
    ValidationResult res = _validator.validatePath(n);

    // Check if other Ball knows this Vertex.
    // Include it in results.
    if ((getDepth() + other.getDepth() >= _minDepth) && !res.isFiltered()) {
      // One side of the path is checked, the other side is unclear:
      // We need to combine the test of both sides.

      // For GLOBAL: We ignore otherValidator, On FIRST match: Add this match as result, clear both sides. => This will give the shortest path.
      // TODO: Check if the GLOBAL holds true for weightedEdges
      other.matchResultsInShell(n, results, _validator);
    }
    if (!res.isPruned()) {
      // Add the step to our shell
      _shell.emplace(std::move(n));
    }
  });
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::testDepthZero(
    Ball& other, ResultList& results) {
  for (auto const& step : _shell) {
    other.matchResultsInShell(step, results, _validator);
  }
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::matchResultsInShell(
    Step const& match, ResultList& results, PathValidator const& otherSideValidator)
    -> void {
  auto [first, last] = _shell.equal_range(match);
  if (_direction == FORWARD) {
    while (first != last) {
      auto res = _validator.validatePath(*first, otherSideValidator);
      if (!res.isFiltered()) {
        LOG_TOPIC("6a01b", DEBUG, Logger::GRAPHS)
            << "Found path " << *first << " and " << match;
        results.push_back(std::make_pair(*first, match));
      }
      first++;
    }
  } else {
    while (first != last) {
      auto res = _validator.validatePath(*first, otherSideValidator);
      if (!res.isFiltered()) {
        LOG_TOPIC("d1830", DEBUG, Logger::GRAPHS)
            << "Found path " << match << " and " << *first;
        results.push_back(std::make_pair(match, *first));
      }
      first++;
    }
  }
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::buildPath(
    Step const& vertexInShell, PathResult<ProviderType, Step>& path) -> void {
  if (_direction == FORWARD) {
    _interior.buildPath(vertexInShell, path);
  } else {
    _interior.reverseBuildPath(vertexInShell, path);
  }
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::Ball::provider()
    -> ProviderType& {
  return _provider;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::TwoSidedEnumerator(
    ProviderType&& forwardProvider, ProviderType&& backwardProvider,
    TwoSidedEnumeratorOptions&& options, arangodb::ResourceMonitor& resourceMonitor)
    : _options(std::move(options)),
      _left{Direction::FORWARD, std::move(forwardProvider), _options, resourceMonitor},
      _right{Direction::BACKWARD, std::move(backwardProvider), _options, resourceMonitor},
      _resultPath{_left.provider(), _right.provider()} {}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::~TwoSidedEnumerator() {
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::destroyEngines()
    -> void {
  // Note: Left & Right Provider use the same traversal engines.
  //   => Destroying one of them is enough.
  _left.provider().destroyEngines();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::clear() {
  _left.clear();
  _right.clear();
  _results.clear();
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::isDone() const {
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
 * @param target The target vertex to end the paths
 */
template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::reset(
    VertexRef source, VertexRef target) {
  _results.clear();
  _left.reset(source);
  _right.reset(target);
  _resultPath.clear();

  // Special depth == 0 case
  if (_options.getMinDepth() == 0 && source == target) {
    _left.testDepthZero(_right, _results);
  }
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
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::getNextPath(VPackBuilder& result) {
  while (!isDone()) {
    searchMoreResults();

    while (!_results.empty()) {
      auto const& [leftVertex, rightVertex] = _results.back();

      // Performance Optimization:
      // It seems to be pointless to first push
      // everything in to the _resultPath object
      // and then iterate again to return the path
      // we should be able to return the path in the first go.
      _resultPath.clear();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);
      TRI_ASSERT(!_resultPath.isEmpty());
      _results.pop_back();
      _resultPath.toVelocyPack(result);
      return true;
    }
  }
  return false;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::searchMoreResults() {
  while (_results.empty() && !searchDone()) {
    _resultsFetched = false;
    if (_searchLeft) {
      if (ADB_UNLIKELY(_left.doneWithDepth())) {
        startNextDepth();
      } else {
        _left.computeNeighbourhoodOfNextVertex(_right, _results);
      }
    } else {
      if (ADB_UNLIKELY(_right.doneWithDepth())) {
        startNextDepth();
      } else {
        _right.computeNeighbourhoodOfNextVertex(_left, _results);
      }
    }
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
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::skipPath() {
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
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::startNextDepth()
    -> void {
  if (_right.shellSize() < _left.shellSize()) {
    _searchLeft = false;
    _right.startNextDepth();
  } else {
    _searchLeft = true;
    _left.startNextDepth();
  }
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::searchDone() const
    -> bool {
  return _left.noPathLeft() || _right.noPathLeft() ||
         _left.getDepth() + _right.getDepth() > _options.getMaxDepth();
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::fetchResults()
    -> void {
  if (!_resultsFetched && !_results.empty()) {
    _left.fetchResults(_results);
    _right.fetchResults(_results);
  }
  _resultsFetched = true;
}

template <class QueueType, class PathStoreType, class ProviderType, class PathValidator>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::stealStats()
    -> aql::TraversalStats {
  aql::TraversalStats stats = _left.provider().stealStats();
  stats += _right.provider().stealStats();
  return stats;
}

/* SingleServerProvider Section */

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::FifoQueue<::arangodb::graph::SingleServerProvider::Step>,
    ::arangodb::graph::PathStore<SingleServerProvider::Step>, SingleServerProvider,
    ::arangodb::graph::PathValidator<PathStore<SingleServerProvider::Step>, VertexUniquenessLevel::PATH>>;

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::graph::SingleServerProvider::Step>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<SingleServerProvider::Step>>,
    ::arangodb::graph::ProviderTracer<SingleServerProvider>,
    ::arangodb::graph::PathValidator<::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<SingleServerProvider::Step>>, VertexUniquenessLevel::PATH>>;

/* ClusterProvider Section */

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::FifoQueue<::arangodb::graph::ClusterProvider::Step>,
    ::arangodb::graph::PathStore<ClusterProvider::Step>, ClusterProvider,
    ::arangodb::graph::PathValidator<PathStore<ClusterProvider::Step>, VertexUniquenessLevel::PATH>>;

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::graph::ClusterProvider::Step>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<ClusterProvider::Step>>,
    ::arangodb::graph::ProviderTracer<ClusterProvider>,
    ::arangodb::graph::PathValidator<::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<ClusterProvider::Step>>, VertexUniquenessLevel::PATH>>;
