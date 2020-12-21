////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/QueueTracer.h"

#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class QueueType, class PathStoreType, class ProviderType>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::Ball(
    Direction dir, ProviderType&& provider, GraphOptions const& options,
    arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _interior(resourceMonitor),
      _queue(resourceMonitor),
      _provider(std::move(provider)),
      _direction(dir),
      _minDepth(options.getMinDepth()) {}

template <class QueueType, class PathStoreType, class ProviderType>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::~Ball() = default;

template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::reset(VertexRef center) {
  clear();
  auto firstStep = _provider.startVertex(center);
  _shell.emplace(std::move(firstStep));
}

template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::clear() {
  _resourceMonitor.decreaseMemoryUsage(sizeof(Step) * _shell.size());
  _shell.clear();
  _interior.reset();
  _queue.clear();
  _depth = 0;
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::doneWithDepth() const
    -> bool {
  return _queue.isEmpty();
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::noPathLeft() const
    -> bool {
  return doneWithDepth() && _shell.empty();
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::getDepth() const
    -> size_t {
  return _depth;
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::shellSize() const
    -> size_t {
  return _shell.size();
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::startNextDepth()
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

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::fetchResults(ResultList& results)
    -> void {
  std::vector<Step*> looseEnds{};
#if 0
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
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.get();
    LOG_DEVEL << "needs to be implemented: " << preparedEnds;
    // TODO: we need to ensure that we now have all vertices fetched - (future - not yet implemented and not relevant yet)
    // or that we need to refetch at some later point.
    // TODO: maybe we can combine this with prefetching of paths
  }
#endif
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::computeNeighbourhoodOfNextVertex(
    Ball const& other, ResultList& results) -> void {
  // Pull next element from Queue
  // Do 1 step search
  TRI_ASSERT(!_queue.isEmpty());
  if (!_queue.hasProcessableElement()) {
    std::vector<Step*> looseEnds = _queue.getLooseEnds();
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.get();
    LOG_DEVEL << "needs to be implemented: " << preparedEnds;
    // TODO we somehow need to handover those looseends to - (future - not yet implemented and not relevant yet)
    // the queue again, in order to remove them from the loosend list.
  }
  auto step = _queue.pop();
  auto previous = _interior.append(step);
  _provider.expand(step, previous, [&](Step n) -> void {
    // Check if other Ball knows this Vertex.
    // Include it in results.
    if (getDepth() + other.getDepth() >= _minDepth) {
      other.matchResultsInShell(n, results);
    }
    LOG_TOPIC("9620b", TRACE, Logger::GRAPHS) << "  Neighbor " << n;
    // Add the step to our shell
    _resourceMonitor.increaseMemoryUsage(sizeof(n));
    _shell.emplace(std::move(n));
  });
}

template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::testDepthZero(
    Ball const& other, ResultList& results) const {
  for (auto const& step : _shell) {
    other.matchResultsInShell(step, results);
  }
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::matchResultsInShell(
    Step const& match, ResultList& results) const -> void {
  auto [first, last] = _shell.equal_range(match);
  if (_direction == FORWARD) {
    while (first != last) {
      LOG_TOPIC("6a01b", DEBUG, Logger::GRAPHS)
          << "Found path " << *first << " and " << match;
      results.push_back(std::make_pair(*first, match));
      first++;
    }
  } else {
    while (first != last) {
      LOG_TOPIC("d1830", DEBUG, Logger::GRAPHS)
          << "Found path " << match << " and " << *first;
      results.push_back(std::make_pair(match, *first));
      first++;
    }
  }
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::buildPath(
    Step const& vertexInShell, PathResult<ProviderType, Step>& path) -> void {
  if (_direction == FORWARD) {
    _interior.buildPath(vertexInShell, path);
  } else {
    _interior.reverseBuildPath(vertexInShell, path);
  }
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::Ball::provider()
    -> ProviderType& {
  return _provider;
}

template <class QueueType, class PathStoreType, class ProviderType>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::TwoSidedEnumerator(
    ProviderType&& forwardProvider, ProviderType&& backwardProvider,
    TwoSidedEnumeratorOptions&& options, arangodb::ResourceMonitor& resourceMonitor)
    : _options(std::move(options)),
      _left{Direction::FORWARD, std::move(forwardProvider), _options, resourceMonitor},
      _right{Direction::BACKWARD, std::move(backwardProvider), _options, resourceMonitor},
      _resultPath{_left.provider(), _right.provider()} {}

template <class QueueType, class PathStoreType, class ProviderType>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::~TwoSidedEnumerator() {}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::destroyEngines()
    -> void {
  // Note: Left & Right Provider use the same traversal engines.
  //   => Destroying one of them is enough.
  _left.provider().destroyEngines();
}

template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::clear() {
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
template <class QueueType, class PathStoreType, class ProviderType>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::isDone() const {
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
template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::reset(VertexRef source,
                                                                       VertexRef target) {
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
template <class QueueType, class PathStoreType, class ProviderType>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::getNextPath(VPackBuilder& result) {
  while (!isDone()) {
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

    while (!_results.empty()) {
      auto const& [leftVertex, rightVertex] = _results.back();

      _resultPath.clear();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);

      // result done
      _results.pop_back();
      if (_resultPath.isValid()) {
        _resultPath.toVelocyPack(result);
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template <class QueueType, class PathStoreType, class ProviderType>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::skipPath() {
  return false;
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::startNextDepth()
    -> void {
  if (_right.shellSize() < _left.shellSize()) {
    _searchLeft = false;
    _right.startNextDepth();
  } else {
    _searchLeft = true;
    _left.startNextDepth();
  }
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::searchDone() const
    -> bool {
  return _left.noPathLeft() || _right.noPathLeft() ||
         _left.getDepth() + _right.getDepth() > _options.getMaxDepth();
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::fetchResults() -> void {
  if (!_resultsFetched && !_results.empty()) {
    _left.fetchResults(_results);
    _right.fetchResults(_results);
  }
  _resultsFetched = true;
}

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::FifoQueue<::arangodb::graph::SingleServerProvider::Step>,
    ::arangodb::graph::PathStore<SingleServerProvider::Step>, SingleServerProvider>;

template class ::arangodb::graph::TwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::FifoQueue<::arangodb::graph::SingleServerProvider::Step>>,
    ::arangodb::graph::PathStoreTracer<::arangodb::graph::PathStore<SingleServerProvider::Step>>,
    ::arangodb::graph::ProviderTracer<SingleServerProvider>>;
