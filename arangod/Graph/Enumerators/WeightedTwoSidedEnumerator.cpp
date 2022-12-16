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
  clear();
  auto firstStep = _provider.startVertex(center, depth);
  _queue.append(std::move(firstStep));
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::clear() {
  _queue.clear();
  _shell.clear();
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

  // Guarantee that _shell is empty. Steps are contained in _shell and those
  // do contain VertexRefs which are hold in PathStore.
  TRI_ASSERT(_shell.empty());

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
  return doneWithDepth() && _shell.empty();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::shellSize() const
    -> size_t {
  return _shell.size();
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
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::computeNeighbourhoodOfNextVertex(Ball& other,
                                                           ResultList& results)
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
  LOG_DEVEL << " - expanding: " << step.toString();
  auto previous = _interior.append(step);

  auto verifyStep = [&](Step currentStep) {
    ValidationResult res = _validator.validatePath(currentStep);

    // Check if other Ball knows this Vertex.
    // Include it in results.
    if (!res.isFiltered()) {
      // One side of the path is checked, the other side is unclear:
      // We need to combine the test of both sides.

      // For GLOBAL: We ignore otherValidator, On FIRST match: Add this match
      // as result, clear both sides. => This will give the shortest path.
      // TODO: Check if the GLOBAL holds true for weightedEdges

      LOG_DEVEL << "Checking new method: ";

      // NEW comparison code
      LOG_DEVEL << "=== NOW CHECKING FOR A MATCH";
      LOG_DEVEL << " - we're checking step: " << currentStep.toString();

      if (_direction == FORWARD) {
        LOG_DEVEL << "(FORWARD)";
        if (other.hasBeenVisited(currentStep)) {
          LOG_DEVEL << "WE HAVE FOUND A STEP WHICH HAS BEEN VISITED IN THE "
                       "OTHER BALL ALREADY";
          other.matchResultsInShell(currentStep, results, _validator);
          LOG_DEVEL << "IN THIS CASE WE SHOULD BE FINISHED";
        } else {
          LOG_DEVEL << "WE HAVE FOUND A STEP WHICH HAS NOT BEEN VISITED IN THE "
                       "OTHER BALL";
        }
      } else {
        LOG_DEVEL << "(BACKWARD)";
        if (other.hasBeenVisited(currentStep)) {
          other.matchResultsInShell(currentStep, results, _validator);
          LOG_DEVEL << "WE HAVE FOUND A STEP WHICH HAS BEEN VISITED IN THE "
                       "OTHER BALL ALREADY";
          LOG_DEVEL << "IN THIS CASE WE SHOULD BE FINISHED";
        } else {
          LOG_DEVEL << "WE HAVE FOUND A STEP WHICH HAS NOT BEEN VISITED IN THE "
                       "OTHER BALL";
        }
      }
      LOG_DEVEL << "Match result in shell";
    }
    if (!res.isPruned()) {
      // Add the step to our shell
      LOG_DEVEL << "adding step to our shell: " << currentStep.toString();
      _queue.append(std::move(currentStep));
    }
  };

  _visitedNodes.emplace(step.getVertex().getID().toString(), previous);
  _provider.expand(step, previous, [&](Step n) -> void {
    LOG_DEVEL << "Received from Provider: " << n.toString();
    LOG_DEVEL << "ZZZZZZ Weight of that STEP is " << n.getWeight();
    // TODO: Check hasBeenVisited(!) - include weights
    if (!hasBeenVisited(n)) {
      verifyStep(std::move(n));
    } else {
      LOG_DEVEL << "Step " << n.toString()
                << " - has been visited already, therefore skipping!";
    }
  });

  // (B, 3)
  // (B, 5) <- DISCARD - hier bitte nicht weiter suchen
  // (B, 2) müsste (B, 3) ersetzen

  // -> Wir holen den besten (minimum weight) von der Queue (X)
  // -> Expand(Step X)
  //  => Neue Steps: A(1),B(2),C(3)
  //    => FILTER && PRUNE muss erfolgen
  //    => Auf die Queue, falls "valide"
  //
  // DONE, wenn:
  // Links shortest path gefunden und rechts shortest path gefunden
  // -> Knoten(Y) als Schnittpunkt gefunden (TRI_ASSERT == size(1))

  // -> Bewiesen Links shortest path für Y gefunden
  // -> Bewiesen REchts shortest path für Y gefunden
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::testDepthZero(Ball& other, ResultList& results) {
  for (auto const& step : _shell) {
    other.matchResultsInShell(step, results, _validator);
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    matchResultsInShell(Step const& otherStep, ResultList& results,
                        PathValidator const& otherSideValidator) -> void {
  // OLD Comparison code

  LOG_DEVEL << "========================";
  LOG_DEVEL << "=== FOUND MATCH FOR : " << otherStep.toString() << " === ";
  LOG_DEVEL << "========================";

  LOG_DEVEL << "now we need to build the full path LEFT + RIGHT";
  // auto invalidStep = match;
  // results.push_back(std::make_pair(invalidStep, invalidStep));
  LOG_DEVEL << " -> I. Generate path";
  LOG_DEVEL << " -> II. Validate path";
  auto position = _visitedNodes.at(otherStep.getVertex().getID().toString());
  auto ourStep = _interior.getStepReference(position);

  auto res = _validator.validatePath(ourStep, otherSideValidator);
  if (!res.isFiltered()) {
    LOG_DEVEL << " -> III. (path is validated)";
    LOG_DEVEL << " -> III. Write path into result";
    if (_direction == FORWARD) {
      results.push_back(std::make_pair(ourStep, otherStep));
    } else {
      results.push_back(std::make_pair(otherStep, ourStep));
    }
  }

  /*auto [first, last] = _shell.equal_range(match);
  if (_direction == FORWARD) {
    while (first != last) {
      LOG_DEVEL << "Forward First: " << first->toString();
      LOG_DEVEL << "Forward Last: " << last->toString();
      auto res = _validator.validatePath(*first, otherSideValidator);
      if (!res.isFiltered()) {
        LOG_TOPIC("6a01b", DEBUG, Logger::GRAPHS)
            << "Found path " << *first << " and " << match;
        LOG_DEVEL << "Found path " << *first << " and " << match;
        results.push_back(std::make_pair(*first, match));
      }
      first++;
    }
  } else {
    while (first != last) {
      LOG_DEVEL << "Backward First: " << first->toString();
      LOG_DEVEL << "Backward Last: " << last->toString();
      auto res = _validator.validatePath(*first, otherSideValidator);
      if (!res.isFiltered()) {
        LOG_TOPIC("d1830", DEBUG, Logger::GRAPHS)
            << "Found path " << match << " and " << *first;
        LOG_DEVEL << "2. Found path " << match << " and " << *first;
        results.push_back(std::make_pair(match, *first));
      }
      first++;
    }
  }*/
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
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::getNextPath(VPackBuilder&
                                                                result) {
  while (!isDone()) {
    LOG_DEVEL << "- we're not done yet (getNextPath)";
    searchMoreResults();

    while (!_results.empty()) {
      LOG_DEVEL << "Results populated, we should be finished!";
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
  LOG_DEVEL << "Before loop searchMoreResults";
  while (_results.empty() && !searchDone()) {
    LOG_DEVEL << "Inside loop searchMoreResults (results are empty und we're "
                 "not done yet)";
    _resultsFetched = false;

    auto searchLocation = getBallToContinueSearch();
    if (searchLocation == BallSearchLocation::LEFT) {
      _left.computeNeighbourhoodOfNextVertex(_right, _results);
    } else if (searchLocation == BallSearchLocation::RIGHT) {
      _right.computeNeighbourhoodOfNextVertex(_left, _results);
    } else {
      setAlgorithmFinished();
    }
  }

  LOG_DEVEL << "fetch results";
  fetchResults();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::setAlgorithmFinished() {
  LOG_DEVEL << "Setting state to finished.";
  _algorithmFinished = true;
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
    LOG_DEVEL << "- we're not done yet (skip)";
    searchMoreResults();

    while (!_results.empty()) {
      LOG_DEVEL << "- skipped a result (skip)";
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
  LOG_DEVEL << "";
  LOG_DEVEL << "Get ball to continue search";
  if (!_leftInitialFetch) {
    LOG_DEVEL << "";
    LOG_DEVEL << "";
    LOG_DEVEL << "-init left";
    _leftInitialFetch = true;
    return BallSearchLocation::LEFT;
  } else if (!_rightInitialFetch) {
    LOG_DEVEL << "";
    LOG_DEVEL << "";
    LOG_DEVEL << "-init right";
    _rightInitialFetch = true;
    return BallSearchLocation::RIGHT;
  }

  LOG_DEVEL << "";
  LOG_DEVEL << "";
  if (!_left.isQueueEmpty() && !_right.isQueueEmpty()) {
    auto leftStep = _left.peekQueue();
    auto rightStep = _right.peekQueue();
    LOG_DEVEL << "########### Debug Queue Information ########### ";
    LOG_DEVEL << "Left step is: " << leftStep.toString() << ", weight: ("
              << leftStep.getWeight() << ")";
    LOG_DEVEL << "Right step is: " << rightStep.toString() << ", weight: ("
              << rightStep.getWeight() << ")";

    LOG_DEVEL << "########### Debug Queue    END      ########### ";

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

  LOG_DEVEL << "default FINISH";
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
