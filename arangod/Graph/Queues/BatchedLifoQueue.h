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

namespace arangodb {
namespace graph {

/**
 Stack-like structure (last-in-first-out) that can contain either steps
 (which correspond to vertex-edges) or neighbour cursors to get more steps.
*/
template<class StepType, NeighbourCursor<StepType> Cursor>
class BatchedLifoQueue {
 public:
  static constexpr bool RequiresWeight = false;
  using Step = StepType;
  // TODO: Add Sorting (Performance - will be implemented in the future -
  // cluster relevant) -> loose ends to the end

  explicit BatchedLifoQueue(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor{resourceMonitor} {}
  ~BatchedLifoQueue() { this->clear(); }

  bool isBatched() { return true; }

  void clear() {
    if (!_queue.empty()) {
      _resourceMonitor.decreaseMemoryUsage(_queue.size() * sizeof(Entry));
      _queue.clear();
    }
  }

  void append(Step step) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Entry));
    // if push_front() throws, no harm is done, and the memory usage increase
    // will be rolled back
    _queue.push_front({std::move(step)});
    guard.steal();  // now we are responsible for tracking the memory
  }

  void append(Cursor& cursor) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Entry));
    // if push_front() throws, no harm is done, and the memory usage increase
    // will be rolled back
    _queue.push_front({std::reference_wrapper<Cursor>(cursor)});
    guard.steal();  // now we are responsible for tracking the memory
  }

  void setStartContent(std::vector<Step> startSteps) {
    arangodb::ResourceUsageScope guard(_resourceMonitor,
                                       sizeof(Entry) * startSteps.size());
    TRI_ASSERT(_queue.empty());
    for (auto& s : startSteps) {
      // For LIFO just append to the back,
      // The handed in vector will then be processed from start to end.
      // And appending would queue BEFORE items in the queue.
      _queue.push_back({std::move(s)});
    }
    guard.steal();  // now we are responsible for tracking the memory
  }

  bool hasProcessableElement() const {
    if (!isEmpty()) {
      auto const& first = _queue.front();
      if (std::holds_alternative<Step>(first)) {
        return std::get<Step>(first).isProcessable();
      }
    }

    return false;
  }

  size_t size() const { return _queue.size(); }

  bool isEmpty() const { return _queue.empty(); }

  std::vector<Step*> getLooseEnds() {
    TRI_ASSERT(!hasProcessableElement());

    std::vector<Step*> steps;
    for (auto& item : _queue) {
      if (std::holds_alternative<Step>(item)) {
        auto step = std::get<Step>(item);
        if (!step.isProcessable()) {
          steps.emplace_back(&step);
        }
      }
    }

    return steps;
  }

  Step const& peek() const {
    // Currently only implemented and used in WeightedQueue
    TRI_ASSERT(false);
    auto const& first = _queue.front();
    return std::get<Step>(first);
  }

  std::optional<Step> pop() {
    if (isEmpty()) {
      return std::nullopt;
    }
    auto first = _queue.front();
    LOG_TOPIC("9cda4", TRACE, Logger::GRAPHS)
        << "<BatchedLifoQueue> Pop: "
        << (std::holds_alternative<Step>(first)
                ? std::get<Step>(first).toString()
                : "next batch");
    if (std::holds_alternative<Step>(first)) {
      _resourceMonitor.decreaseMemoryUsage(sizeof(Entry));
      _queue.pop_front();
      return {std::get<Step>(first)};
    }
    auto& cursor = std::get<std::reference_wrapper<Cursor>>(first).get();
    if (not cursor.hasMore()) {
      _resourceMonitor.decreaseMemoryUsage(sizeof(Entry));
      _queue.pop_front();
      cursor.markForDeletion();
      return pop();
    }
    for (auto&& step : cursor.next()) {
      append(step);
    }
    return pop();
  }

  std::vector<Step*> getStepsWithoutFetchedVertex() {
    std::vector<Step*> steps{};
    for (auto& item : _queue) {
      if (std::holds_alternative<Step>(item)) {
        auto& step = std::get<Step>(item);
        if (!step.vertexFetched()) {
          steps.emplace_back(&step);
        }
      }
    }
    return steps;
  }

  void getStepsWithoutFetchedEdges(std::vector<Step*>& steps) {
    for (auto& item : _queue) {
      if (std::holds_alternative<Step>(item)) {
        auto step = std::get<Step>(item);
        if (!step.edgeFetched() && !step.isUnknown()) {
          steps.emplace_back(&step);
        }
      }
    }
  }
  template<class S, NeighbourCursor<S> C, typename Inspector>
  friend auto inspect(Inspector& f, BatchedLifoQueue<S, C>& x);

 private:
  using Entry = QueueEntry<Step, Cursor>;
  /// @brief queue datastore
  std::deque<Entry> _queue;

  /// @brief query context
  arangodb::ResourceMonitor& _resourceMonitor;
};
template<class StepType, NeighbourCursor<StepType> C, typename Inspector>
auto inspect(Inspector& f, BatchedLifoQueue<StepType, C>& x) {
  return f.object(x).fields(f.field("queue", x._queue));
}

}  // namespace graph
}  // namespace arangodb
