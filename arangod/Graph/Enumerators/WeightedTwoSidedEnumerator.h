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

#pragma once

#include "Containers/HashSet.h"

#include "Basics/ResourceUsage.h"

#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathResult.h"
#include "Containers/FlatHashMap.h"

// TODO: CLEAN THIS UP LATER move implementation to cpp file remove this include
#include "Assertions/Assert.h"

#include <set>
#include <deque>
#include <unordered_map>

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
class WeightedTwoSidedEnumerator {
 public:
  using Step = typename ProviderType::Step;  // public due to tracer access

  // A meeting point with calculated path weight
  using CalculatedCandidate = std::tuple<double, Step, Step>;

 private:
  enum Direction { FORWARD, BACKWARD };

  using VertexRef = arangodb::velocypack::HashedStringRef;

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

    [[nodiscard]] std::vector<CalculatedCandidate> getQueue() const {
      return _queue;
    };

    [[nodiscard]] CalculatedCandidate& peek() {
      TRI_ASSERT(!_queue.empty());
      // will return a pointer to the step with the lowest weight amount
      // possible. The heap structure guarantees that the first element in the
      // queue is the "largest" element (in our case it is the smallest, as we
      // inverted the comparator)
      return _queue.front();
    }

    CalculatedCandidate pop() {
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
        if (weightA == weightB) {
          return weightA > weightB;
        }
        return weightA > weightB;
      }
    };

    WeightedComparator _cmpHeap{};

    /// @brief queue datastore
    /// Note: Mutable is a required for hasProcessableElement right now which is
    /// const. We can easily make it non const here.
    mutable std::vector<CalculatedCandidate> _queue;
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

    auto buildPath(Step const& vertexInShell,
                   PathResult<ProviderType, Step>& path) -> void;

    auto matchResultsInShell(Step const& match, CandidatesStore& results,
                             PathValidatorType const& otherSideValidator)
        -> double;

    // @brief returns a positive double in a match has been found.
    // returns -1.0 if no match has been found.
    auto computeNeighbourhoodOfNextVertex(Ball& other, CandidatesStore& results)
        -> double;

    auto hasBeenVisited(Step const& step) -> bool;

    // Ensure that we have fetched all vertices in the _results list.
    // Otherwise, we will not be able to generate the resulting path
    auto fetchResults(CandidatesStore& candidates) -> void;
    auto fetchResult(CalculatedCandidate& candidate) -> void;

    auto provider() -> ProviderType&;

   private:
    auto clearProvider() -> void;

   private:
    // TODO: Double check if we really need the monitor here. Currently unused.
    arangodb::ResourceMonitor& _resourceMonitor;

    // This stores all paths processed by this ball
    PathStoreType _interior;

    // The next elements to process
    QueueType _queue;

    ProviderType _provider;

    PathValidatorType _validator;
    std::unordered_map<typename Step::VertexType, std::vector<size_t>>
        _visitedNodes;
    Direction _direction;
    GraphOptions _graphOptions;
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
    auto addResult(CalculatedCandidate const& candidate) -> bool;
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
  [[nodiscard]] auto searchDone() const -> bool;

  // Ensure that we have fetched all vertices
  // in the _results list.
  // Otherwise we will not be able to
  // generate the resulting path
  auto fetchResults() -> void;
  auto fetchResult(double key) -> void;

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

  auto setInitialFetchVerified() -> void;
  auto getInitialFetchVerified() -> bool;
  [[nodiscard]] auto isAlgorithmFinished() const -> bool;

 private:
  GraphOptions _options;
  Ball _left;
  Ball _right;

  // We always start with two vertices (start- and end vertex)
  // Initially, we want to fetch both and then decide based on the
  // initial results, where to continue our search.
  bool _leftInitialFetch{false};   // TODO: Put this into the ball?
  bool _rightInitialFetch{false};  // TODO: Put this into the ball?
  // Bool to check whether we've verified our initial fetched steps
  // or not. This is an optimization. Only during our initial _left and
  // _right fetch it may be possible that we find matches, which are valid
  // paths - but not the shortest one. Therefore, we need to compare with both
  // queues. After that check - we always pull the minStep from both queues.
  // After init, this check is no longer required as we will always have the
  // smallest (in terms of path-weight) step in our hands.
  bool _handledInitialFetch{false};

  // Templated result list, where only valid result(s) are stored in
  CandidatesStore _candidatesStore{};
  ResultCache _resultsCache;
  ResultList _results{};
  double _bestCandidateLength = -1.0;

  bool _resultsFetched{false};
  bool _algorithmFinished{false};

  PathResult<ProviderType, Step> _resultPath;
};
}  // namespace graph
}  // namespace arangodb
