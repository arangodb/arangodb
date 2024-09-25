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
/// @author Max Neunhoeffer
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

#include <utility>

using namespace arangodb;
using namespace arangodb::graph;

template<class ProviderType, class EnumeratorType, bool IsWeighted>
YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::YenEnumerator(
    ProviderType&& forwardProvider, ProviderType&& backwardProvider,
    TwoSidedEnumeratorOptions&& options, PathValidatorOptions validatorOptions,
    arangodb::ResourceMonitor& resourceMonitor)
    : _arena(resourceMonitor),
      _resourceMonitor(resourceMonitor),
      _totalMemoryUsageHere(0),
      _isDone(true),
      _isInitialized(false) {
  // Yen's algorithm only ever uses the TwoSidedEnumerator here to find
  // exactly one shortest path:
  options.setOnlyProduceOnePath(true);
  options.setPathType(PathType::Type::ShortestPath);
  _shortestPathEnumerator = std::make_unique<EnumeratorType>(
      std::forward<ProviderType>(forwardProvider),
      std::forward<ProviderType>(backwardProvider), std::move(options),
      std::move(validatorOptions), resourceMonitor);
  _shortestPathEnumerator->setEmitWeight(true);
}

template<class ProviderType, class EnumeratorType, bool IsWeighted>
YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::~YenEnumerator() {
  _resourceMonitor.decreaseMemoryUsage(_totalMemoryUsageHere);
}

template<class ProviderType, class EnumeratorType, bool IsWeighted>
auto YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::destroyEngines()
    -> void {
  _shortestPathEnumerator->destroyEngines();
}

