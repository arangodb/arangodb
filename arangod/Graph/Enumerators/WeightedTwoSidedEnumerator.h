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
#include "Graph/Types/ForbiddenVertices.h"
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

// This class `WeightedTwoSidedEnumerator` is used for legacy k-shortest-path
// searches, whenever the length is measured by an edge weight.
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
// A few words on uniqueness conditions are in order, since they are
// specified in the PathValidator, but have very strong interferences with
// the actual algorithm. For edges, there is either "no uniqueness"
// or "path uniqueness" (which means no edge may appear more than once
// on a single path), or "global uniqueness" (which means no edge may
// appear more than once in the whole traversal. For vertices, there is
// either "no uniqueness" or "path uniqueness" (no vertex may appear
// more than once on a single path), or "global uniqueness", which says
// that no vertex may occur more than once in the whole traversal.
// The great variety is only used for normal graph traversals, and so in
// particular not here in the `WeightedTwoSidedEnumerator`. Here, only one
// combination is in use:
//   vertex path uniqueness / edge path uniqueness
// It is for finding all possible loopless paths and edge uniqueness
// follows from vertex uniqueness. This is used for k-shortest-paths
// which do not use Yen's algorithm.
// Please note the following subtle issue: When enumerating paths (first
// combination above), the item on the queue is a "Step" (which encodes
// the path so far plus one more edge). In particular, there can and will
// be multiple Steps on the queue, which have arrived at the same vertex
// (with different edges or indeed different paths). This is necessary,
// since we have to enumerate all possible paths.

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidatorType>
class WeightedTwoSidedEnumerator {
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

  using Shell = std::multiset<Step>;
  using ResultList = std::deque<CalculatedCandidate>;
  using GraphOptions = arangodb::graph::TwoSidedEnumeratorOptions;

  /*
   * Weighted candidate store for handling path-match candidates in the order
   * from the lowest weight to the highest weight.
   */
  class CandidatesStore {
   public:
    void clear() {
      if (!_queue.empty()) {
        _queue.clear();
      }
    }

    void append(CalculatedCandidate candidate) {
      // if emplace() throws, no harm is done, and the memory usage increase
      // will be rolled back
      _queue.emplace_back(std::move(candidate));
      // std::push_heap takes the last element in the queue, assumes that all
      // other elements are in heap structure, and moves the last element into
      // the correct position in the heap (incl. rebalancing of other elements)
      // The heap structure guarantees that the first element in the queue
      // is the "largest" element (in our case it is the smallest, as we
      // inverted the comparator)
      std::push_heap(_queue.begin(), _queue.end(), _cmpHeap);
    }

    [[nodiscard]] size_t size() const { return _queue.size(); }

    [[nodiscard]] bool isEmpty() const { return _queue.empty(); }

    [[nodiscard]] std::vector<Step*> getLeftLooseEnds() {
      std::vector<Step*> steps;

      for (auto& [_, step, __] : _queue) {
        if (!step.isProcessable()) {
          steps.emplace_back(&step);
        }
      }

      return steps;
    }

    [[nodiscard]] std::vector<Step*> getRightLooseEnds() {
      std::vector<Step*> steps;

      for (auto& [_, __, step] : _queue) {
        if (!step.isProcessable()) {
          steps.emplace_back(&step);
        }
      }

      return steps;
    }

    [[nodiscard]] CalculatedCandidate& peek() {
      TRI_ASSERT(!_queue.empty());
      // will return a pointer to the step with the lowest weight amount
      // possible. The heap structure guarantees that the first element in the
      // queue is the "largest" element (in our case it is the smallest, as we
      // inverted the comparator)
      return _queue.front();
    }

    [[nodiscard]] CalculatedCandidate pop() {
      TRI_ASSERT(!isEmpty());
      // std::pop_heap will move the front element (the one we would like to
      // steal) to the back of the vector, keeping the tree intact otherwise.
      // Now we steal the last element.
      std::pop_heap(_queue.begin(), _queue.end(), _cmpHeap);
      CalculatedCandidate first = std::move(_queue.back());
      _queue.pop_back();
      return first;
    }

   private:
    struct WeightedComparator {
      bool operator()(CalculatedCandidate const& a,
                      CalculatedCandidate const& b) {
        auto const& [weightA, candAA, candAB] = a;
        auto const& [weightB, candBA, candBB] = b;
        return weightA > weightB;
      }
    };

    WeightedComparator _cmpHeap{};

    /// @brief queue datastore
    std::vector<CalculatedCandidate> _queue;
  };

  class Ball {
   public:
    Ball(Direction dir, ProviderType&& provider, GraphOptions const& options,
         PathValidatorOptions validatorOptions,
         arangodb::ResourceMonitor& resourceMonitor);
    ~Ball();
    auto clear() -> void;
    auto reset(VertexRef center, size_t depth = 0) -> void;
    [[nodiscard]] auto noPathLeft() const -> bool;
    [[nodiscard]] auto peekQueue() const -> Step const&;
    [[nodiscard]] auto isQueueEmpty() const -> bool;
    [[nodiscard]] auto doneWithDepth() const -> bool;
    [[nodiscard]] auto queueSize() const -> size_t { return _queue.size(); }

    auto buildPath(Step const& vertexInShell,
                   PathResult<ProviderType, Step>& path) -> void;

