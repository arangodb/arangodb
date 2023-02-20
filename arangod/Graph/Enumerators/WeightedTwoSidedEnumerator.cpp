////////////////////////////////////////////////////////////////////////////////
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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "WeightedTwoSidedEnumerator.h"

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

using namespace arangodb;
using namespace arangodb::graph;

/*
 * Class: Ball (internally used in WeightedTwoSidedEnumerator)
 */

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
  _visitedNodes.clear();

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
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::fetchResult(CalculatedCandidate& candidate) -> void {
  // TODO: Right now almost a copy of method below - optimize this.
  std::vector<Step*> looseEnds{};
  auto& [weight, first, second] = candidate;

  if (_direction == Direction::FORWARD) {
    auto& step = first;
    if (!step.isProcessable()) {
      looseEnds.emplace_back(&step);
    }

  } else {
    auto& step = second;
    if (!step.isProcessable()) {
      looseEnds.emplace_back(&step);
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
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::fetchResults(CandidatesStore& candidates) -> void {
  std::vector<Step*> looseEnds{};

  // TODO: Check read of candidates
  if (_direction == Direction::FORWARD) {
    for (auto& [weight, firstStep, secondStep] : candidates.getQueue()) {
      auto& step = firstStep;
      if (!step.isProcessable()) {
        looseEnds.emplace_back(&step);
      }
    }
  } else {
    for (auto& [weight, firstStep, secondStep] : candidates.getQueue()) {
      auto& step = secondStep;
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
    // Vertices are now fetched. Think about other less-blocking and batch-wise
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
  if (_direction == FORWARD) {
    LOG_DEVEL << "LEFT: ";
  } else {
    LOG_DEVEL << "RIGHT: ";
  }

  for (auto const& peter : _visitedNodes) {
    arangodb::velocypack::HashedStringRef x = peter.first;
    LOG_DEVEL << " -> " << x.toString();
  }

  if (_visitedNodes.contains(step.getVertex().getID())) {
    return true;
  }
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    computeNeighbourhoodOfNextVertex(Ball& other, CandidatesStore& candidates)
        -> double {
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
  if (hasBeenVisited(step)) {
    return -1.0;
  }
  auto previous = _interior.append(step);

  auto verifyStep = [&](Step currentStep) {
    TRI_ASSERT(!hasBeenVisited(currentStep));
    ValidationResult res = _validator.validatePath(currentStep);

    if (!res.isPruned()) {
      // Add the step to our shell
      _queue.append(std::move(currentStep));
    }
  };

  double matchPathWeight = -1.0;

  _visitedNodes.emplace(step.getVertex().getID(), previous);
  if (other.hasBeenVisited(step)) {
    // TODO: WE DO NOT FIND 104 as a match but we should have!
    LOG_DEVEL << "OTHER HAS BEEN VISITED !!!!!!!!!!!!!!!!!!!!!!!!!!";
    // Shortest Path Match
    matchPathWeight = other.matchResultsInShell(step, candidates, _validator);
    LOG_DEVEL << "Result of the visit: " << matchPathWeight;
  } else {
    LOG_DEVEL << "OTHER has been NOT visited";
  }

  _provider.expand(step, previous, [&](Step n) -> void {
    if (!hasBeenVisited(n)) {
      verifyStep(std::move(n));
    }
  });

  return matchPathWeight;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::testDepthZero(Ball& other,
                                        CandidatesStore& candidates) {
  // TODO: Check - is this even required?
  /*for (auto const& step : _shell) {
    other.matchResultsInShell(step, candidates, _validator);
  }*/
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    matchResultsInShell(Step const& otherStep, CandidatesStore& candidates,
                        PathValidator const& otherSideValidator) -> double {
  auto position = _visitedNodes.at(otherStep.getVertex().getID());
  auto ourStep = _interior.getStepReference(position);

  // TODO: Check Prune + Filter - Currently, not supported, but could be (!) as
  //  a new feature.
  // auto res = _validator.validatePath(ourStep, otherSideValidator);
  // TRI_ASSERT(!res.isFiltered());
  // if (!res.isFiltered()) {
  // }

  // if (ourStep.isFirst() || otherStep.isFirst()) {
  //  TODO IMPORTANT
  //  TODO IMPORTANT
  //   this has been an opt. approach - but is wrong.
  //  TODO IMPORTANT
  //  TODO: Check this. Without this code snippet - we polluted the candidates
  //   store. We need to exclude special cases more carefully.
  //  return -1.0; <-- this whole body of the code is just wrong. We cannot
  //  exclude first or last
  //}
  double fullPathWeight = ourStep.getWeight() + otherStep.getWeight();
  LOG_DEVEL << "=========APPENDING CANDIDATES=========";

  if (_direction == FORWARD) {
    LOG_DEVEL << "WEIGHT: " << fullPathWeight;
    LOG_DEVEL << "OurStep:" << ourStep.toString();
    LOG_DEVEL << "OtherStep:" << otherStep.toString();
    candidates.append(std::make_tuple(fullPathWeight, ourStep, otherStep));
  } else {
    LOG_DEVEL << "WEIGHT: " << fullPathWeight;
    LOG_DEVEL << "OtherStep:" << otherStep.toString();
    LOG_DEVEL << "OurStep:" << ourStep.toString();
    candidates.append(std::make_tuple(fullPathWeight, otherStep, ourStep));
  }

  LOG_DEVEL << "==================";
  return fullPathWeight;
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

/*
 * Class: ResultCache (internally used in WeightedTwoSidedEnumerator)
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::ResultCache::ResultCache(Ball& left,
                                                                    Ball& right)
    : _internalLeft(left), _internalRight(right), _internalResultsCache{} {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::ResultCache::~ResultCache() =
    default;

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::ResultCache::clear() -> void {
  _internalResultsCache.clear();
};

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::ResultCache::addResult(CalculatedCandidate const& candidate)
    -> bool {
  auto const& [weight, first, second] = candidate;
  PathResult<ProviderType, typename ProviderType::Step> resultPathCandidate{
      _internalLeft.provider(), _internalRight.provider()};

  // generates left and right parts of the path and combines them
  // TODO: Check if we can implement this more effectively.
  //  Right now the I'm using a quick approach here for code verification.

  _internalLeft.buildPath(first, resultPathCandidate);
  _internalRight.buildPath(second, resultPathCandidate);

  // generates our (minimal) cached path representation
  // TODO: Continue here.
  // Tasks:
  // * we need a minimal edge based representation that does not take a lot of
  // memory
  // * then we need to check if that path is already in our
  // _internalResultsCache
  // * if is is, return false
  // * if it is now, return true and insert it to the cache datastructure.
  // auto pathCandidate = _internalResultPathCandidate;
  for (auto const& pathToCheck : _internalResultsCache) {
    bool foundDuplicate =
        resultPathCandidate.isEqualEdgeRepresentation(pathToCheck);
    if (foundDuplicate) {
      LOG_DEVEL << "==> DUPLICATE? " << std::boolalpha << foundDuplicate;
      return false;
    }
  }
  _internalResultsCache.push_back(std::move(resultPathCandidate));

  LOG_DEVEL << "==> DUPLICATE? " << std::boolalpha << " no !";
  return true;
};

/*
 * Class: WeightedTwoSidedEnumerator
 */

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
      _resultsCache(_left, _right),
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
  // 1.) Remove current results & state
  _candidatesStore.clear();
  _bestCandidateLength = -1.0;
  _resultsCache.clear();
  _results.clear();
  _leftInitialFetch = false;
  _rightInitialFetch = false;
  _handledInitialFetch = false;

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
  LOG_DEVEL << "isDone? " << std::boolalpha << searchDone();
  LOG_DEVEL << "_results.empty? " << std::boolalpha << _results.empty();
  LOG_DEVEL << "isAlgorithmFinished? " << std::boolalpha
            << isAlgorithmFinished();

  if (_options.getPathType() == PathType::Type::KShortestPaths) {
    if (!_candidatesStore.isEmpty()) {
      return false;
    }
  }

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
  LOG_DEVEL << "Init Weighted Shortest Path (K)";
  clear();

  _left.reset(source, 0);
  _right.reset(target, 0);
  _resultPath.clear();

  // Special depth == 0 case
  // TODO: Check - is this even required?
  /*if (_options.getMinDepth() == 0 && source == target) {
    _left.testDepthZero(_right, _candidates);
  }*/
}

// TODO: I think this can be removed and is probably not needed.
// Hint: check toVelocypack (Velocypack) vs. calculation via steps.
// Not 100% sure yet which way to go.
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
typename PathResult<ProviderType, typename ProviderType::Step>::WeightType
WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                           PathValidator>::identifyWeightType() {
  if (_options.getPathType() == PathType::Type::KShortestPaths) {
    bool hasWeightMethod = _options.hasWeightCallback();
    if (hasWeightMethod) {
      return PathResult<ProviderType, Step>::WeightType::ACTUAL_WEIGHT;
    } else {
      return PathResult<ProviderType, Step>::WeightType::AMOUNT_EDGES;
    }
  }

  return PathResult<ProviderType, Step>::WeightType::NONE;
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
  auto handleResult = [&]() {
    /*
     * Helper method to take care of stored results (in: _results)
     */
    if (!_results.empty()) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      if (_options.getPathType() == PathType::Type::ShortestPath) {
        TRI_ASSERT(_results.size() == 1);
      }
#endif

      LOG_DEVEL << "Size is: " << _results.size();
      LOG_DEVEL << "read candidate from _results";
      auto const& [weight, first, second] = _results.front();
      LOG_DEVEL << "Result Path WEIGHT: " << weight;
      auto const& leftVertex = first;
      auto const& rightVertex = second;

      // Performance Optimization:
      // It seems to be pointless to first push
      // everything in to the _resultPath object
      // and then iterate again to return the path
      // we should be able to return the path in the first go.
      _resultPath.clear();
      LOG_DEVEL << "Building result LEFT : " << leftVertex.toString();
      LOG_DEVEL << "Building result RIGHT: " << rightVertex.toString();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);
      TRI_ASSERT(!_resultPath.isEmpty());

      if (_options.getPathType() == PathType::Type::KShortestPaths) {
        // Add weight attribute to edges
        _resultPath.toVelocyPack(
            result, PathResult<ProviderType, Step>::WeightType::ACTUAL_WEIGHT);
      } else {
        _resultPath.toVelocyPack(result);
      }
      // remove handled result
      _results.pop_front();

      LOG_DEVEL << "<1337> Returned True (above)";
      return true;
    } else {
      LOG_DEVEL << "<1337> Returned false (above)";
      return false;
    }
  };

  auto handleCandidate = [&](CalculatedCandidate&& candidate) {
    LOG_DEVEL << "CANDIDATE DOES !!!  APPLY!!!!";
    auto const& [weight, first, second] = candidate;
    auto const& leftVertex = first;
    auto const& rightVertex = second;

    // Performance Optimization:
    // It seems to be pointless to first push
    // everything in to the _resultPath object
    // and then iterate again to return the path
    // we should be able to return the path in the first go.
    _resultPath.clear();
    LOG_DEVEL << "Building result LEFT : " << leftVertex.toString();
    LOG_DEVEL << "Building result RIGHT: " << rightVertex.toString();
    _left.buildPath(leftVertex, _resultPath);
    _right.buildPath(rightVertex, _resultPath);
    TRI_ASSERT(!_resultPath.isEmpty());

    if (_options.getPathType() == PathType::Type::KShortestPaths) {
      // Add weight attribute to edges
      // Add weight attribute to edges
      _resultPath.toVelocyPack(
          result, PathResult<ProviderType, Step>::WeightType::ACTUAL_WEIGHT);
    } else {
      _resultPath.toVelocyPack(result);
    }
  };

  auto checkKPathsCandidates = [&]() {
    /*
     * Helper method to take care of KPath related candidates
     */
    TRI_ASSERT(searchDone());
    TRI_ASSERT(_options.getPathType() == PathType::Type::KShortestPaths);
    if (!_candidatesStore.isEmpty()) {
      LOG_DEVEL << " WE STILL HAVE CANDIDATES LEFT";
      while (!_candidatesStore.isEmpty()) {
        LOG_DEVEL << "CANDIDATE STORE SIZE: " << _candidatesStore.size();
        CalculatedCandidate potentialCandidate = _candidatesStore.pop();

        // only add if non-duplicate
        bool foundNonDuplicatePath =
            _resultsCache.addResult(potentialCandidate);
        LOG_DEVEL << "CHECKING CANDIDATE";
        if (foundNonDuplicatePath) {
          handleCandidate(std::move(potentialCandidate));
          return true;
        }
      }

      LOG_DEVEL << "<1337> Returning false(1)";
      return false;
    }

    LOG_DEVEL << "<1337> Returning false(11)";
    return false;
  };

  auto checkShortestPathCandidates = [&]() {
    TRI_ASSERT(searchDone());
    TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
    /*
     * Helper method to take care of ShortestPath related candidates
     */
    bool foundPath = handleResult();
    if (!foundPath) {
      if (!_candidatesStore.isEmpty() && !isAlgorithmFinished()) {
        CalculatedCandidate candidate = _candidatesStore.pop();
        handleCandidate(std::move(candidate));
        foundPath = true;
        // ShortestPath produces only one result.
        setAlgorithmFinished();
      }
    }

    return foundPath;
  };

  while (!isDone()) {
    if (!searchDone()) {
      LOG_DEVEL << "SEARCH IS NOT DONE!";
      searchMoreResults();

      if (handleResult()) {
        if (_options.onlyProduceOnePath()) {
          // At this state we've produced a valid path result. In case we're
          // using the path type "(Weighted)ShortestPath", the algorithm is
          // finished. We need to store this information.

          TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
          setAlgorithmFinished();
          return false;
        }
        return true;
      }
    }
  }

  TRI_ASSERT(isDone());
  LOG_DEVEL << "We are done";
  if (_options.getPathType() == PathType::Type::KShortestPaths) {
    LOG_DEVEL << "III.";
    if (checkKPathsCandidates()) {
      return true;
    }
  } else if (_options.getPathType() == PathType::Type::ShortestPath &&
             !isAlgorithmFinished()) {
    LOG_DEVEL << "IV.";
    if (checkShortestPathCandidates()) {
      return true;
    }
  }

  LOG_DEVEL << "<1337> Returned false(3)";
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchMoreResults() {
  while (!searchDone()) {
    _resultsFetched = false;

    double matchPathLength = -1.0;
    auto searchLocation = getBallToContinueSearch();
    if (searchLocation == BallSearchLocation::LEFT) {
      // might need special case depth == 0
      matchPathLength =
          _left.computeNeighbourhoodOfNextVertex(_right, _candidatesStore);
    } else if (searchLocation == BallSearchLocation::RIGHT) {
      // special case for initial step expansion
      // this needs to only be checked once and only _right as we always
      // start _left with our search. If that behaviour changed, this
      // verification needs to be moved as well.

      if (!getInitialFetchVerified()) {
        setInitialFetchVerified();
      }

      matchPathLength =
          _right.computeNeighbourhoodOfNextVertex(_left, _candidatesStore);
    } else {
      TRI_ASSERT(searchLocation == BallSearchLocation::FINISH);
      // Our queue is empty. We cannot produce more results.
      setAlgorithmFinished();
    }

    LOG_DEVEL << "Matchpathlength is: " << matchPathLength;
    if (matchPathLength > 0) {
      // means we've found a match (-1.0 means no match)
      if (_bestCandidateLength < 0 || matchPathLength < _bestCandidateLength) {
        _bestCandidateLength = matchPathLength;
      } else if (matchPathLength > _bestCandidateLength) {
        LOG_DEVEL << "Found new path result - emplacing:";
        // TODO: Double check this.
        // If a candidate has been found, we insert it into the store and only
        // return the length of that path. As soon as we insert in into the
        // store we sort that internal vector, therefore it might re-balance. If
        // we pop the first element it is guaranteed that the calculated weight
        // must be the same. It is not guaranteed that this is the path we
        // actually inserted. Nevertheless, this should not be any problem.
        //
        // Additionally, this is only a potential candidate. We may find same
        // paths multiple times. If that is the case, we just get rid of that
        // candidate and do not pass it to the result data structure.
        //
        // Not only one candidate needs to be verified, but all candidates
        // which do have lower weights as the current found (match).

        LOG_DEVEL << "=== Candidates Summary === ";
        for (auto const& [weight, first, second] :
             _candidatesStore.getQueue()) {
          LOG_DEVEL << weight;
          LOG_DEVEL << first.toString();
          LOG_DEVEL << second.toString();
          LOG_DEVEL << "______________________";
        }

        while (std::get<0>(_candidatesStore.peek()) < matchPathLength) {
          LOG_DEVEL << "::::::::::::::::::::::::::::::::";
          LOG_DEVEL << " ::::: CANDIDATE :::: ";
          CalculatedCandidate potentialCandidate = _candidatesStore.pop();
          LOG_DEVEL << "From: " << std::get<1>(potentialCandidate).toString();
          LOG_DEVEL << "To: " << std::get<2>(potentialCandidate).toString();
          LOG_DEVEL << "Cand weight: " << std::get<0>(potentialCandidate);
          LOG_DEVEL << "Expected: " << matchPathLength;
          LOG_DEVEL << "::::::::::::::::::::::::::::::::";

          // TODO IMPORTANT: CHECK / DISCUSS THIS
          // TRI_ASSERT(std::get<0>(potentialCandidate) ==
          // _bestCandidateLength);

          // This assert only being used for development. My guess has been that
          // this needs to be true but currently (above assert) fails.
          // TRI_ASSERT(std::get<0>(potentialCandidate) <=
          // _bestCandidateLength);
          bool foundShortestPath = false;
          if (_options.getPathType() == PathType::Type::KShortestPaths) {
            foundShortestPath = _resultsCache.addResult(potentialCandidate);
            LOG_DEVEL << "did we found a valid path result: " << std::boolalpha
                      << foundShortestPath;
          } else if (_options.getPathType() == PathType::Type::ShortestPath) {
            // Performance optimization: We do not use the cache as we will
            // always calculate only one path.
            foundShortestPath = true;
          }

          if (foundShortestPath) {
            LOG_DEVEL << "::::::::::::::::::::::::::::::::";
            LOG_DEVEL << " This candidate will be placed into results ("
                      << std::get<0>(potentialCandidate)
                      << ") total size: " << _results.size();
            LOG_DEVEL << "::::::::::::::::::::::::::::::::";
            _results.emplace_back(std::move(potentialCandidate));

            if (_options.getPathType() == PathType::Type::ShortestPath) {
              // Proven to be finished with the algorithm. Our last best score
              // is the shortest path (quick exit).
              setAlgorithmFinished();
              break;
            }

            // Setting _bestCandidateLength to -1.0 (double) will perform an
            // initialize / reset. This means we need to find at least two new
            // paths to prove that we've found the next proven and valid
            // shortest path.
            LOG_DEVEL << " -- resetting candidate -- ";
            // TODO: CHECK THIS. Next row can be just removed I guess.
            // ONly init value should be set to -1.0. As soon as we do have our
            // candidates in place - and that list is sorted - we can just peek
            // at the first one and set this as new "bestCandidate".
            // _bestCandidateLength = -1.0;
            auto [weight, nextStepLeft, nextStepRight] =
                _candidatesStore.peek();
            _bestCandidateLength = weight;
          }
        }
      }
    }
  }
  // setAlgorithmFinished();

  if (_options.onlyProduceOnePath() && _bestCandidateLength >= 0) {
    fetchResult(_bestCandidateLength);
  } else {
    fetchResults();
  }
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

    if (_bestCandidateLength >= 0 && isAlgorithmFinished()) {
      // TODO: As soon as we enable also KShortestPaths - this must be changed.
      //  The whole candidate structure right now is build for exactly one path
      //  and not multiple paths.
      //  This means we must calculate the second best path match as next.

      // just drop one result for skipping
      std::ignore = _candidatesStore.pop();
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
  if (!_leftInitialFetch) {
    _leftInitialFetch = true;
    return BallSearchLocation::LEFT;
  } else if (!_rightInitialFetch) {
    _rightInitialFetch = true;
    return BallSearchLocation::RIGHT;
  }

  if (!_left.isQueueEmpty() && !_right.isQueueEmpty()) {
    auto leftStep = _left.peekQueue();
    auto rightStep = _right.peekQueue();

    if (leftStep.getWeight() <= rightStep.getWeight()) {
      return BallSearchLocation::LEFT;
    } else {
      return BallSearchLocation::RIGHT;
    }
  } else if (!_left.isQueueEmpty() && _right.isQueueEmpty()) {
    return BallSearchLocation::RIGHT;
  } else if (_left.isQueueEmpty() && !_right.isQueueEmpty()) {
    return BallSearchLocation::LEFT;
  }

  // Default
  return BallSearchLocation::FINISH;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchDone() const -> bool {
  LOG_DEVEL << " => SearchDone?: " << std::boolalpha
            << (_left.noPathLeft() || _right.noPathLeft() ||
                isAlgorithmFinished())
            << " Candidates (" << _candidatesStore.size() << ")"
            << " leftNoPath: " << std::boolalpha << _left.noPathLeft()
            << " rightNoPath: " << std::boolalpha << _right.noPathLeft();

  return _left.noPathLeft() || _right.noPathLeft() || isAlgorithmFinished();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::fetchResults() -> void {
  if (!_resultsFetched && !_candidatesStore.isEmpty()) {
    _left.fetchResults(_candidatesStore);
    _right.fetchResults(_candidatesStore);
  }
  _resultsFetched = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::fetchResult(double key)
    -> void {
  if (!_resultsFetched && !_candidatesStore.isEmpty()) {
    // TODO: Double check this.
    // TRI_ASSERT(key == weight);
    _left.fetchResult(_candidatesStore.peek());
    _right.fetchResult(_candidatesStore.peek());
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