template<class ProviderType, class EnumeratorType, bool IsWeighted>
void YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::clear() {
  _shortestPathEnumerator->clear();
  _shortestPaths.clear();
  _candidatePaths.clear();
  _arena.clear();
  _isDone = true;
  _isInitialized = false;
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template<class ProviderType, class EnumeratorType, bool IsWeighted>
bool YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::isDone() const {
  // This is more subtle than first meets the eye: If we are not yet
  // initialized, we must return `true`.
  // This is necessary such that the EnumeratePathsExecutor works. Once
  // we are initialized with `reset()`, we return `true` once we have
  // proved that no further path will be found. Note that it might be that
  // we have returned `false` and yet no further path is found.
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
template<class ProviderType, class EnumeratorType, bool IsWeighted>
void YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::reset(
    VertexRef source, VertexRef target, size_t depth) {
  _source = source;
  _target = target;
  clear();
  _isDone = false;
  _isInitialized = true;
}

template<class ProviderType, class EnumeratorType, bool IsWeighted>
PathResult<ProviderType, typename ProviderType::Step>
YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::toOwned(
    PathResult<ProviderType, typename ProviderType::Step> const& path) {
  PathResult<ProviderType, typename ProviderType::Step> copy{
      path.getSourceProvider(), path.getTargetProvider()};  // empty path
  size_t l = path.getLength();
  for (size_t i = 0; i < l + 1; ++i) {
    typename ProviderType::Step::Vertex v{
        _arena.toOwned(path.getVertex(i).getID())};
    copy.appendVertex(v);
    if (i == l) {
      break;  // one more vertex than edge
    }
    typename ProviderType::Step::Edge e{
        _arena.toOwned(path.getEdge(i).getID())};
    copy.appendEdge(e, path.getWeight(i));
  }
  copy.addWeight(path.getWeight());
  return copy;
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
template<class ProviderType, class EnumeratorType, bool IsWeighted>
bool YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::getNextPath(
    VPackBuilder& result) {
  if (!_isInitialized) {
    return false;
  }
  if (_isDone) {
    return false;
  }
  if (_shortestPaths.empty()) {
    // First find the shortest path using the _shortestPathEnumerator:
    _shortestPathEnumerator->reset(_source, _target);
    bool found = _shortestPathEnumerator->getNextPath(result);
    if (!found) {
      _isDone = true;
      return false;
    }
    auto const& path = _shortestPathEnumerator->getLastPathResult();
    _shortestPaths.emplace_back(toOwned(path));  // Copy the path with all
                                                 // its referenced data!
    // When we are called next, we will continue below!
    return true;
  }
  // Here comes the code to find the next shortest path: We must try all
  // proper prefixes of the previous shortest path and start a shortest
  // path computation for each prefix with some forbidden vertices and
  // edges. This then adds to the candidates and in the end we either
  // take the best candidate or have proven that no more shortest paths
  // exist.
  auto const& prevPath = _shortestPaths.back();
  auto const len = prevPath.getLength();
  for (size_t prefixLen = 0; prefixLen < len; ++prefixLen) {
    auto spurVertex = prevPath.getVertex(prefixLen);
    // To avoid cycles, forbid all vertices before the spurVertex in the
    // previous path:
    auto forbiddenVertices = std::make_shared<VertexSet>();
    for (size_t i = 0; i < prefixLen; ++i) {
      forbiddenVertices->insert(prevPath.getVertex(i).getID());
    }
    // To avoid finding old shortest paths again, we must forbid every edge,
    // which is a continuation of a previous shortest path which has the
    // same prefix:
    auto forbiddenEdges = std::make_shared<EdgeSet>();
    forbiddenEdges->insert(prevPath.getEdge(prefixLen).getID());
    // This handles the previous one, now do the ones before:
    for (size_t i = 0; i + 1 < _shortestPaths.size(); ++i) {
      // Check if that shortest path has the same prefix:
      if (_shortestPaths[i].getLength() > prefixLen) {
        bool samePrefix = true;
        for (size_t j = 0; j < prefixLen; ++j) {
          if (!_shortestPaths[i].getEdge(j).getID().equals(
                  prevPath.getEdge(j).getID())) {
            samePrefix = false;
            break;
          }
        }
        if (samePrefix) {
          forbiddenEdges->insert(_shortestPaths[i].getEdge(prefixLen).getID());
        }
      }
    }
    // And run a shortest path computation from the spur vertex to the sink
    // with forbidden vertices and edges:
    _shortestPathEnumerator->clear();  // needed, otherwise algorithm
                                       // finished remains!
    _shortestPathEnumerator->reset(spurVertex.getID(), _target);
    _shortestPathEnumerator->setForbiddenVertices(std::move(forbiddenVertices));
    _shortestPathEnumerator->setForbiddenEdges(std::move(forbiddenEdges));

    VPackBuilder temp;
    if (_shortestPathEnumerator->getNextPath(temp)) {
      PathResult<ProviderType, typename ProviderType::Step> const& path =
          _shortestPathEnumerator->getLastPathResult();
      auto newPath = std::make_unique<
          PathResult<ProviderType, typename ProviderType::Step>>(
          path.getSourceProvider(), path.getTargetProvider());  // empty path
      for (size_t i = 0; i < prefixLen; ++i) {
        newPath->appendVertex(prevPath.getVertex(i));
        auto weight = prevPath.getWeight(i);
        newPath->appendEdge(prevPath.getEdge(i), weight);
        newPath->addWeight(weight);
      }
      for (size_t i = 0; i < path.getLength(); ++i) {
        newPath->appendVertex(path.getVertex(i));
        auto weight = path.getWeight(i);
        newPath->appendEdge(path.getEdge(i), weight);
        newPath->addWeight(weight);
      }
      newPath->appendVertex(path.getVertex(path.getLength()));

      // We are about to add the new path to the set of candidates,
      // but we only want to add it if the same path is not already
      // part of the set of candidates (because it was added in an
      // earlier iteration). If the candidates would include twice the
      // same path, the user would possibly get this path twice if they
      // requested enough paths. This can happen because we only forbid
      // edges from already found shortest paths but not from candidates.

      auto lb = std::lower_bound(_candidatePaths.begin(), _candidatePaths.end(),
                                 newPath, pathComparator);
      if (lb == _candidatePaths.end() || !pathEquals(*lb, newPath)) {
        // Note that we must copy all vertex and edge data and make them
        // our own. Otherwise, once the providers are cleared, the references
        // might no longer be valid!
        auto copy = std::make_unique<
            PathResult<ProviderType, typename ProviderType::Step>>(
            toOwned(*newPath));
        size_t mem = copy->getMemoryUsage();
        _resourceMonitor.increaseMemoryUsage(mem);
        _totalMemoryUsageHere += mem;
        _candidatePaths.insert(lb, std::move(copy));
      }
    }
  }

  if (_candidatePaths.empty()) {
    _isDone = true;
    return false;
  }

  // Finally get the best candidate, this will always be the last:
  _shortestPaths.emplace_back(std::move(*(_candidatePaths.back())));
  _candidatePaths.pop_back();
  if constexpr (IsWeighted) {
    _shortestPaths.back().toVelocyPack(
        result,
        PathResult<ProviderType,
                   typename ProviderType::Step>::WeightType::ACTUAL_WEIGHT);
  } else {
    _shortestPaths.back().toVelocyPack(
        result,
        PathResult<ProviderType,
                   typename ProviderType::Step>::WeightType::AMOUNT_EDGES);
  }

  return true;
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template<class ProviderType, class EnumeratorType, bool IsWeighted>
bool YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::skipPath() {
  VPackBuilder builder;
  return getNextPath(builder);
}

template<class ProviderType, class EnumeratorType, bool IsWeighted>
auto YenEnumerator<ProviderType, EnumeratorType, IsWeighted>::stealStats()
    -> aql::TraversalStats {
  return _shortestPathEnumerator->stealStats();
}

// Explicit instantiations:

// SingleServerProvider Section:

using SingleProvider = SingleServerProvider<SingleServerProviderStep>;

template class ::arangodb::graph::YenEnumerator<
    SingleProvider, ShortestPathEnumeratorForYen<SingleProvider>, false>;

template class ::arangodb::graph::YenEnumerator<
    ProviderTracer<SingleProvider>,
    TracedShortestPathEnumeratorForYen<SingleProvider>, false>;

template class ::arangodb::graph::YenEnumerator<
    SingleProvider, WeightedShortestPathEnumeratorForYen<SingleProvider>, true>;

template class ::arangodb::graph::YenEnumerator<
    ProviderTracer<SingleProvider>,
    TracedWeightedShortestPathEnumeratorForYen<SingleProvider>, true>;

// ClusterProvider Section:

using ClustProvider = ClusterProvider<ClusterProviderStep>;

template class ::arangodb::graph::YenEnumerator<
    ClustProvider, ShortestPathEnumeratorForYen<ClustProvider>, false>;

template class ::arangodb::graph::YenEnumerator<
    ProviderTracer<ClustProvider>,
    TracedShortestPathEnumeratorForYen<ClustProvider>, false>;

template class ::arangodb::graph::YenEnumerator<
    ClustProvider, WeightedShortestPathEnumeratorForYen<ClustProvider>, true>;

template class ::arangodb::graph::YenEnumerator<
    ProviderTracer<ClustProvider>,
    TracedWeightedShortestPathEnumeratorForYen<ClustProvider>, true>;