    auto matchResultsInShell(Step const& match, CandidatesStore& results,
                             PathValidatorType const& otherSideValidator)
        -> void;

    auto computeNeighbourhoodOfNextVertex(Ball& other, CandidatesStore& results)
        -> void;

    [[nodiscard]] auto hasBeenVisited(Step const& step) -> bool;
    auto validateSingletonPath(CandidatesStore& candidates) -> void;

    auto ensureQueueHasProcessableElement() -> void;

    // Ensure that we have fetched all vertices in the _results list.
    // Otherwise, we will not be able to generate the resulting path
    auto fetchResults(CandidatesStore& candidates) -> void;
    auto fetchResult(CalculatedCandidate& candidate) -> void;

    auto provider() -> ProviderType&;

    auto getDiameter() const noexcept -> double { return _diameter; }

    auto haveSeenOtherSide() const noexcept -> bool {
      return _haveSeenOtherSide;
    }

    auto setForbiddenVertices(std::shared_ptr<VertexSet> forbidden)
        -> void requires HasForbidden<PathValidatorType> {
      _validator.setForbiddenVertices(std::move(forbidden));
    };

    auto setForbiddenEdges(std::shared_ptr<EdgeSet> forbidden)
        -> void requires HasForbidden<PathValidatorType> {
      _validator.setForbiddenEdges(std::move(forbidden));
    };

    auto getCenter() const noexcept -> VertexRef { return _center; }

   private:
    auto clearProvider() -> void;

   private:
    // TODO: Double check if we really need the monitor here. Currently unused.
    arangodb::ResourceMonitor& _resourceMonitor;

    // This stores all paths processed by this ball
    PathStoreType _interior;

    // Center:
    VertexRef _center;

    // The next elements to process
    QueueType _queue;

    ProviderType _provider;

    PathValidatorType _validator;
    containers::FlatHashMap<typename Step::VertexType, std::vector<size_t>>
        _visitedNodes;
    Direction _direction;
    GraphOptions _graphOptions;
    double _diameter = -std::numeric_limits<double>::infinity();
    bool _haveSeenOtherSide;
  };
  enum BallSearchLocation { LEFT, RIGHT, FINISH };

  /*
   * A class that is able to store valid shortest paths results.
   * This is required to be able to check for duplicate paths, as those can be
   * found during a KShortestPaths graphs search.
   */
  class ResultCache {
   public:
    ResultCache(Ball& left, Ball& right);
    ~ResultCache();

    // @brief: returns whether a path could be inserted or not.
    // True: It will be inserted if this specific path has not been added yet.
    // False: Could not insert as this path has already been found.
    [[nodiscard]] auto tryAddResult(CalculatedCandidate const& candidate)
        -> bool;
    auto clear() -> void;

   private:
    Ball& _internalLeft;
    Ball& _internalRight;
    std::vector<PathResult<ProviderType, Step>> _internalResultsCache{};
  };

 public:
  WeightedTwoSidedEnumerator(ProviderType&& forwardProvider,
                             ProviderType&& backwardProvider,
                             TwoSidedEnumeratorOptions&& options,
                             PathValidatorOptions validatorOptions,
                             arangodb::ResourceMonitor& resourceMonitor);
  WeightedTwoSidedEnumerator(WeightedTwoSidedEnumerator const& other) = delete;
  WeightedTwoSidedEnumerator& operator=(
      WeightedTwoSidedEnumerator const& other) = delete;
  WeightedTwoSidedEnumerator(WeightedTwoSidedEnumerator&& other) = delete;
  WeightedTwoSidedEnumerator& operator=(WeightedTwoSidedEnumerator&& other) =
      delete;

  ~WeightedTwoSidedEnumerator();

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
  // getNextPath is called again or until the WeightedTwoSidedEnumerator
  // is destroyed or otherwise modified!
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

  auto setForbiddenVertices(std::shared_ptr<VertexSet> forbidden)
      -> void requires HasForbidden<PathValidatorType> {
    _left.setForbiddenVertices(forbidden);
    _right.setForbiddenVertices(std::move(forbidden));
  };

  auto setForbiddenEdges(std::shared_ptr<EdgeSet> forbidden)
      -> void requires HasForbidden<PathValidatorType> {
    _left.setForbiddenEdges(forbidden);
    _right.setForbiddenEdges(std::move(forbidden));
  };

 private : [[nodiscard]] auto searchDone() const -> bool;
  // Ensure that we have fetched all vertices in the _results list. Otherwise,
  // we will not be able to generate the resulting path
  auto fetchResults() -> void;
  auto fetchResult() -> void;

  // Ensure that we have more valid paths in the _result stock.
  // May be a noop if _result is not empty.
  auto searchMoreResults() -> void;

  // Check where we want to continue our search
  // (Left or right ball)
  auto getBallToContinueSearch() const -> BallSearchLocation;

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

  // Templated result list, where only valid result(s) are stored in
  CandidatesStore _candidatesStore{};
  ResultCache _resultsCache;
  ResultList _results{};

  bool _resultsFetched{false};
  bool _algorithmFinished{false};
  bool _singleton{false};

  PathResult<ProviderType, Step> _resultPath;
};
}  // namespace graph
}  // namespace arangodb
