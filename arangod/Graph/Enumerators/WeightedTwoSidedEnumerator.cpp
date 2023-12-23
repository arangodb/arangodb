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
#include "Graph/PathType.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Types/UniquenessLevel.h"
#include "Graph/Types/ValidationResult.h"
#include "Graph/algorithm-aliases.h"
#include "Logger/LogMacros.h"

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

template<typename Configuration>
WeightedTwoSidedEnumerator<Configuration>::Ball::Ball(
    Direction dir, Provider&& provider, GraphOptions const& options,
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

template<typename Configuration>
WeightedTwoSidedEnumerator<Configuration>::Ball::~Ball() = default;

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::Ball::reset(VertexRef center,
                                                            size_t depth) {
  clear();
  auto firstStep = _provider.startVertex(center, depth);
  _queue.append(std::move(firstStep));
}

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::Ball::clear() {
  _visitedNodes.clear();

  _queue.clear();
  _interior.reset();  // PathStore
  _diameter = -std::numeric_limits<double>::infinity();
  _validator.reset();

  // Provider - Must be last one to be cleared(!)
  clearProvider();
}

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::Ball::clearProvider() {
  // We need to make sure, no one holds references to _provider.
  // Guarantee that the used Queue is empty and we do not hold any reference to
  // PathStore. Info: Steps do contain VertexRefs which are hold in PathStore.
  TRI_ASSERT(_queue.isEmpty());

  // Guarantee that the used PathStore is cleared, before we clear the Provider.
  // The Provider does hold the StringHeap cache.
  TRI_ASSERT(_interior.size() == 0);

  _provider.clear();
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::doneWithDepth() const
    -> bool {
  return _queue.isEmpty();
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::noPathLeft() const
    -> bool {
  return doneWithDepth();
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::peekQueue() const
    -> Step const& {
  return _queue.peek();
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::isQueueEmpty() const
    -> bool {
  return _queue.isEmpty();
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::fetchResult(
    CalculatedCandidate& candidate) -> void {
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

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::fetchResults(
    CandidatesStore& candidates) -> void {
  auto looseEnds = [&]() {
    switch (_direction) {
      case Direction::FORWARD: {
        return candidates.getLeftLooseEnds();
      }
      case Direction::BACKWARD: {
        return candidates.getRightLooseEnds();
      }
      default: {
        TRI_ASSERT(false);
      }
    }
  }();

  if (!looseEnds.empty()) {
    // Will throw all network errors here
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);
    futureEnds.get();
  }
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::hasBeenVisited(
    Step const& step) -> bool {
  if (_visitedNodes.contains(step.getVertex().getID())) {
    return true;
  }
  return false;
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<
    Configuration>::Ball::ensureQueueHasProcessableElement() -> void {
  TRI_ASSERT(!_queue.isEmpty());
  if (!_queue.hasProcessableElement()) {
    std::vector<Step*> looseEnds = _queue.getLooseEnds();
    futures::Future<std::vector<Step*>> futureEnds = _provider.fetch(looseEnds);

    // Will throw all network errors here
    auto&& preparedEnds = futureEnds.get();

    TRI_ASSERT(preparedEnds.size() != 0);
  }
  TRI_ASSERT(_queue.hasProcessableElement());
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::validateSingletonPath(
    CandidatesStore& candidates) -> void {
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

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::
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

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::matchResultsInShell(
    Step const& otherStep, CandidatesStore& candidates,
    Validator const& otherSideValidator) -> void {
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

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::buildPath(
    Step const& vertexInShell, PathResult<Provider, Step>& path) -> void {
  if (_direction == FORWARD) {
    _interior.buildPath(vertexInShell, path);
  } else {
    _interior.reverseBuildPath(vertexInShell, path);
  }
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::Ball::provider() -> Provider& {
  return _provider;
}

/*
 * Class: WeightedTwoSidedEnumerator
 */

template<typename Configuration>
WeightedTwoSidedEnumerator<Configuration>::WeightedTwoSidedEnumerator(
    Provider&& forwardProvider, Provider&& backwardProvider,
    TwoSidedEnumeratorOptions&& options, PathValidatorOptions validatorOptions,
    arangodb::ResourceMonitor& resourceMonitor)
    : _options(std::move(options)),
      _left{Direction::FORWARD, std::move(forwardProvider), _options,
            validatorOptions, resourceMonitor},
      _right{Direction::BACKWARD, std::move(backwardProvider), _options,
             std::move(validatorOptions), resourceMonitor},
      _resultPath{_left.provider(), _right.provider()} {}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::destroyEngines() -> void {
  // Note: Left & Right Provider use the same traversal engines.
  //   => Destroying one of them is enough.
  _left.provider().destroyEngines();
}

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::clear() {
  // Order is important here, please do not change.
  // 1.) Remove current results & state
  _candidatesStore.clear();

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
template<typename Configuration>
bool WeightedTwoSidedEnumerator<Configuration>::isDone() const {
  // #if 0
  switch (_options.getPathType()) {
    case PathType::Type::ShortestPath: {
      return (_candidatesStore.isEmpty() && searchDone()) ||
             isAlgorithmFinished();
    } break;
    default: {
      if (!_candidatesStore.isEmpty()) {
        return false;
      }
    } break;
  }
  // #endif

  return (_candidatesStore.isEmpty() &&
          searchDone());  // || isAlgorithmFinished();
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
template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::reset(VertexRef source,
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
template<typename Configuration>
bool WeightedTwoSidedEnumerator<Configuration>::getNextPath(
    VPackBuilder& result) {
  while (!isDone()) {
    if (!searchDone()) {
      searchMoreResults();
    }

    if (_candidatesStore.isEmpty()) {
      return false;
    } else {
      auto [weight, leftVertex, rightVertex] = _candidatesStore.pop();

      _resultPath.clear();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);
      TRI_ASSERT(!_resultPath.isEmpty());

      if (_options.getPathType() == PathType::Type::KShortestPaths) {
        // Add weight attribute to edges
        _resultPath.toVelocyPack(
            result, PathResult<Provider, Step>::WeightType::ACTUAL_WEIGHT);
      } else {
        _resultPath.toVelocyPack(result);
        setAlgorithmFinished();
      }

      return true;
    }
  }
  TRI_ASSERT(isDone());
  return false;
}

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::searchMoreResults() {
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

    // if the sum of the diameters of left and right search are bigger
    // than the best candidate, there will not be a better candidate found.
    //
    // A simple shortest path search is done *now* (and not earlier!);
    //
    // It is *required* to continue search for a shortest path even
    // after having found *some* path between the two searches:
    // There might be improvements on the weight in paths that are
    // found later. Improvements are impossible only if the sum of
    // the diameters of the two searches is smaller than the current
    // best found path.
    //
    // For a K-SHORTEST-PATH search all candidates that have lower weight
    // than the sum of the two diameters are valid shortest paths that must
    // be returned.
    // K-SHORTEST-PATH search has to continue until the queues on both
    // sides are empty

    auto leftDiameter = _left.getDiameter();
    auto rightDiameter = _right.getDiameter();
    auto sumDiameter = leftDiameter + rightDiameter;

    if (!_candidatesStore.isEmpty() &&
        std::get<0>(_candidatesStore.peek()) < sumDiameter) {
      if (_options.getPathType() == PathType::Type::ShortestPath) {
        // Proven to be finished with the algorithm. Our last best score
        // is the shortest path (quick exit).
        setAlgorithmFinished();
      }
      break;
    }
  }

  if (_options.onlyProduceOnePath()) {
    fetchResult();
  } else {
    fetchResults();
  }
}

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::setAlgorithmFinished() {
  _algorithmFinished = true;
}

template<typename Configuration>
void WeightedTwoSidedEnumerator<Configuration>::setAlgorithmUnfinished() {
  _algorithmFinished = false;
}

template<typename Configuration>
bool WeightedTwoSidedEnumerator<Configuration>::isAlgorithmFinished() const {
  return _algorithmFinished;
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */
template<typename Configuration>
bool WeightedTwoSidedEnumerator<Configuration>::skipPath() {
  while (!isDone()) {
    if (!searchDone()) {
      searchMoreResults();
    }

    if (_candidatesStore.isEmpty()) {
      return false;
    } else {
      std::ignore = _candidatesStore.pop();
      if (_options.getPathType() == PathType::Type::ShortestPath) {
        setAlgorithmFinished();
      }
      return true;
    }
  }
  return false;
}

template<typename Configuration>
typename WeightedTwoSidedEnumerator<Configuration>::BallSearchLocation
WeightedTwoSidedEnumerator<Configuration>::getBallToContinueSearch() const {
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

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::searchDone() const -> bool {
  return (_left.noPathLeft() && _right.noPathLeft()) || isAlgorithmFinished();
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::fetchResults() -> void {
  if (!_resultsFetched && !_candidatesStore.isEmpty()) {
    _left.fetchResults(_candidatesStore);
    _right.fetchResults(_candidatesStore);
  }
  _resultsFetched = true;
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::fetchResult() -> void {
  if (!_resultsFetched && !_candidatesStore.isEmpty()) {
    _left.fetchResult(_candidatesStore.peek());
    _right.fetchResult(_candidatesStore.peek());
  }
  _resultsFetched = true;
}

template<typename Configuration>
auto WeightedTwoSidedEnumerator<Configuration>::stealStats()
    -> aql::TraversalStats {
  aql::TraversalStats stats = _left.provider().stealStats();
  stats += _right.provider().stealStats();
  return stats;
}

/* SingleServerProvider Section */
using SingleServerProviderT =
    SingleServerProvider<::arangodb::graph::SingleServerProviderStep>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    WeightedPathSearch<SingleServerProviderT, VertexUniquenessLevel::PATH,
                       EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    WeightedPathSearch<SingleServerProviderT, VertexUniquenessLevel::GLOBAL,
                       EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    TracedWeightedPathSearch<SingleServerProviderT, VertexUniquenessLevel::PATH,
                             EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    TracedWeightedPathSearch<SingleServerProviderT,
                             VertexUniquenessLevel::GLOBAL,
                             EdgeUniquenessLevel::PATH>>;

/* ClusterProvider Section */

using ClusterProviderT = ClusterProvider<ClusterProviderStep>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<WeightedPathSearch<
    ClusterProviderT, VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    TracedWeightedPathSearch<ClusterProviderT, VertexUniquenessLevel::PATH,
                             EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    WeightedPathSearch<ClusterProviderT, VertexUniquenessLevel::GLOBAL,
                       EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    TracedWeightedPathSearch<ClusterProviderT, VertexUniquenessLevel::GLOBAL,
                             EdgeUniquenessLevel::PATH>>;
