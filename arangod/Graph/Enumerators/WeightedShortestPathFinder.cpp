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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "WeightedShortestPathFinder.h"

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
 * Class: WeightedShortestPathFinder
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                           PathValidator>::
    WeightedShortestPathFinder(ProviderType&& forwardProvider,
                               ProviderType&& backwardProvider,
                               PathValidatorOptions validatorOptions,
                               arangodb::ResourceMonitor& resourceMonitor)
    : _forward{std::move(forwardProvider), validatorOptions, resourceMonitor},
      _backward{std::move(backwardProvider), std::move(validatorOptions),
                resourceMonitor},
      _resultPath{_forward.provider(), _backward.provider()} {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                           PathValidator>::~WeightedShortestPathFinder() {
  // TODO: check that destroyEngines was called?
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::destroyEngines() -> void {
  // Note: Left & Right Provider use the same traversal engines.
  //   => Destroying one of them is enough.
  _forward.provider().destroyEngines();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::clear() {
  // Order is important here, please do not change.
  // 1.) Remove current results & state
  _bestCandidate = std::nullopt;
  _handledInitialFetch = false;

  // 2.) Remove both Balls (order here is not important)
  _forward.clear();
  _backward.clear();

  _resultPath.clear();

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
bool WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::isDone() const {
  return searchDone();
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
void WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::reset(VertexRef source,
                                                      VertexRef target) {
  clear();
  _forward.reset(source);
  _backward.reset(target);
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
bool WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::getNextPath(VPackBuilder&
                                                                result) {
  while (!isDone()) {
    if (!searchDone()) {
      searchMoreResults();
    }

    _resultPath.clear();
    if (_bestCandidate.has_value()) {
      _forward.buildPath(_bestCandidate->left, _resultPath);
      _backward.buildPath(_bestCandidate->right, _resultPath);
      TRI_ASSERT(!_resultPath.isEmpty());

      _resultPath.toVelocyPack(result);

      return true;
    } else {
      return false;
    }
  }

  TRI_ASSERT(isDone());
  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchMoreResults() {
  while (!searchDone()) {
    double matchPathLength = -1.0;

    auto searchLocation = getBallToContinueSearch();
    switch (searchLocation) {
      case BallSearchLocation::FORWARD: {
        // might need special case depth == 0
        matchPathLength = _forward.computeNeighbourhoodOfNextVertex();
      } break;

      case BallSearchLocation::BACKWARD: {
        // special case for initial step expansion
        // this needs to only be checked once and only _backward as we always
        // start _forward with our search. If that behaviour changed, this
        // verification needs to be moved as well.

        if (!getInitialFetchVerified()) {
          setInitialFetchVerified();
        }

        matchPathLength = _backward.computeNeighbourhoodOfNextVertex();
      } break;

      case BallSearchLocation::FINISH: {
        // Our queue is empty. We cannot produce more results.
        setAlgorithmFinished();
      } break;
    }

    if (matchPathLength > 0 and (_bestCandidate.has_value() == false or
                                 _bestCandidate->weight < matchPathLength)) {
      _bestCandidate->weight = matchPathLength;

      if (_forward.getRadius() + _backward.getRadius() >
          _bestCandidate->weight) {
        // Found the best path
        fetchResult();
      }
    }
  }
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::skipPath() {
  while (!isDone()) {
    if (!searchDone()) {
      searchMoreResults();
    }

    if (_bestCandidate.has_value()) {
      return true;
    } else {
      return false;
    }
  }

  return false;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::getBallToContinueSearch()
    -> BallSearchLocation {
  if (!_forward.isQueueEmpty() && !_backward.isQueueEmpty()) {
    if (_forward.getRadius() <= _backward.getRadius()) {
      return BallSearchLocation::FORWARD;
    } else {
      return BallSearchLocation::BACKWARD;
    }
  } else if (!_forward.isQueueEmpty() && _backward.isQueueEmpty()) {
    return BallSearchLocation::FORWARD;
  } else if (_forward.isQueueEmpty() && !_backward.isQueueEmpty()) {
    return BallSearchLocation::BACKWARD;
  }

  // Default
  return BallSearchLocation::FINISH;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::searchDone() const -> bool {
  return _forward.noPathLeft() || _backward.noPathLeft() ||
         isAlgorithmFinished();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::fetchResult() -> void {
  //  _forward.fetchResult(_bestCandidate.left);
  // _backward.fetchResult(_bestCandidate.right);
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto WeightedShortestPathFinder<QueueType, PathStoreType, ProviderType,
                                PathValidator>::stealStats()
    -> aql::TraversalStats {
  aql::TraversalStats stats = _forward.provider().stealStats();
  stats += _backward.provider().stealStats();
  return stats;
}

/* SingleServerProvider Section */
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::WeightedShortestPathFinder<
    ::arangodb::graph::WeightedQueue<SingleServerProviderStep>,
    ::arangodb::graph::PathStore<SingleServerProviderStep>,
    SingleServerProvider<SingleServerProviderStep>,
    ::arangodb::graph::PathValidator<
        SingleServerProvider<SingleServerProviderStep>,
        PathStore<SingleServerProviderStep>, VertexUniquenessLevel::PATH,
        EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedShortestPathFinder<
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

template class ::arangodb::graph::WeightedShortestPathFinder<
    ::arangodb::graph::WeightedQueue<::arangodb::graph::ClusterProviderStep>,
    ::arangodb::graph::PathStore<ClusterProviderStep>,
    ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::PathValidator<
        ClusterProvider<ClusterProviderStep>, PathStore<ClusterProviderStep>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedShortestPathFinder<
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
