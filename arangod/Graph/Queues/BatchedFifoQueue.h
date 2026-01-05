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
#include "Graph/Queues/ExpansionMarker.h"

#include <queue>
#include <variant>

namespace arangodb::graph {

/**
   Queue-like structure (first-in-first-out) that can contain either steps
   (which correspond to vertex-edges) or expansions that promise more
   vertex-edges for a specific vertex.

   Internally, this queue includes two queues, the main queue (_queue) that
   includes steps and the continuation queue (_continuationQueue) that includes
   the expansions. Basically, we want a queue that can include either a step or
   an expansion, but if an expansion is epxanded, we need to add steps in the
   middle of the queue (where the responsible expansion sits). This is
   complicated to do, therefore _queue includes all steps that are ready and
   they are popped first. If _queue is empty when popping, an expansion from the
   _continuationQueue is popped. From code that uses this BatchedFifoQueue this
   expansion can then be expanded and these expansion steps can the be pushed
   again to the queue (where they directly go into the main _queue).
 */
template<class StepType>
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
                                           sizeof(Expansion));
      _continuationQueue.clear();
    }
  }

  void append(Step step) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Step));
    _queue.push_back({std::move(step)});
    guard.steal();  // now we are responsible for tracking the memory
  }

  void append(Expansion expansion) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Expansion));
    auto cursorId = expansion.id;
    if (_biggestCursorId.has_value() && cursorId <= _biggestCursorId.value()) {
      // iterator already existed, put it at front! of continuation queue
      _continuationQueue.push_front(std::move(expansion));
    } else {
      // new iterator
      _biggestCursorId = cursorId;
      _continuationQueue.push_back(std::move(expansion));
    }
    guard.steal();  // now we are responsible for tracking the memory
  }

  void setStartContent(std::vector<Step> startSteps) {
    arangodb::ResourceUsageScope guard(_resourceMonitor,
                                       sizeof(Step) * startSteps.size());
    TRI_ASSERT(_queue.empty());
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

  QueueEntry<Step> pop() {
    TRI_ASSERT(!isEmpty());
    // if _queue is empty, pop next iterator
    if (_queue.empty()) {
      auto first = std::move(_continuationQueue.front());
      LOG_TOPIC("0cda4", TRACE, Logger::GRAPHS)
          << "<BatchedFifoQueue> Pop: next batch";
      _resourceMonitor.decreaseMemoryUsage(sizeof(Expansion));
      _continuationQueue.pop_front();
      return {first};
    }
    auto first = std::move(_queue.front());
    LOG_TOPIC("9c2a4", TRACE, Logger::GRAPHS)
        << "<BatchedFifoQueue> Pop: " << first.toString();
    _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
    _queue.pop_front();
    return {first};
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
  template<class S, typename Inspector>
  friend auto inspect(Inspector& f, BatchedFifoQueue<S>& x);

 private:
  /// @brief queue datastore
  std::deque<Step> _queue;
  std::deque<Expansion> _continuationQueue;
  std::optional<size_t> _biggestCursorId;

  /// @brief query context
  arangodb::ResourceMonitor& _resourceMonitor;
};
template<class StepType, typename Inspector>
auto inspect(Inspector& f, BatchedFifoQueue<StepType>& x) {
  return f.object(x).fields(f.field("queue", x._queue),
                            f.field("continuations", x._continuationQueue));
}

}  // namespace arangodb::graph
