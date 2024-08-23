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

#include "YenEnumerator.h"

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
YenEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::
    YenEnumerator(ProviderType&& forwardProvider,
                  ProviderType&& backwardProvider,
                  TwoSidedEnumeratorOptions&& options,
                  PathValidatorOptions validatorOptions,
                  arangodb::ResourceMonitor& resourceMonitor)
    : _resourceMonitor(resourceMonitor), _isDone(false) {
  _shortestPathEnumerator = std::make_unique<ShortestPathEnumerator>(
      std::move(forwardProvider), std::move(backwardProvider),
      std::move(options), std::move(validatorOptions), resourceMonitor);
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
YenEnumerator<QueueType, PathStoreType, ProviderType,
              PathValidator>::~YenEnumerator() {}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::destroyEngines() -> void {
  _shortestPathEnumerator->destroyEngines();
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::clear() {
  _shortestPathEnumerator->clear();
  _shortestPaths.clear();
  _candidatePaths.clear();
  _isDone = false;
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::isDone() const {
  return _isDone;
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
void YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::reset(VertexRef source, VertexRef target,
                                         size_t depth) {
  _source = source;
  _target = target;
  clear();
  _shortestPathEnumerator->reset(source, target);
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
bool YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::getNextPath(VPackBuilder& result) {
  if (_isDone || !_shortestPaths.empty()) {
    return false;
  }
  // First find the shortest path using the _shortestPathEnumerator:
  bool found = _shortestPathEnumerator->getNextPath(result);
  if (!found) {
    return false;
  }
  // PathResult<ProviderType, typename ProviderType::Step> const& path =
  //     _shortestPathEnumerator->getLastPathResult;
  LOG_DEVEL << "Found one shortest path:" << result.slice().toJson();
  _isDone = true;
  return true;
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::skipPath() {
  VPackBuilder builder;
  // TODO, we might be able to improve this by not producing the result:
  return getNextPath(builder);
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
auto YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::stealStats() -> aql::TraversalStats {
  return _shortestPathEnumerator->stealStats();
}

// Explicit instantiations:

// SingleServerProvider Section:
using SingleServerProviderStep = ::arangodb::graph::SingleServerProviderStep;

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::FifoQueue<SingleServerProviderStep>,
    ::arangodb::graph::PathStore<SingleServerProviderStep>,
    SingleServerProvider<SingleServerProviderStep>,
    ::arangodb::graph::PathValidator<
        SingleServerProvider<SingleServerProviderStep>,
        PathStore<SingleServerProviderStep>, VertexUniquenessLevel::PATH,
        EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::QueueTracer<
        ::arangodb::graph::FifoQueue<SingleServerProviderStep>>,
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

// ClusterProvider Section:

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::FifoQueue<ClusterProviderStep>,
    ::arangodb::graph::PathStore<ClusterProviderStep>,
    ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::PathValidator<
        ClusterProvider<ClusterProviderStep>, PathStore<ClusterProviderStep>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::QueueTracer<
        ::arangodb::graph::FifoQueue<ClusterProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<ClusterProviderStep>>,
    ::arangodb::graph::ProviderTracer<ClusterProvider<ClusterProviderStep>>,
    ::arangodb::graph::PathValidator<
        ::arangodb::graph::ProviderTracer<ClusterProvider<ClusterProviderStep>>,
        ::arangodb::graph::PathStoreTracer<
            ::arangodb::graph::PathStore<ClusterProviderStep>>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;
