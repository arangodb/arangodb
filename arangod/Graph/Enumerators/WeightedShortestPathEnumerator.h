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

#include "Assertions/Assert.h"
#include "Basics/ResourceUsage.h"
#include "Containers/HashSet.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Containers/FlatHashMap.h"

#include <limits>
#include <set>
#include <deque>

namespace arangodb {

using VertexRef = arangodb::velocypack::HashedStringRef;
using VertexSet = arangodb::containers::HashSet<VertexRef, std::hash<VertexRef>,
                                                std::equal_to<VertexRef>>;

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

// This class `WeightedTwoSidedEnumerator` is used for shortest path searches,
// whenever the // length is measured by an edge weight.
// It works by doing a Dijkstra-like graph traversal from both sides and
// then matching findings. As work queue it uses a priority queue, always
// processing the next unprocessed step according to the queue.
// This class is used in very different situations (single server, cluster,
// various different types of smart and not so smart graphs, with tracing
// and without, etc.). Therefore we need many template parameters. Let me
// here give an overview over what they do:
//  - QueueType: This is the queue being used to track which steps to visit
//    next. It is always `WeightedQueue`, but it needs to be a template
//    argument since there is a wrapper template for tracing `QueueTracer`,
//    so it is sometimes QueueTracer<WeightedQueue>.
//  - PathStoreType: This is a class to store paths. Its type depends on
//    the type ProviderType::Step (see below) and on the presence of a
//    tracing wrapper type.
//  - ProviderType: This is a class which delivers the actual graph data,
//    essentially to answer the question as to what the neighbours of a
//    vertex are. This can be `SingleServerProvider` or `ClusterProvider`.
//    Again, there is a tracing wrapper.
//  - PathValidatorType: Finally, this is a class which is used to validate
//    if paths are valid. Various filtering conditions can be handed in,
//    but the most important one is to specify the uniqueness conditions
//    on edges and vertices. Again, there is a tracing wrapper.
//    For this class, the vertex uniqueness condition must be GLOBAL and
//    the edge uniqueness condition must be PATH.
// Please note the following subtle issue: When enumerating paths (first
// combination above), the item on the queue is a "Step" (which encodes
// the path so far plus one more edge). In particular, there can and will
// be multiple Steps on the queue, which have arrived at the same vertex
// (with different edges or indeed different paths). This is necessary,
// since we have to enumerate all possible paths.
// Since we are only looking for a shortest path, we use global vertex
// uniqueness. However, the implementation is slightly different from a
// standard Dijkstra algorithm as can be found in the literature. Namely,
// some vertex V can indeed be found in different ways, and in this case
// multiple Steps to reach it will in this case be put on the queue. This
// is to get the accounting of the weight of the different ways to reach
// this vertex right. Therefore, we must not check the validity of the path
// **when we explore a new step and put it on the queue**. Rather, we check
// path validity only when we **visit** a step to explore all next steps!
// That is, we have no "reduce weight" operation when we find a new path
// to a vertex which has already been visited, but we administrate both
// Steps (the shorter and the longer path), the shorter path's Step will
// be earlier in the WeightedQueue and thus will be visited earlier.
// The other Step will then be later in the queue and when we would otherwise
// visit it, we will check validity of the path and will then not visit it,
// since global vertex uniqueness is violated.
// This could eventually be improved but for now we run with it.
// Note that the path type in the TwoSidedEnumeratorOptions must always
// be "ShortestPath" for this class here to work.

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidatorType>
class WeightedShortestPathEnumerator {
 public:
  using Step = typename ProviderType::Step;  // public due to tracer access

  // A meeting point with calculated path weight
  using CalculatedCandidate = std::tuple<double, Step, Step>;

 private:
  enum Direction { FORWARD, BACKWARD };

  using VertexRef = arangodb::velocypack::HashedStringRef;

  using Edge = ProviderType::Step::EdgeType;
  using EdgeSet =
      arangodb::containers::HashSet<Edge, std::hash<Edge>, std::equal_to<Edge>>;

  using GraphOptions = arangodb::graph::TwoSidedEnumeratorOptions;

  class Ball {
   public:
    Ball(Direction dir, ProviderType&& provider, GraphOptions const& options,
         PathValidatorOptions validatorOptions,
         arangodb::ResourceMonitor& resourceMonitor);
    ~Ball();
    auto clear() -> void;
    auto reset(VertexRef center, size_t depth = 0) -> void;
    [[nodiscard]] auto isQueueEmpty() const -> bool;
    [[nodiscard]] auto queueSize() const -> size_t { return _queue.size(); }

    auto buildPath(Step const& vertexInShell,
                   PathResult<ProviderType, Step>& path) -> void;

    auto matchResultsInShell(Step const& match,
                             std::optional<CalculatedCandidate>& bestPath,
                             PathValidatorType const& otherSideValidator)
        -> void;

    auto computeNeighbourhoodOfNextVertex(
        Ball& other, std::optional<CalculatedCandidate>& bestPath) -> void;

    [[nodiscard]] auto hasBeenVisited(Step const& step) -> bool;
    auto validateSingletonPath(std::optional<CalculatedCandidate>& bestPath)
        -> void;

    auto ensureQueueHasProcessableElement() -> void;

