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

#include "WeightedTwoSidedEnumerator.h"

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
#include "Graph/Queues/QueueTracer.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/ValidationResult.h"
#include "Graph/algorithm-aliases.h"

#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>

using namespace arangodb;
using namespace arangodb::graph;

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedTwoSidedEnumerator<
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
      _graphOptions(options) {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::Ball::~Ball() = default;

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::reset(VertexRef center,
                                                            size_t depth) {
  LOG_DEVEL << "FINDER RESET WEIGHTED";
  clear();
  auto firstStep = _provider.startVertex(center, depth);
  _queue.append(std::move(firstStep));
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::clear() {
  _queue.clear();
  _interior.reset();  // PathStore

  // Provider - Must be last one to be cleared(!)
  clearProvider();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
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
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::doneWithDepth() const
    -> bool {
  return _queue.isEmpty();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::noPathLeft() const
    -> bool {
  return doneWithDepth();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::peekQueue() const
    -> Step const& {
  return _queue.peek();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::isQueueEmpty() const
    -> bool {
  return _queue.isEmpty();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::fetchResults(ResultList&
                                                                       results)
    -> void {
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
    // TODO: Discuss how to optimize here. Currently we'll mark looseEnds in
    // fetch as fetched. This works, but we might create a batch limit here in
    // the future. Also discuss: Do we want (re-)fetch logic here?
    // TODO: maybe we can combine this with prefetching of paths
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::hasBeenVisited(Step const&
                                                                         step)
    -> bool {
  LOG_DEVEL << "THIS NEEDS to be fixed properly";

  if (_direction == FORWARD) {
    LOG_DEVEL << "content of visited left is:";
  } else {
    LOG_DEVEL << "content of visited right is:";
  }
  for (auto const& peter : _visitedNodes) {
    LOG_DEVEL << "---> " << peter.first;
  }

  if (_visitedNodes.contains(step.getVertex().getID().toString())) {
    return true;
  }
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    computeNeighbourhoodOfNextVertex(Ball& other, CandidatesMap& candidates)
        -> void {
  // Pull next element from Queue
  // Do 1 step search
  TRI_ASSERT(!_queue.isEmpty());
  if (!_queue.hasProcessableElement()) {
    std::vector<Step*> looseEnds = _queue.getLooseEnds();
    for (auto const& elem : looseEnds) {
      LOG_DEVEL << "Step: " << elem->toString();
    }
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);

    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.get();

    TRI_ASSERT(preparedEnds.size() != 0);
    TRI_ASSERT(_queue.hasProcessableElement());
  }

  auto step = _queue.pop();
  if (hasBeenVisited(step)) {
    return;
  }
  LOG_DEVEL << " - expanding: " << step.toString();
  auto previous = _interior.append(step);

  auto verifyStep = [&](Step currentStep) {
    TRI_ASSERT(!hasBeenVisited(currentStep));
    ValidationResult res = _validator.validatePath(currentStep);

    if (!res.isPruned()) {
      // Add the step to our shell
      _queue.append(std::move(currentStep));
    }
  };

  // A -> B
  // Step A (previous: -> null)
  // Step B (previous: 0 (index auf A)

  // (B, 2) , (B, 5)

  _visitedNodes.emplace(step.getVertex().getID().toString(), previous);
  if (other.hasBeenVisited(step)) {
    // Shortest Path Match
    other.matchResultsInShell(step, candidates, _validator);
  }

  _provider.expand(step, previous, [&](Step n) -> void {
    LOG_DEVEL << "Received from Provider: " << n.toString()
              << ", weight:" << n.getWeight();
    if (!hasBeenVisited(n)) {
      verifyStep(std::move(n));
    } else {
      LOG_DEVEL << "Step " << n.toString()
                << " - has been visited already, therefore skipping!";
    }
  });

  // Expand C:
  // -> F 14
  // -> A 2
  // -> B 5            // init: F, A, B
  // => A, B, F
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::testDepthZero(Ball& other,
                                        CandidatesMap& candidates) {
  // TODO: FIXME
  /*for (auto const& step : _shell) {
    other.matchResultsInShell(step, candidates, _validator);
  }*/
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    matchResultsInShell(Step const& otherStep, CandidatesMap& candidates,
                        PathValidator const& otherSideValidator) -> void {
  LOG_DEVEL << "========================";
  LOG_DEVEL << "=== FOUND MATCH FOR : " << otherStep.toString() << " === ";

  auto position = _visitedNodes.at(otherStep.getVertex().getID().toString());
  auto ourStep = _interior.getStepReference(position);

  // TODO: Check Prune + Filter
  // auto res = _validator.validatePath(ourStep, otherSideValidator);
  // TRI_ASSERT(!res.isFiltered());
  // if (!res.isFiltered()) {
  LOG_DEVEL << " -> III. (path is validated)";
  LOG_DEVEL << " -> III. Write path into result";
  LOG_DEVEL << "Our: " << ourStep.toString();
  LOG_DEVEL << "Oth: " << otherStep.toString();
  double fullPathLength = ourStep.getWeight() + otherStep.getWeight();
  if (_direction == FORWARD) {
    candidates.emplace(fullPathLength, std::make_pair(ourStep, otherStep));
  } else {
    candidates.emplace(fullPathLength, std::make_pair(otherStep, ourStep));
  }
  //}
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<
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
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::provider()
    -> ProviderType& {
  return _provider;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::
    WeightedTwoSidedEnumerator(ProviderType&& forwardProvider,
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
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::~WeightedTwoSidedEnumerator() {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::destroyEngines() -> void {
  // Note: Left & Right Provider use the same traversal engines.
  //   => Destroying one of them is enough.
  _left.provider().destroyEngines();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::clear() {
  // Order is important here, please do not change.
  // 1.) Remove current results
  _results.clear();

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
bool WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::isDone() const {
  return (_results.empty() && searchDone()) || isAlgorithmFinished();
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
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::reset(VertexRef source,
                                                      VertexRef target,
                                                      size_t depth) {
  _results.clear();
  _left.reset(source, 0);
  _right.reset(target, 0);
  _resultPath.clear();

  // Special depth == 0 case
  if (_options.getMinDepth() == 0 && source == target) {
    _left.testDepthZero(_right, _candidates);
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
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::getNextPath(VPackBuilder&
                                                                result) {
  while (!isDone()) {
    LOG_DEVEL << "- we're not done yet (getNextPath)";
    searchMoreResults();

    bool validShortestPathFound = false;
    // TODO: continue here (see slack information)

    while (!_results.empty()) {
      LOG_DEVEL << "Results populated, we must be finished!";
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

      // At this state we've produced a valid path result. In case we're using
      // the path type "ShortestPath", the algorithm is finished. We need
      // to store this information.
      if (_options.onlyProduceOnePath()) {
        LOG_DEVEL << "Set to finished!";
        setAlgorithmFinished();
      }

      return true;
    }
  }
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchMoreResults() {
  while (_results.empty() && !searchDone()) {
    _resultsFetched = false;

    auto searchLocation = getBallToContinueSearch();
    if (searchLocation == BallSearchLocation::LEFT) {
      // might need special case depth == 0
      _left.computeNeighbourhoodOfNextVertex(_right, _results);
    } else if (searchLocation == BallSearchLocation::RIGHT) {
      // special case for initial step expansion
      // this needs to only be checked once and only _right as we always
      // start _left with our search. If that behaviour changed, this
      // verification needs to be moved as well.

      if (!getInitialFetchVerified()) {
        setInitialFetchVerified();
      }

      _right.computeNeighbourhoodOfNextVertex(_left, _results);
    } else {
      setAlgorithmFinished();
    }
  }

  fetchResults();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::setAlgorithmFinished() {
  _algorithmFinished = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::setAlgorithmUnfinished() {
  _algorithmFinished = false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::setInitialFetchVerified() {
  _handledInitialFetch = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::getInitialFetchVerified() {
  return _handledInitialFetch;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::isAlgorithmFinished() const {
  return _algorithmFinished;
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::skipPath() {
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

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
typename WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                    PathValidator>::BallSearchLocation
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::getBallToContinueSearch() {
  // TODO: Implement easy strategy way to change iteration "order" here.
  LOG_DEVEL << "";
  LOG_DEVEL << "Get ball to continue search";
  if (!_leftInitialFetch) {
    LOG_DEVEL << "";
    LOG_DEVEL << "-init left";
    _leftInitialFetch = true;
    return BallSearchLocation::LEFT;
  } else if (!_rightInitialFetch) {
    LOG_DEVEL << "";
    LOG_DEVEL << "-init right";
    _rightInitialFetch = true;
    return BallSearchLocation::RIGHT;
  }

  LOG_DEVEL << "";
  if (!_left.isQueueEmpty() && !_right.isQueueEmpty()) {
    auto leftStep = _left.peekQueue();
    auto rightStep = _right.peekQueue();
    LOG_DEVEL << "########### Debug Queue Information ########### ";
    LOG_DEVEL << "    Left step is: " << leftStep.toString() << ", weight: ("
              << leftStep.getWeight() << ")";
    LOG_DEVEL << "    Right step is: " << rightStep.toString() << ", weight: ("
              << rightStep.getWeight() << ")";

    LOG_DEVEL << "########### Debug Queue    END ###########";

    if (leftStep.getWeight() <= rightStep.getWeight()) {
      LOG_DEVEL << "-left smaller";
      return BallSearchLocation::LEFT;
    } else {
      LOG_DEVEL << "-right smaller";
      return BallSearchLocation::RIGHT;
    }
  } else if (!_left.isQueueEmpty() && _right.isQueueEmpty()) {
    LOG_DEVEL << "RIGHT default";
    return BallSearchLocation::RIGHT;
  } else if (_left.isQueueEmpty() && !_right.isQueueEmpty()) {
    LOG_DEVEL << "LEFT default";
    return BallSearchLocation::LEFT;
  }

  // Default
  return BallSearchLocation::FINISH;
};

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchDone() const -> bool {
  return _left.noPathLeft() || _right.noPathLeft() || isAlgorithmFinished();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::fetchResults() -> void {
  if (!_resultsFetched && !_results.empty()) {
    _left.fetchResults(_results);
    _right.fetchResults(_results);
  }
  _resultsFetched = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::stealStats()
    -> aql::TraversalStats {
  aql::TraversalStats stats = _left.provider().stealStats();
  stats += _right.provider().stealStats();
  return stats;
}

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    ::arangodb::graph::WeightedQueue<SingleServerProviderStep>,
    ::arangodb::graph::PathStore<SingleServerProviderStep>,
    SingleServerProvider<SingleServerProviderStep>,
    ::arangodb::graph::PathValidator<
        SingleServerProvider<SingleServerProviderStep>,
        PathStore<SingleServerProviderStep>, VertexUniquenessLevel::PATH,
        EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<
        ::arangodb::graph::WeightedQueue<SingleServerProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<SingleServerProviderStep>>,
    ::arangodb::graph::ProviderTracer<
        SingleServerProvider<SingleServerProviderStep>>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::ProviderTracer<
            SingleServerProvider<SingleServerProviderStep>>,
        ::arangodb::graph::PathStoreTracer<
            ::arangodb::graph::PathStore<SingleServerProviderStep>>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

/* ClusterProvider Section */

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    ::arangodb::graph::WeightedQueue<::arangodb::graph::ClusterProviderStep>,
    ::arangodb::graph::PathStore<ClusterProviderStep>,
    ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::PathValidator<
        ClusterProvider<ClusterProviderStep>, PathStore<ClusterProviderStep>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    ::arangodb::graph::QueueTracer<::arangodb::graph::WeightedQueue<
        ::arangodb::graph::ClusterProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<ClusterProviderStep>>,
    ::arangodb::graph::ProviderTracer<ClusterProvider<ClusterProviderStep>>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::ProviderTracer<ClusterProvider<ClusterProviderStep>>,
        ::arangodb::graph::PathStoreTracer<
            ::arangodb::graph::PathStore<ClusterProviderStep>>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;
