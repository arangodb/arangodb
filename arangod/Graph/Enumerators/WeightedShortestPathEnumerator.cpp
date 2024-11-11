////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "WeightedShortestPathEnumerator.h"

#include "Basics/debugging.h"
#include "Basics/system-compiler.h"

#include "Futures/Future.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"
#include "Graph/algorithm-aliases.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <limits>

namespace arangodb::graph {

/*
 * Class: Ball (internally used in WeightedShortestPathEnumerator)
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedShortestPathEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::Ball(Direction dir, ProviderType&& provider,
                               GraphOptions const& options,
                               PathValidatorOptions validatorOptions,
                               arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor),
      _interior(resourceMonitor),
      _queue(resourceMonitor),
      _provider(std::move(provider)),
      _validator(_provider, _interior, std::move(validatorOptions)),
      _direction(dir),
      _graphOptions(options),
      _diameter(-std::numeric_limits<double>::infinity()) {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                               PathValidator>::Ball::~Ball() {
  clear();
  clearProvider();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::reset(VertexRef
                                                                    center,
                                                                size_t depth) {
  clear();
  _center = center;
  auto firstStep = _provider.startVertex(center, depth);
  _queue.append(std::move(firstStep));
  _queued = 1;
  _expanded = 0;
  _foundVertices.emplace(center, VertexInfo(0.0));
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::clear() {
  _foundVertices.clear();
  _queue.clear();
  _interior.reset();  // PathStore
  _diameter = -std::numeric_limits<double>::infinity();
  _validator.reset();
  // We do not clear the provider here, or else it would immediately
  // clear all its caches. For repeated calls we want to retain the caches.
  // The provider is only cleared in the destructor.
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::clearProvider() {
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
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::isQueueEmpty() const
    -> bool {
  return _queue.isEmpty();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::fetchResult(CalculatedCandidate& candidate) -> void {
  std::vector<Step*> looseEnds{};
  auto& [weight, leftMeetingPoint, rightMeetingPoint] = candidate;

  if (_direction == Direction::FORWARD) {
    auto& step = leftMeetingPoint;
    if (!step.isProcessable()) {
      looseEnds.emplace_back(&step);
    }

  } else {
    auto& step = rightMeetingPoint;
    if (!step.isProcessable()) {
      looseEnds.emplace_back(&step);
    }
  }

  if (!looseEnds.empty()) {
    // Will throw all network errors here
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    futureEnds.waitAndGet();
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::hasBeenVisited(Step const& step) -> bool {
  auto it = _foundVertices.find(step.getVertex().getID());
  if (it == _foundVertices.end()) {
    return false;
  }
  return it->second.expanded;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::ensureQueueHasProcessableElement() -> void {
  TRI_ASSERT(!_queue.isEmpty());
  if (!_queue.hasProcessableElement()) {
    std::vector<Step*> looseEnds = _queue.getLooseEnds();
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);

    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.waitAndGet();

    TRI_ASSERT(preparedEnds.size() != 0);
  }
  TRI_ASSERT(_queue.hasProcessableElement());
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::
    validateSingletonPath(std::optional<CalculatedCandidate>& bestPath)
        -> void {
  ensureQueueHasProcessableElement();
  auto tmp = _queue.pop();

  TRI_ASSERT(_queue.isEmpty());

  auto posPrevious = _interior.append(std::move(tmp));
  auto& step = _interior.getStepReference(posPrevious);
  ValidationResult res = _validator.validatePath(step);

  if (!res.isFiltered()) {
    TRI_ASSERT(!bestPath.has_value());
    bestPath = std::make_tuple(0.0, step, step);
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::
    computeNeighbourhoodOfNextVertex(
        Ball& other, std::optional<CalculatedCandidate>& bestPath) -> void {
  ensureQueueHasProcessableElement();
  auto tmp = _queue.pop();

  auto posPrevious = _interior.append(std::move(tmp));
  auto& step = _interior.getStepReference(posPrevious);

  TRI_ASSERT(step.getWeight() >= _diameter);
  _diameter = step.getWeight();
  ValidationResult res = _validator.validatePath(step);

  if (!res.isPruned() && step.getVertex().getID() != other.getCenter()) {
    auto it = _foundVertices.find(step.getVertex().getID());
    TRI_ASSERT(it != _foundVertices.end());
    if (it->second.cancelled) {
      // This happens if we have later found a shorter path to the vertex
      // and have still not queued the cheaper Step, for example, because
      // the other side has already expanded the vertex. This is a performance
      // optimization.
      return;
    }
    it->second.position = posPrevious;
    it->second.expanded = true;
    ++_expanded;
    // We do not want to go further than the center of the other side!
    _provider.expand(step, posPrevious, [&](Step n) -> void {
      // We check for
      if (_forbiddenVertices != nullptr &&
          _forbiddenVertices->contains(n.getVertex().getID())) {
        return;
      }
      if (_forbiddenEdges != nullptr &&
          _forbiddenEdges->contains(n.getEdge().getID())) {
        return;
      }
      auto reachedIt = _foundVertices.find(n.getVertex().getID());
      bool needToQueue = true;
      bool weightReduced = false;
      if (reachedIt == _foundVertices.end()) {
        reachedIt =
            _foundVertices
                .emplace(n.getVertex().getID(), VertexInfo(n.getWeight()))
                .first;
      } else if (reachedIt->second.weight > n.getWeight()) {
        // Reduce the weight of the vertex, note that the old Step will
        // still be queued with the higher weight, but we will queue it
        // again below, so the one with the smaller weight will come first
        // on the queue and will eventually be expanded.
        reachedIt->second.weight = n.getWeight();
        weightReduced = true;
      } else {
        // We have already reached this vertex with at most this weight.
        needToQueue = false;
      }

      if (other.hasBeenVisited(n)) {
        // If the other side has already expanded the vertex, we do not need
        // to queue it, since we do not have to expand it.
        needToQueue = false;
        // Need to validate this step, too:
        ValidationResult res =
            _validator.validatePathWithoutGlobalVertexUniqueness(n);
        // Note that we must not fully enforce uniqueness here for the
        // following reason: If vertex uniqueness is set to global,
        // then we would burn that vertex (which belongs to the
        // other side!), so that we can no longer reach it with a
        // different path, which might have a smaller weight.
        if (!(res.isFiltered() || res.isPruned())) {
          other.matchResultsInShell(n, bestPath, _validator);
        }
      }
      if (needToQueue) {
        // If the other side has already expanded the vertex, we do not
        // have to put it on our queue. But if not, we must look at it
        // later:
        _queue.append(std::move(n));
        _queued++;
        reachedIt->second.cancelled = false;  // Make sure we expand the vertex
      } else if (weightReduced) {
        reachedIt->second.cancelled = true;
      }
    });
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::
    matchResultsInShell(Step const& otherStep,
                        std::optional<CalculatedCandidate>& bestPath,
                        PathValidator const& otherSideValidator) -> void {
  auto it = _foundVertices.find(otherStep.getVertex().getID());
  TRI_ASSERT(it != _foundVertices.end());
  TRI_ASSERT(it->second.expanded);
  auto position = it->second.position;

  auto ourStep = _interior.getStepReference(position);

  auto res = _validator.validatePath(ourStep, otherSideValidator);
  if (!(res.isFiltered() || res.isPruned())) {
    double nextFullPathWeight = ourStep.getWeight() + otherStep.getWeight();
    if (!bestPath.has_value() ||
        nextFullPathWeight < std::get<0>(bestPath.value())) {
      if (_direction == FORWARD) {
        bestPath = std::make_tuple(nextFullPathWeight, ourStep, otherStep);
      } else {
        bestPath = std::make_tuple(nextFullPathWeight, otherStep, ourStep);
      }
    }
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::buildPath(Step const& vertexInShell,
                                    PathResult<ProviderType, Step>& path)
    -> void {
  if (_direction == FORWARD) {
    _interior.buildPath(vertexInShell, path);
  } else {
    _interior.reverseBuildPath(vertexInShell, path);
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::Ball::provider()
    -> ProviderType& {
  return _provider;
}

/*
 * Class: WeightedShortestPathEnumerator
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                               PathValidator>::
    WeightedShortestPathEnumerator(ProviderType&& forwardProvider,
                                   ProviderType&& backwardProvider,
                                   TwoSidedEnumeratorOptions&& options,
                                   PathValidatorOptions validatorOptions,
                                   arangodb::ResourceMonitor& resourceMonitor)
    : _options(std::move(options)),
      _left{Direction::FORWARD, std::move(forwardProvider), _options,
            validatorOptions, resourceMonitor},
      _right{Direction::BACKWARD, std::move(backwardProvider), _options,
             std::move(validatorOptions), resourceMonitor},
      _resultPath{_left.provider(), _right.provider()} {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedShortestPathEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::~WeightedShortestPathEnumerator() {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::destroyEngines() -> void {
  // Note: Left & Right Provider use the same traversal engines.
  //   => Destroying one of them is enough.
  _left.provider().destroyEngines();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::clear() {
  // Order is important here, please do not change.
  // 1.) Remove current results & state
  _bestPath.reset();

  // 2.) Remove both Balls (order here is not important)
  _left.clear();
  _right.clear();

  // 3.) Remove finished state
  setAlgorithmUnfinished();
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::isDone() const {
  TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
  return !_bestPath.has_value() && searchDone();
}

/**
 * @brief Reset to new source and target vertices.
 * This API uses string references, this class will not take responsibility
 * for the referenced data. It is caller's responsibility to retain the
 * underlying data and make sure the strings stay valid until next
 * call of reset.
 *
 * @param source The source vertex to start the paths
 * @param target The target vertex to end the paths
 */
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::reset(VertexRef source,
                                                          VertexRef target,
                                                          size_t depth) {
  clear();

  _left.reset(source, 0);

  // TODO: this is not ideal; here's the issue: If source == target there is
  // no search to be done as there is only *at most* one shortest path between
  // a vertex and itself: the path of length and weight 0. If the vertex does
  // not fulfill the global vertex condition, there is none. So the global
  // vertex condition has to be evaluated! This is why the _left ball is used
  // here.
  //
  // Admittedly, this choice is arbitrary: in our context a path is a sequence
  // of edges that does not repeat vertices. Otherwise this path search would
  // have to return all cycles based at the source == target vertex. This can
  // be implemented using a OneSidedEnumerator if ever requested.
  if (source == target) {
    _singleton = true;
    _right.clear();
  } else {
    _singleton = false;
    _right.reset(target, 0);
  }
  _resultPath.clear();
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
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::getNextPath(VPackBuilder&
                                                                    result) {
  auto handleResult = [&]() {
    // Helper method to take care of a found result:
    if (!isAlgorithmFinished() || !_bestPath.has_value()) {
      return false;
    }
    auto const& [weight, leftVertex, rightVertex] = _bestPath.value();

    _resultPath.clear();
    _left.buildPath(leftVertex, _resultPath);
    _right.buildPath(rightVertex, _resultPath);
    TRI_ASSERT(!_resultPath.isEmpty());

    if (_emitWeight) {
      // Add weight attribute to edges
      _resultPath.toVelocyPack(
          result, PathResult<ProviderType, Step>::WeightType::ACTUAL_WEIGHT);
    } else {
      _resultPath.toVelocyPack(result);
    }
    _bestPath.reset();
    return true;
  };

  auto report = [&]() {
    LOG_TOPIC("47010", TRACE, Logger::GRAPHS)
        << "Yen: Left: expanded " << _left.getExpanded()
        << " queued: " << _left.getQueued() << " Right: expanded "
        << _right.getExpanded() << " queued: " << _right.getQueued();
  };

  while (!isDone()) {
    if (!searchDone()) {
      searchMoreResults();
    }

    TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
    if (handleResult()) {
      TRI_ASSERT(_options.onlyProduceOnePath());
      // At this state we've produced a valid path result. In case we're
      // using the path type "(Weighted)ShortestPath", the algorithm is
      // finished. We need to store this information.
      report();
      return true;
    }
  }

  TRI_ASSERT(isDone());
  report();
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::searchMoreResults() {
  while (!searchDone()) {
    _resultsFetched = false;

    if (_singleton) {
      _left.validateSingletonPath(_bestPath);
      setAlgorithmFinished();
    } else {
      auto searchLocation = getBallToContinueSearch();
      if (searchLocation == BallSearchLocation::LEFT) {
        _left.computeNeighbourhoodOfNextVertex(_right, _bestPath);
      } else if (searchLocation == BallSearchLocation::RIGHT) {
        _right.computeNeighbourhoodOfNextVertex(_left, _bestPath);
      } else {
        TRI_ASSERT(searchLocation == BallSearchLocation::FINISH);
        // Our queue is empty. We cannot produce more results.
        setAlgorithmFinished();
      }
    }

    if (_bestPath.has_value()) {
      auto bestWeight = std::get<0>(_bestPath.value());
      // If the sum of the diameters of left and right search is
      // at least as big as the best candidate, there will not be a better
      // candidate found. This is because of the following: The "diameter"
      // of a ball is the weight d of the next vertex to be expanded. This
      // means that we already know that the shortest paths to that vertex
      // have weight d. All vertices with a lower weight have already
      // been expanded (and potentially some of weight d).
      // Now assume that we have d1 and d2 as diameters of the left and
      // right search, respectively, and we have found some path with
      // weight w <= d1 + d2 and that was the best we have found so far.
      // We claim that no shorter path will be found, so we can stop.
      // Proof:
      // Assume there is a shortest path P with weight w' < w. Then w' < d1+d2
      // in particular. Since P is a shortest path, all weights on P are
      // minimal possible. Then all vertices on P which are less than d1
      // away from the start vertex have already been found and expanded
      // by the left hand side. Likewise, all vertices on P which are less
      // than d2 away from the target vertex have already been found and
      // expanded by the right hand side. Since w' < d1+d2, there is no
      // "gap" between the two sides: There cannot be a vertex on the path,
      // which is both at least d1 from the start and at least d2 from the
      // target. It might even be that some vertex on the path P has been
      // expanded by both sides. In any case, there must be an edge on the
      // path, so that the source of the edge has been expanded by the
      // left hand side and the target of the edge has been expanded
      // by the right hand side. Then one of these expansions has to have
      // happened first and the other must have seen this path,
      // a contradiction!
      // In this case, a simple shortest path search is done *now*.

      auto leftDiameter = _left.getDiameter();
      auto rightDiameter = _right.getDiameter();
      auto sumDiameter = leftDiameter + rightDiameter;

      if (sumDiameter >= bestWeight) {
        setAlgorithmFinished();
      }

      // There is another case, in which we are done: If one of the
      // sides has finished in the sense that its queue is empty, and we
      // have actually found some path, then we are done. Why is that?
      // Assume wlog that the left side is finished and we have found
      // a path. Then the left hand side has found everything that is
      // reachable from the source, unless it has already been expanded
      // by the other side. The left hand side has expanded everything it
      // has found. If the right hand side has expanded a vertex, it "knows"
      // the distance of this vertex from the target and knows a shortest
      // path. Therefore, we do not have to go there any more.
      if (_left.isQueueEmpty() || _right.isQueueEmpty()) {
        // Note that we might have already found a path with the above
        // criteria, so we should then not report a second one:
        setAlgorithmFinished();
      }
    }
  }

  TRI_ASSERT(_options.onlyProduceOnePath());
  fetchResult();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::setAlgorithmFinished() {
  _algorithmFinished = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::setAlgorithmUnfinished() {
  _algorithmFinished = false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::isAlgorithmFinished()
    const {
  return _algorithmFinished;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
typename WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                        PathValidator>::BallSearchLocation
WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                               PathValidator>::getBallToContinueSearch() {
  if (_left.isQueueEmpty() and _right.isQueueEmpty()) {
    return BallSearchLocation::FINISH;
  }

  LOG_TOPIC("47011", TRACE, Logger::GRAPHS)
      << "Pondering left/right: " << _left.queueSize() << " vs. "
      << _right.queueSize() << " ==> "
      << (_left.queueSize() < _right.queueSize() ? "LEFT" : "RIGHT");

  if (_left.getDiameter() < 0.0) {
    return BallSearchLocation::LEFT;
  }

  if (_right.getDiameter() < 0.0) {
    return BallSearchLocation::RIGHT;
  }

  // Note not *both* left and right are empty, so if
  // _left is, _right is not!
  if (_left.isQueueEmpty()) {
    return BallSearchLocation::RIGHT;
  }

  if (_right.isQueueEmpty()) {
    return BallSearchLocation::LEFT;
  }

  // Here is the argument for the following final decision: If the search
  // happens to be "asymmetric" in the sense that one side has a lot more
  // work to do (which can easily happen with directed edges), then we
  // have better chances to complete the search if we put more emphasis
  // on the "cheaper" side. This is likely the one which has a shorter
  // queue. And even if the queue is only shorter temporarily, then this
  // will change over time as we expand more and more vertices on that
  // side. If the search is symmetric, we expect to have approximately
  // equal queue lengths by always expanding the shorter one. This is because
  // most graphs "expand" around their vertices. And even if we happen to
  // finish off one side first by this choice, this does not matter in the
  // grand scheme of things.
  TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
  if (_left.queueSize() < _right.queueSize()) {
    return BallSearchLocation::LEFT;
  }
  return BallSearchLocation::RIGHT;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::searchDone() const -> bool {
  if ((_left.isQueueEmpty() && _right.isQueueEmpty()) ||
      isAlgorithmFinished()) {
    return true;
  }
  if (_left.isQueueEmpty() && !_bestPath.has_value()) {
    return true;
  }
  if (_right.isQueueEmpty() && !_bestPath.has_value()) {
    // Special case for singleton (source == target), in this case
    // we should indicate that there is something still coming. If
    // we have already delivered the singleton, then the algorithm
    // will be marked as finished anyway and we return above with
    // true anyway.
    if (_singleton) {
      return false;
    }
    return true;
  }
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::fetchResult() -> void {
  if (_bestPath.has_value()) {
    _left.fetchResult(_bestPath.value());
    _right.fetchResult(_bestPath.value());
  }
  _resultsFetched = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::stealStats()
    -> aql::TraversalStats {
  aql::TraversalStats stats = _left.provider().stealStats();
  stats += _right.provider().stealStats();
  return stats;
}

//
// The following template parameter instantiations happen:
//
//
// Name                            Queue   Store   Prov    Valid
//
// # Weighted ShortestPath:
// WShortestPath (single)          We      No      Si      Va<Gl,Pa>
// WShortestPath (cluster)         We      No      Cl      Va<Gl,Pa>
// TracedWShortestPath (single)    Tr<We>  Tr      Tr<Si>  Va<Tr,Gl,Pa>
// TracedWShortestPath (cluster)   Tr<We>  Tr      Tr<Cl>  Va<Tr,Gl,Pa>
//
// # Weighted ShortestPath for Yen:
// WShortestPath (yen, single)     We      No      Si      Va<No,No>
// WShortestPath (yen, cluster)    We      No      Cl      Va<No,No>
// TracedWShortestPath (yen, sin)  Tr<We>  Tr      Tr<Si>  Tr<Va<No,No>>
// TracedWShortestPath (yen, clu)  Tr<We>  Tr      Tr<Cl>  Tr<Va<No,No>>
//
// Where:
//   Si/Cl    Single or Cluster provider
//   No/Tr    Non-traced or traced
//   Fi/We    Fifo-Queue or WeightedQueue (prio)
//   Va/Ta    Path validator or Taboo validator (wrapping normal)
//   No/Pa/Gl For validator: no uniqueness vs. path uniq. vs. global uniq.
//

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;
using SingleProvider = SingleServerProvider<SingleServerProviderStep>;

// WeightedShortestPathEnumerator<SingleProvider>:
template class WeightedShortestPathEnumerator<
    WeightedQueue<SingleServerProviderStep>,
    PathStore<SingleServerProviderStep>, SingleProvider,
    PathValidator<SingleProvider, PathStore<SingleServerProviderStep>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

// TracedWeightedShortestPathEnumerator<SingleProvider>:
template class WeightedShortestPathEnumerator<
    QueueTracer<WeightedQueue<SingleServerProviderStep>>,
    PathStoreTracer<PathStore<SingleServerProviderStep>>,
    ProviderTracer<SingleProvider>,
    PathValidatorTracer<
        PathValidator<ProviderTracer<SingleProvider>,
                      PathStoreTracer<PathStore<SingleServerProviderStep>>,
                      VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>>;

/* ClusterProvider Section */

using ClustProvider = ClusterProvider<ClusterProviderStep>;

// WeightedShortestPathEnumerator<ClustProvider>:
template class WeightedShortestPathEnumerator<
    WeightedQueue<ClusterProviderStep>, PathStore<ClusterProviderStep>,
    ClustProvider,
    PathValidator<ClustProvider, PathStore<ClusterProviderStep>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

// TracedWeightedShortestPathEnumerator<ClustProvider>:
template class WeightedShortestPathEnumerator<
    QueueTracer<WeightedQueue<ClusterProviderStep>>,
    PathStoreTracer<PathStore<ClusterProviderStep>>,
    ProviderTracer<ClustProvider>,
    PathValidatorTracer<
        PathValidator<ProviderTracer<ClustProvider>,
                      PathStoreTracer<PathStore<ClusterProviderStep>>,
                      VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>>;

}  // namespace arangodb::graph