    // Ensure that we have fetched all vertices in the _results list.
    // Otherwise, we will not be able to generate the resulting path
    auto fetchResult(CalculatedCandidate& candidate) -> void;

    auto provider() -> ProviderType&;

    auto getDiameter() const noexcept -> double { return _diameter; }

    auto setForbiddenVertices(std::shared_ptr<VertexSet> forbidden) -> void {
      _forbiddenVertices = std::move(forbidden);
    };

    auto setForbiddenEdges(std::shared_ptr<EdgeSet> forbidden) -> void {
      _forbiddenEdges = std::move(forbidden);
    };

    auto getCenter() const noexcept -> VertexRef { return _center; }

    auto getQueued() const noexcept -> size_t { return _queued; }
    auto getExpanded() const noexcept -> size_t { return _expanded; }

   private:
    auto clearProvider() -> void;

   private:
    // TODO: Double check if we really need the monitor here. Currently unused.
    arangodb::ResourceMonitor& _resourceMonitor;

    // This stores all paths processed by this ball
    PathStoreType _interior;

    // The center:
    VertexRef _center;

    // The next elements to process
    QueueType _queue;

    size_t _queued = 0;
    size_t _expanded = 0;

    ProviderType _provider;

    PathValidatorType _validator;
    struct VertexInfo {
      VertexInfo(double w)
          : weight(w), position(0), expanded(false), cancelled(false) {}
      double weight;
      size_t position;  // this is only set once `expanded` is true
                        // it refers to the position in _interior
                        // once the vertex has been expanded.
      bool expanded;    // This is set to true if a vertex has been expanded.
      bool cancelled;   // This is set to true if a vertex has been found
                        // with a lower weight than the current one and
                        // yet no new Step has been queued for it. We can
                        // then prevent further expansion of this vertex
                        // without deleting its Step with the wrong
                        // weight from the queue.
    };
    containers::FlatHashMap<typename Step::VertexType, VertexInfo>
        _foundVertices;
    Direction _direction;
    GraphOptions _graphOptions;
    double _diameter = -std::numeric_limits<double>::infinity();
    std::shared_ptr<VertexSet> _forbiddenVertices;
    std::shared_ptr<EdgeSet> _forbiddenEdges;
  };
  enum BallSearchLocation { LEFT, RIGHT, FINISH };

 public:
  WeightedShortestPathEnumerator(ProviderType&& forwardProvider,
                                 ProviderType&& backwardProvider,
                                 TwoSidedEnumeratorOptions&& options,
                                 PathValidatorOptions validatorOptions,
                                 arangodb::ResourceMonitor& resourceMonitor);
  WeightedShortestPathEnumerator(WeightedShortestPathEnumerator const& other) =
      delete;
  WeightedShortestPathEnumerator& operator=(
      WeightedShortestPathEnumerator const& other) = delete;
  WeightedShortestPathEnumerator(WeightedShortestPathEnumerator&& other) =
      delete;
  WeightedShortestPathEnumerator& operator=(
      WeightedShortestPathEnumerator&& other) = delete;

  ~WeightedShortestPathEnumerator();

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
  // getNextPath is called again or until the WeightedShortestPathEnumerator
  // is destroyed or otherwise modified!
  PathResult<ProviderType, typename ProviderType::Step> const&
  getLastPathResult() const {
    return _resultPath;
  }

  auto destroyEngines() -> void;

  /**
   * @brief Return statistics generated since
   * the last time this method was called.
   */
  auto stealStats() -> aql::TraversalStats;

  auto setForbiddenVertices(std::shared_ptr<VertexSet> forbidden) -> void {
    _left.setForbiddenVertices(forbidden);
    _right.setForbiddenVertices(std::move(forbidden));
  };

  auto setForbiddenEdges(std::shared_ptr<EdgeSet> forbidden) -> void {
    _left.setForbiddenEdges(forbidden);
    _right.setForbiddenEdges(std::move(forbidden));
  };

  auto setEmitWeight(bool flag) -> void { _emitWeight = flag; }

 private:
  [[nodiscard]] auto searchDone() const -> bool;
  // Ensure that we have fetched all vertices in the _results list. Otherwise,
  // we will not be able to generate the resulting path
  auto fetchResult() -> void;

  // Ensure that we have more valid paths in the _result stock.
  // May be a noop if _result is not empty.
  auto searchMoreResults() -> void;

  // Check where we want to continue our search
  // (Left or right ball)
  auto getBallToContinueSearch() -> BallSearchLocation;

  // In case we call this method, we know that we've already produced
  // enough results. This flag will be checked within the "isDone" method
  // and will provide a quick exit. Currently, this is only being used for
  // graph searches of type "Shortest Path".
  auto setAlgorithmFinished() -> void;
  auto setAlgorithmUnfinished() -> void;
  [[nodiscard]] auto isAlgorithmFinished() const -> bool;

 private:
  GraphOptions _options;
  Ball _left;
  Ball _right;

  std::optional<CalculatedCandidate> _bestPath{std::nullopt};

  bool _resultsFetched{false};
  bool _algorithmFinished{false};
  bool _singleton{false};
  bool _emitWeight{false};

  PathResult<ProviderType, Step> _resultPath;
};
}  // namespace graph
}  // namespace arangodb
