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

#pragma once

#include "Containers/HashSet.h"

#include "Basics/ResourceUsage.h"

#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/Types/ForbiddenVertices.h"

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
class TwoSidedEnumerator {
 public:
  using Step = typename ProviderType::Step;  // public due to tracer access

 private:
  enum Direction { FORWARD, BACKWARD };

  using VertexRef = arangodb::velocypack::HashedStringRef;
  using VertexSet =
      arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                    std::equal_to<VertexRef>>;
  using Edge = ProviderType::Step::EdgeType;
  using EdgeSet =
      arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>>;

  using Shell = std::multiset<Step>;
  using ResultList = std::vector<std::pair<Step, Step>>;
  using GraphOptions = arangodb::graph::TwoSidedEnumeratorOptions;

  class Ball {
   public:
    Ball(Direction dir, ProviderType&& provider, GraphOptions const& options,
         PathValidatorOptions validatorOptions,
         arangodb::ResourceMonitor& resourceMonitor);
    ~Ball();
    auto clear() -> void;
    auto reset(VertexRef center, size_t depth = 0) -> void;
    auto startNextDepth() -> void;
    [[nodiscard]] auto noPathLeft() const -> bool;
    [[nodiscard]] auto getDepth() const -> size_t;
    [[nodiscard]] auto shellSize() const -> size_t;
    [[nodiscard]] auto doneWithDepth() const -> bool;
    auto testDepthZero(Ball& other, ResultList& results) -> void;

    auto buildPath(Step const& vertexInShell,
                   PathResult<ProviderType, Step>& path) -> void;

    auto matchResultsInShell(Step const& match, ResultList& results,
                             PathValidatorType const& otherSideValidator)
        -> void;
    auto computeNeighbourhoodOfNextVertex(Ball& other, ResultList& results)
        -> void;

    // Ensure that we have fetched all vertices
    // in the _results list.
    // Otherwise we will not be able to
    // generate the resulting path
    auto fetchResults(ResultList& results) -> void;

    auto provider() -> ProviderType&;

    auto setForbiddenVertices(std::unique_ptr<VertexSet> forbidden)
        -> void requires HasForbidden<PathValidatorType> {
      _validator.setForbiddenVertices(std::move(forbidden));
    };

    auto setForbiddenEdges(std::unique_ptr<EdgeSet> forbidden)
        -> void requires HasForbidden<PathValidatorType> {
      _validator.setForbiddenEdges(std::move(forbidden));
    };

   private : auto clearProvider() -> void;
   private:
    // Fast path, to test if we find a connecting vertex between left and right.
    Shell _shell{};

    // TODO: Double check if we really need the monitor here. Currently unused.
    arangodb::ResourceMonitor& _resourceMonitor;

    // This stores all paths processed by this ball
    PathStoreType _interior;

    // The next elements to process
    QueueType _queue;

    ProviderType _provider;

    PathValidatorType _validator;

    size_t _depth{0};
    size_t _searchIndex{std::numeric_limits<size_t>::max()};
    Direction _direction;
    size_t _minDepth{0};
    GraphOptions _graphOptions;
  };

 public:
  TwoSidedEnumerator(ProviderType&& forwardProvider,
                     ProviderType&& backwardProvider,
                     TwoSidedEnumeratorOptions&& options,
                     PathValidatorOptions validatorOptions,
                     arangodb::ResourceMonitor& resourceMonitor);
  TwoSidedEnumerator(TwoSidedEnumerator const& other) = delete;
  TwoSidedEnumerator& operator=(TwoSidedEnumerator const& other) = delete;
  TwoSidedEnumerator(TwoSidedEnumerator&& other) = delete;
  TwoSidedEnumerator& operator=(TwoSidedEnumerator&& other) = delete;

  ~TwoSidedEnumerator();

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

  // The reference returned by the following call is only valid until
  // getNextPath is called again or until the TwoSidedEnumerator is destroyed
  // or otherwise modified!
  PathResult<ProviderType, typename ProviderType::Step> const&
  getLastPathResult() const {
    return _resultPath;
  }

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

  auto setForbiddenVertices(std::unique_ptr<VertexSet> forbidden)
      -> void requires HasForbidden<PathValidatorType> {
    auto copy = std::make_unique<VertexSet>(*forbidden);
    _left.setForbiddenVertices(std::move(copy));
    _right.setForbiddenVertices(std::move(forbidden));
  };

  auto setForbiddenEdges(std::unique_ptr<EdgeSet> forbidden)
      -> void requires HasForbidden<PathValidatorType> {
    auto copy = std::make_unique<EdgeSet>(*forbidden);
    _left.setForbiddenEdges(std::move(copy));
    _right.setForbiddenEdges(std::move(forbidden));
  };

  auto setEmitWeight(bool flag) { _emitWeight = flag; }

 private:
  [[nodiscard]] auto searchDone() const -> bool;
  auto startNextDepth() -> void;

  // Ensure that we have fetched all vertices
  // in the _results list.
  // Otherwise we will not be able to
  // generate the resulting path
  auto fetchResults() -> void;

  // Ensure that we have more valid paths in the _result stock.
  // May be a noop if _result is not empty.
  auto searchMoreResults() -> void;

  // In case we call this method, we know that we've already produced
  // enough results. This flag will be checked within the "isDone" method
  // and will provide a quick exit. Currently, this is only being used for
  // graph searches of type "Shortest Path".
  auto setAlgorithmFinished() -> void;
  auto setAlgorithmUnfinished() -> void;
  auto isAlgorithmFinished() const -> bool;

 private:
  GraphOptions _options;
  Ball _left;
  Ball _right;
  bool _searchLeft{true};
  ResultList _results{};
  bool _resultsFetched{false};
  size_t _baselineDepth;
  bool _algorithmFinished{false};
  bool _emitWeight{false};

  PathResult<ProviderType, Step> _resultPath;
};
}  // namespace graph
}  // namespace arangodb
