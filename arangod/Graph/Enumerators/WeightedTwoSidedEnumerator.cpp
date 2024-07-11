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
#include <limits>

using namespace arangodb;
using namespace arangodb::graph;

namespace {

bool almostEqual(double x, double y) {
  if (x == y) {
    return true;
  }

  auto diff = std::abs(x - y);
  auto norm =
      std::min((std::abs(x) + std::abs(y)), std::numeric_limits<double>::max());
  return diff < std::max(std::numeric_limits<double>::round_error(),
                         std::numeric_limits<double>::epsilon() * norm);
}
}  // namespace

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
      _graphOptions(options),
      _diameter(-std::numeric_limits<double>::infinity()) {}

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
  _diameter = -std::numeric_limits<double>::infinity();
  _validator.reset();

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
    futureEnds.waitAndGet();
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::fetchResults(CandidatesStore& candidates) -> void {
  auto looseEnds = [&]() {
    if (_direction == FORWARD) {
      return candidates.getLeftLooseEnds();
    }

    ADB_PROD_ASSERT(_direction == Direction::BACKWARD);
    return candidates.getRightLooseEnds();
  }();

  if (!looseEnds.empty()) {
    // Will throw all network errors here
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    futureEnds.waitAndGet();
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
auto WeightedTwoSidedEnumerator<
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
auto WeightedTwoSidedEnumerator<
    QueueType, PathStoreType, ProviderType,
    PathValidator>::Ball::validateSingletonPath(CandidatesStore& candidates)
    -> void {
  ensureQueueHasProcessableElement();
  auto tmp = _queue.pop();

  TRI_ASSERT(_queue.isEmpty());

  auto posPrevious = _interior.append(std::move(tmp));
  auto& step = _interior.getStepReference(posPrevious);
  ValidationResult res = _validator.validatePath(step);

  if (!res.isFiltered()) {
    candidates.append(std::make_tuple(0.0, step, step));
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    computeNeighbourhoodOfNextVertex(Ball& other, CandidatesStore& candidates)
        -> void {
  ensureQueueHasProcessableElement();
  auto tmp = _queue.pop();

  // if the other side has explored this vertex, don't add it again
  if (!other.hasBeenVisited(tmp)) {
    auto posPrevious = _interior.append(std::move(tmp));
    auto& step = _interior.getStepReference(posPrevious);

    TRI_ASSERT(step.getWeight() >= _diameter);
    _diameter = step.getWeight();
    ValidationResult res = _validator.validatePath(step);

    if (!res.isFiltered()) {
      _visitedNodes[step.getVertex().getID()].emplace_back(posPrevious);
    }

    if (!res.isPruned()) {
      _provider.expand(step, posPrevious, [&](Step n) -> void {
        // TODO: maybe the pathStore could be asked whether a vertex has been
        // visited?
        if (other.hasBeenVisited(n)) {
          other.matchResultsInShell(n, candidates, _validator);
        }
        _queue.append(std::move(n));
      });
    }
  }
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::Ball::
    matchResultsInShell(Step const& otherStep, CandidatesStore& candidates,
                        PathValidator const& otherSideValidator) -> void {
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
  }
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
  _resultsCache.clear();
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
  if (_options.getPathType() == PathType::Type::KShortestPaths) {
    if (!_candidatesStore.isEmpty()) {
      return false;
    }
  }

  return _results.empty() && searchDone();
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

  // TODO: this is not ideal; here's the issue: If source == target there is no
  // search to be done as there is only *at most* one shortest path between a
  // vertex and itself: the path of length and weight 0. If the vertex does not
  // fulfill the global vertex condition, there is none. So the global vertex
  // condition has to be evaluated! This is why the _left ball is used here.
  //
  // Admittedly, this choice is arbitrary: in our context a path is a sequence
  // of edges that does not repeat vertices. Otherwise this path search would
  // have to return all cycles based at the source == target vertex. This can be
  // implemented using a OneSidedEnumerator if ever requested.
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

    if (_singleton) {
      _left.validateSingletonPath(_candidatesStore);
      setAlgorithmFinished();
    } else {
      auto searchLocation = getBallToContinueSearch();
      if (searchLocation == BallSearchLocation::LEFT) {
        _left.computeNeighbourhoodOfNextVertex(_right, _candidatesStore);
      } else if (searchLocation == BallSearchLocation::RIGHT) {
        _right.computeNeighbourhoodOfNextVertex(_left, _candidatesStore);
      } else {
        TRI_ASSERT(searchLocation == BallSearchLocation::FINISH);
        // Our queue is empty. We cannot produce more results.
        setAlgorithmFinished();
      }
    }

    if (!_candidatesStore.isEmpty()) {
      auto bestWeight = std::get<0>(_candidatesStore.peek());
      // if the sum of the diameters of left and right search are
      // bigger than the best candidate, there will not be a better
      // candidate found.
      //
      // A simple shortest path search is done *now* (and not
      // earlier!);
      //
      // It is *required* to continue search for a shortest path even
      // after having found *some* path between the two searches:
      // There might be improvements on the weight in paths that are
      // found later. Improvements are impossible only if the sum of the
      // diameters of the two searches is bigger or equal to the current
      // best found path.
      //
      // For a K-SHORTEST-PATH search all candidates that have lower
      // weight than the sum of the two diameters are valid shortest
      // paths that must be returned.  K-SHORTEST-PATH search has to
      // continue until the queues on both sides are empty

      auto leftDiameter = _left.getDiameter();
      auto rightDiameter = _right.getDiameter();
      auto sumDiameter = leftDiameter + rightDiameter;

      if (sumDiameter >= bestWeight) {
        while (!_candidatesStore.isEmpty() and
               std::get<0>(_candidatesStore.peek()) < sumDiameter) {
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
          }
        }
      }
    }
  }

  if (_options.onlyProduceOnePath() /* found a candidate */) {
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
                           PathValidator>::getBallToContinueSearch() const {
  if (_left.isQueueEmpty() and _right.isQueueEmpty()) {
    return BallSearchLocation::FINISH;
  }

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

  // From here both _left and _right are guaranteed to not be empty.
  if (almostEqual(_left.peekQueue().getWeight(), _left.getDiameter())) {
    return BallSearchLocation::LEFT;
  }

  if (almostEqual(_right.peekQueue().getWeight(), _right.getDiameter())) {
    return BallSearchLocation::RIGHT;
  }

  if (_left.getDiameter() <= _right.getDiameter()) {
    return BallSearchLocation::LEFT;
  } else {
    return BallSearchLocation::RIGHT;
  }

  // Default
  return BallSearchLocation::FINISH;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedTwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchDone() const -> bool {
  return (_left.noPathLeft() && _right.noPathLeft()) || isAlgorithmFinished();
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

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    ::arangodb::graph::WeightedQueue<SingleServerProviderStep>,
    ::arangodb::graph::PathStore<SingleServerProviderStep>,
    SingleServerProvider<SingleServerProviderStep>,
    ::arangodb::graph::PathValidator<
        SingleServerProvider<SingleServerProviderStep>,
        PathStore<SingleServerProviderStep>, VertexUniquenessLevel::GLOBAL,
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
        VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

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

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    ::arangodb::graph::WeightedQueue<::arangodb::graph::ClusterProviderStep>,
    ::arangodb::graph::PathStore<ClusterProviderStep>,
    ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::PathValidator<
        ClusterProvider<ClusterProviderStep>, PathStore<ClusterProviderStep>,
        VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

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
        VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;
