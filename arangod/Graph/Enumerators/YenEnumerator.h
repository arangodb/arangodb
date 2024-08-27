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

#pragma once

#include "Containers/HashSet.h"

#include "Basics/ResourceUsage.h"

#include "Graph/EdgeDocumentToken.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Types/UniquenessLevel.h"

#include <set>

namespace arangodb {

namespace aql {
class TraversalStats;
}

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {

class PathValidatorOptions;
struct TwoSidedEnumeratorOptions;

template<class ProviderType, class Step>
class PathResult;

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidatorType>
class YenEnumerator {
  enum Direction { FORWARD, BACKWARD };

  using VertexRef = ProviderType::Step::VertexType;
  using Edge = ProviderType::Step::EdgeType;

  using VertexSet =
      arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                    std::equal_to<VertexRef>>;
  using EdgeSet =
      arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>>;

  using GraphOptions = arangodb::graph::TwoSidedEnumeratorOptions;

  // This is also in algorith-aliases.h, but we must not include this here,
  // since it also contains aliases for YenEnumerator!
  using ShortestPathEnumerator =
      TwoSidedEnumerator<QueueType, PathStoreType, ProviderType,
                         PathValidatorType>;

 public:
  YenEnumerator(ProviderType&& forwardProvider, ProviderType&& backwardProvider,
                TwoSidedEnumeratorOptions&& options,
                PathValidatorOptions validatorOptions,
                arangodb::ResourceMonitor& resourceMonitor);
  YenEnumerator(YenEnumerator const& other) = delete;
  YenEnumerator& operator=(YenEnumerator const& other) = delete;
  YenEnumerator(YenEnumerator&& other) = delete;
  YenEnumerator& operator=(YenEnumerator&& other) = delete;

  ~YenEnumerator();

  auto clear() -> void;

  /**
   * @brief Quick test if the finder can prove there is no more data available.
   *        It can respond with false, even though there is no path left.
   * @return true There will be no further path.
   * @return false There is a chance that there is more data available.
   */
  [[nodiscard]] bool isDone() const;

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
  void reset(VertexRef source, VertexRef target, size_t depth = 0);

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
  bool getNextPath(arangodb::velocypack::Builder& result);

  /**
   * @brief Skip the next Path, like getNextPath, but does not return the path.
   *
   * @return true Found and skipped a path.
   * @return false No path found.
   */

  bool skipPath();
  auto destroyEngines() -> void;

  /**
   * @brief Return statistics generated since
   * the last time this method was called.
   */
  auto stealStats() -> aql::TraversalStats;

 private:
  std::unique_ptr<ShortestPathEnumerator> _shortestPathEnumerator;
  std::vector<PathResult<ProviderType, typename ProviderType::Step>>
      _shortestPaths;
  std::vector<
      std::unique_ptr<PathResult<ProviderType, typename ProviderType::Step>>>
      _candidatePaths;
  arangodb::ResourceMonitor& _resourceMonitor;
  bool _isDone;  // shortcut to indicate all is done
  VertexRef _source;
  VertexRef _target;
  bool _isInitialized;
};
}  // namespace graph
}  // namespace arangodb
