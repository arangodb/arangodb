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
    futureEnds.get();
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::fetchResults(CandidatesStore& candidates) -> void {
  std::vector<Step*> looseEnds{};

  if (_direction == Direction::FORWARD) {
    for (auto& [weight, leftMeetingPoint, rightMeetingPoint] :
         candidates.getQueue()) {
      auto& step = leftMeetingPoint;
      if (!step.isProcessable()) {
        looseEnds.emplace_back(&step);
      }
    }
  } else {
    for (auto& [weight, leftMeetingPoint, rightMeetingPoint] :
         candidates.getQueue()) {
      auto& step = rightMeetingPoint;
      if (!step.isProcessable()) {
        looseEnds.emplace_back(&step);
      }
    }
  }

  if (!looseEnds.empty()) {
    // Will throw all network errors here
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    futureEnds.get();
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::hasBeenVisited(Step const&
                                                                         step)
    -> bool {
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

  if (_graphOptions.getPathType() == PathType::Type::ShortestPath) {
    if (hasBeenVisited(step)) {
      return -1.0;
    }
  }

  double matchPathWeight = -1.0;

  if (_graphOptions.getPathType() == PathType::Type::ShortestPath) {
    TRI_ASSERT(!_visitedNodes.contains(step.getVertex().getID()));
    _visitedNodes.emplace(step.getVertex().getID(), std::vector{previous});
  } else {
    _visitedNodes[step.getVertex().getID()].emplace_back(previous);
  }

  if (other.hasBeenVisited(step)) {
    // Shortest Path Match
    ValidationResult res = _validator.validatePath(step);
    TRI_ASSERT(!res.isFiltered() && !res.isPruned());

    matchPathWeight = other.matchResultsInShell(step, candidates, _validator);
  }

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
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    matchResultsInShell(Step const& otherStep, CandidatesStore& candidates,
                        PathValidator const& otherSideValidator) -> double {
  double fullPathWeight = -1.0;
  auto positions = _visitedNodes.at(otherStep.getVertex().getID());

  for (auto const& position : positions) {
    auto ourStep = _interior.getStepReference(position);

    auto res = _validator.validatePath(ourStep, otherSideValidator);
    if (res.isFiltered() || res.isPruned()) {
      // This validator e.g. checks for path uniqueness violations.
      continue;
    }

    double nextFullPathWeight = ourStep.getWeight() + otherStep.getWeight();

    if (_direction == FORWARD) {
      candidates.append(
          std::make_tuple(nextFullPathWeight, ourStep, otherStep));
    } else {
      candidates.append(
          std::make_tuple(nextFullPathWeight, otherStep, ourStep));
    }

    if (fullPathWeight < 0.0 || fullPathWeight > nextFullPathWeight) {
      fullPathWeight = nextFullPathWeight;
    }
  }

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
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::ResultCache::
    tryAddResult(CalculatedCandidate const& candidate) -> bool {
  auto const& [weight, first, second] = candidate;
  PathResult<ProviderType, typename ProviderType::Step> resultPathCandidate{
      _internalLeft.provider(), _internalRight.provider()};

  // generates left and right parts of the path and combines them
  // then checks whether we do have a path duplicate or not.
  _internalLeft.buildPath(first, resultPathCandidate);
  _internalRight.buildPath(second, resultPathCandidate);
  for (auto const& pathToCheck : _internalResultsCache) {
    bool foundDuplicate =
        resultPathCandidate.isEqualEdgeRepresentation(pathToCheck);
    if (foundDuplicate) {
      return false;
    }
  }
  _internalResultsCache.push_back(std::move(resultPathCandidate));
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
  clear();

  _left.reset(source, 0);
  _right.reset(target, 0);
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

      auto const& [weight, leftVertex, rightVertex] = _results.front();

      _resultPath.clear();
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

      return true;
    } else {
      return false;
    }
  };

  auto handleCandidate = [&](CalculatedCandidate&& candidate) {
    auto const& [weight, leftVertex, rightVertex] = candidate;

    _resultPath.clear();
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
  };

  auto checkCandidates = [&]() {
    /*
     * Helper method to take care of multiple candidates
     */
    TRI_ASSERT(searchDone());
    // ShortestPath not allowed because we cannot produce more than one
    // result in total.
    TRI_ASSERT(_options.getPathType() != PathType::Type::ShortestPath);

    if (!_candidatesStore.isEmpty()) {
      while (!_candidatesStore.isEmpty()) {
        CalculatedCandidate potentialCandidate = _candidatesStore.pop();

        // only add if non-duplicate
        bool foundNonDuplicatePath =
            _resultsCache.tryAddResult(potentialCandidate);

        if (foundNonDuplicatePath) {
          handleCandidate(std::move(potentialCandidate));
          return true;
        }
      }

      return false;
    }

    return false;
  };

  auto checkShortestPathCandidates = [&]() {
    TRI_ASSERT(searchDone());
    TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
    /*
     * Helper method to take care of ShortestPath related candidates
     * Only allowed to find and return exactly one path.
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
      searchMoreResults();
    }

    if (handleResult()) {
      if (_options.onlyProduceOnePath()) {
        // At this state we've produced a valid path result. In case we're
        // using the path type "(Weighted)ShortestPath", the algorithm is
        // finished. We need to store this information.

        TRI_ASSERT(_options.getPathType() == PathType::Type::ShortestPath);
        setAlgorithmFinished();  // just quick exit marker
      }
      return true;
    } else {
      // Check candidates list
      if (_options.getPathType() == PathType::Type::KShortestPaths) {
        return checkCandidates();
      } else {
        if (checkShortestPathCandidates()) {
          return true;
        };
      }
    }
  }

  TRI_ASSERT(isDone());
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

    if (matchPathLength > 0) {
      // means we've found a match (-1.0 means no match)
      if (_bestCandidateLength < 0 || matchPathLength < _bestCandidateLength) {
        _bestCandidateLength = matchPathLength;
      } else if (matchPathLength > _bestCandidateLength) {
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

        while (std::get<0>(_candidatesStore.peek()) < matchPathLength) {
          bool foundShortestPath = false;
          CalculatedCandidate potentialCandidate = _candidatesStore.pop();

          if (_options.getPathType() == PathType::Type::KShortestPaths) {
            foundShortestPath = _resultsCache.tryAddResult(potentialCandidate);
          } else if (_options.getPathType() == PathType::Type::ShortestPath) {
            // Performance optimization: We do not use the cache as we will
            // always calculate only one path.
            foundShortestPath = true;
          }

          if (foundShortestPath) {
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
            auto [weight, nextStepLeft, nextStepRight] =
                _candidatesStore.peek();
            _bestCandidateLength = weight;
          }
        }
      }
    }
  }

  if (_options.onlyProduceOnePath() && _bestCandidateLength >= 0) {
    fetchResult();
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
  auto skipResult = [&]() {
    /*
     * Helper method to take care of stored results (in: _results)
     */
    if (!_results.empty()) {
      if (_options.getPathType() == PathType::Type::ShortestPath) {
        ADB_PROD_ASSERT(_results.size() == 1)
            << "ShortestPath found more than one path. This is not allowed.";
      }

      // remove handled result
      _results.pop_front();

      return true;
    } else {
      return false;
    }
  };

  auto skipKPathsCandidates = [&]() {
    /*
     * Helper method to take care of KPath related candidates
     */
    TRI_ASSERT(searchDone());
    TRI_ASSERT(_options.getPathType() == PathType::Type::KShortestPaths);
    if (!_candidatesStore.isEmpty()) {
      while (!_candidatesStore.isEmpty()) {
        CalculatedCandidate potentialCandidate = _candidatesStore.pop();

        // only add if non-duplicate
        bool foundNonDuplicatePath =
            _resultsCache.tryAddResult(potentialCandidate);

        if (foundNonDuplicatePath) {
          // delete potentialCandidate;
          return true;
        }
      }

      return false;
    }

    return false;
  };

  while (!isDone()) {
    if (!searchDone()) {
      searchMoreResults();
    }

    if (skipResult()) {
      // means we've found a valid path
      if (_options.getPathType() == PathType::Type::ShortestPath) {
        setAlgorithmFinished();
      }

      return true;
    } else {
      // Check candidates list
      return skipKPathsCandidates();
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
                                PathValidator>::fetchResult() -> void {
  if (!_resultsFetched && !_candidatesStore.isEmpty()) {
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
