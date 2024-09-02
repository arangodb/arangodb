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
    : _arena(resourceMonitor),
      _resourceMonitor(resourceMonitor),
      _isDone(true),
      _isInitialized(false) {
  // Yen's algorithm only ever uses the TwoSidedEnumerator here to find
  // exactly one shortest path:
  options.setOnlyProduceOnePath(true);
  options.setPathType(PathType::Type::ShortestPath);
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
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::isDone() const {
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
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
void YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::reset(VertexRef source, VertexRef target,
                                         size_t depth) {
  _source = source;
  _target = target;
  clear();
  _isDone = false;
  _isInitialized = true;
}

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
PathResult<ProviderType, typename ProviderType::Step>
YenEnumerator<QueueType, PathStoreType, ProviderType, PathValidator>::toOwned(
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
    copy.appendEdge(e);
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
template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidator>
bool YenEnumerator<QueueType, PathStoreType, ProviderType,
                   PathValidator>::getNextPath(VPackBuilder& result) {
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
    LOG_DEVEL << "Found one shortest path:" << result.slice().toJson();
    PathResult<ProviderType, typename ProviderType::Step> const& path =
        _shortestPathEnumerator->getLastPathResult();
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
  auto& prevPath = _shortestPaths.back();
  auto len = prevPath.getLength();
  for (size_t prefixLen = 0; prefixLen < len; ++prefixLen) {
    auto spurVertex = prevPath.getVertex(prefixLen);
    // To avoid cycles, forbid all vertices before the spurVertex in the
    // previous path:
    auto forbiddenVertices = std::make_unique<VertexSet>();
    for (size_t i = 0; i < prefixLen; ++i) {
      forbiddenVertices->insert(prevPath.getVertex(i).getID());
    }
    // To avoid finding old shortest paths again, we must forbid every edge,
    // which is a continuation of a previous shortest path which has the
    // same prefix:
    auto forbiddenEdges = std::make_unique<EdgeSet>();
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
    _shortestPathEnumerator->setForbiddenVertices(std::move(forbiddenVertices));
    _shortestPathEnumerator->setForbiddenEdges(std::move(forbiddenEdges));

    _shortestPathEnumerator->clear();  // needed, otherwise algorithm
                                       // finished remains!
    _shortestPathEnumerator->reset(spurVertex.getID(), _target);
    VPackBuilder temp;
    if (_shortestPathEnumerator->getNextPath(temp)) {
      LOG_DEVEL << "Found another shortest path:" << temp.slice().toJson();
      PathResult<ProviderType, typename ProviderType::Step> const& path =
          _shortestPathEnumerator->getLastPathResult();
      PathResult<ProviderType, typename ProviderType::Step> newPath{
          path.getSourceProvider(), path.getTargetProvider()};  // empty path
      for (size_t i = 0; i < prefixLen; ++i) {
        newPath.appendVertex(prevPath.getVertex(i));
        newPath.appendEdge(prevPath.getEdge(i));
      }
      newPath.addWeight(1.0 * prefixLen);
      for (size_t i = 0; i < path.getLength(); ++i) {
        newPath.appendVertex(path.getVertex(i));
        newPath.appendEdge(path.getEdge(i));
      }
      newPath.appendVertex(path.getVertex(path.getLength()));
      newPath.addWeight(1.0 * path.getLength());
      // Note that we must copy all vertex and edge data and make them
      // our own. Otherwise, once the providers are cleared, the references
      // might no longer be valid!
      auto copy = std::make_unique<
          PathResult<ProviderType, typename ProviderType::Step>>(
          toOwned(newPath));
      _candidatePaths.emplace_back(std::move(copy));
    }
  }

  // Finally get the best candidate:
  if (_candidatePaths.empty()) {
    _isDone = true;
    return false;
  }

  size_t posBest = 0;
  for (size_t i = 1; i < _candidatePaths.size(); ++i) {
    if (_candidatePaths[i]->getWeight() <
        _candidatePaths[posBest]->getWeight()) {
      posBest = i;
    }
  }
  _shortestPaths.emplace_back(std::move(*_candidatePaths[posBest]));
  _shortestPaths.back().toVelocyPack(result);
  _candidatePaths.erase(_candidatePaths.begin() + posBest);

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
    ::arangodb::graph::PathValidatorTabooWrapper<
        ::arangodb::graph::PathValidator<
            SingleServerProvider<SingleServerProviderStep>,
            PathStore<SingleServerProviderStep>, VertexUniquenessLevel::GLOBAL,
            EdgeUniquenessLevel::PATH>>>;

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::QueueTracer<
        ::arangodb::graph::FifoQueue<SingleServerProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<SingleServerProviderStep>>,
    ::arangodb::graph::ProviderTracer<
        SingleServerProvider<SingleServerProviderStep>>,
    ::arangodb::graph::PathValidatorTabooWrapper<
        ::arangodb::graph::PathValidator<
            ::arangodb::graph::ProviderTracer<
                SingleServerProvider<SingleServerProviderStep>>,
            ::arangodb::graph::PathStoreTracer<
                ::arangodb::graph::PathStore<SingleServerProviderStep>>,
            VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>>;

// ClusterProvider Section:

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::FifoQueue<ClusterProviderStep>,
    ::arangodb::graph::PathStore<ClusterProviderStep>,
    ClusterProvider<ClusterProviderStep>,
    ::arangodb::graph::PathValidatorTabooWrapper<
        ::arangodb::graph::PathValidator<ClusterProvider<ClusterProviderStep>,
                                         PathStore<ClusterProviderStep>,
                                         VertexUniquenessLevel::GLOBAL,
                                         EdgeUniquenessLevel::PATH>>>;

template class ::arangodb::graph::YenEnumerator<
    ::arangodb::graph::QueueTracer<
        ::arangodb::graph::FifoQueue<ClusterProviderStep>>,
    ::arangodb::graph::PathStoreTracer<
        ::arangodb::graph::PathStore<ClusterProviderStep>>,
    ::arangodb::graph::ProviderTracer<ClusterProvider<ClusterProviderStep>>,
    ::arangodb::graph::PathValidatorTabooWrapper<
        ::arangodb::graph::PathValidator<
            ::arangodb::graph::ProviderTracer<
                ClusterProvider<ClusterProviderStep>>,
            ::arangodb::graph::PathStoreTracer<
                ::arangodb::graph::PathStore<ClusterProviderStep>>,
            VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>>;
