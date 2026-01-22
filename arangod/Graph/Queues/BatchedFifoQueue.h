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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"
#include "Inspection/Format.h"
#include "Logger/LogMacros.h"
#include "Graph/Queues/QueueEntry.h"

#include <queue>
#include <variant>

namespace arangodb::graph {

/**
   Queue-like structure (first-in-first-out) that can contain either steps
   (which correspond to vertex-edges) or neighbour cursors to get more steps.

   Internally, this queue includes two queues, the main queue (_queue) that
   includes steps and the continuation queue (_continuationQueue) that includes
   the cursors. Basically, we want a queue that can include either a step or
   an cursor, but if a cursor gives more steps we would need to add these steps
   in the middle of the queue (where the responsible cursor sits). This is
   complicated to do, therefore _queue includes all steps that are ready and
   they are popped first. If _queue is empty when popping, the next cursor in
   the _continuationQueue is used to create more steps that are added to
   _queue.
*/
template<class StepType, NeighbourCursor<StepType> Cursor>
class BatchedFifoQueue {
 public:
  static constexpr bool RequiresWeight = false;
  using Step = StepType;
  // TODO: Add Sorting (Performance - will be implemented in the future -
  // cluster relevant)
  // -> loose ends to the end

  explicit BatchedFifoQueue(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor{resourceMonitor} {}
  ~BatchedFifoQueue() { this->clear(); }

  bool isBatched() { return true; }

  void clear() {
    if (!_queue.empty()) {
      _resourceMonitor.decreaseMemoryUsage(_queue.size() * sizeof(Step));
      _queue.clear();
    }
    if (!_continuationQueue.empty()) {
      _resourceMonitor.decreaseMemoryUsage(_continuationQueue.size() *
                                           sizeof(CursorRef));
      _continuationQueue.clear();
    }
  }

  void append(Step step) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Step));
    _queue.push_back(std::move(step));
    guard.steal();  // now we are responsible for tracking the memory
  }

  void append(Cursor& cursor) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(CursorRef));
    _continuationQueue.push_back(CursorRef{cursor});
    guard.steal();  // now we are responsible for tracking the memory
  }

  void setStartContent(std::vector<Step> startSteps) {
    arangodb::ResourceUsageScope guard(_resourceMonitor,
                                       sizeof(Step) * startSteps.size());
    TRI_ASSERT(isEmpty());
    for (auto& s : startSteps) {
      // For FIFO just append to the back,
      // The handed in vector will then be processed from start to end.
      // And appending would queue BEFORE items in the queue.
      _queue.push_back(std::move(s));
    }
    guard.steal();  // now we are responsible for tracking the memory
  }

  bool firstIsVertexFetched() const {
    if (!_queue.empty()) {
      auto const& first = _queue.front();
      return first.vertexFetched();
    }
    if (!_continuationQueue.empty()) {
      return true;
    }
    return false;
  }

  bool hasProcessableElement() const {
    if (!_queue.empty()) {
      auto const& first = _queue.front();
      return first.isProcessable();
    }
    return false;
  }

  bool isEmpty() const { return _queue.empty() && _continuationQueue.empty(); }

  std::vector<Step*> getLooseEnds() {
    TRI_ASSERT(!hasProcessableElement());

    std::vector<Step*> steps;
    for (auto& step : _queue) {
      if (!step.isProcessable()) {
        steps.emplace_back(&step);
      }
    }
    return steps;
  }

  Step const& peek() const {
    // Currently only implemented and used in WeightedQueue
    TRI_ASSERT(false);
    return _queue.front();
  }

  std::optional<Step> pop() {
    if (isEmpty()) {
      return std::nullopt;
    }
    if (not _queue.empty()) {
      auto first = std::move(_queue.front());
      LOG_TOPIC("9c2a4", TRACE, Logger::GRAPHS)
          << "<BatchedFifoQueue> Pop: " << first.toString();
      _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
      _queue.pop_front();
      return {first};
    }
    // if _queue is empty, pop next iterator
    LOG_TOPIC("0cda4", TRACE, Logger::GRAPHS)
        << "<BatchedFifoQueue> Pop: next batch";
    auto& cursor = _continuationQueue.front().get();
    if (not cursor.hasMore()) {
      _resourceMonitor.decreaseMemoryUsage(sizeof(CursorRef));
      _continuationQueue.pop_front();
      return pop();
    }
    for (auto&& step : cursor.next()) {
      append(step);
    }
    return pop();
  }

  std::vector<Step*> getStepsWithoutFetchedVertex() {
    std::vector<Step*> steps{};
    for (auto& step : _queue) {
      if (!step.vertexFetched()) {
        steps.emplace_back(&step);
      }
    }
    return steps;
  }

  void getStepsWithoutFetchedEdges(std::vector<Step*>& steps) {
    for (auto& step : _queue) {
      if (!step.edgeFetched() && !step.isUnknown()) {
        steps.emplace_back(&step);
      }
    }
  }
  template<class S, NeighbourCursor<S> C, typename Inspector>
  friend auto inspect(Inspector& f, BatchedFifoQueue<S, C>& x);

 private:
  /// @brief queue datastore
  using CursorRef = std::reference_wrapper<Cursor>;
  std::deque<Step> _queue;
  std::deque<CursorRef> _continuationQueue;

  /// @brief query context
  arangodb::ResourceMonitor& _resourceMonitor;
};
template<class StepType, NeighbourCursor<StepType> C, typename Inspector>
auto inspect(Inspector& f, BatchedFifoQueue<StepType, C>& x) {
  return f.object(x).fields(f.field("queue", x._queue),
                            f.field("continuations", x._continuationQueue));
}

}  // namespace arangodb::graph
