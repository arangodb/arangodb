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
#include "Logger/LogMacros.h"
#include "Graph/Queues/ExpansionMarker.h"

#include <queue>
#include <variant>

namespace arangodb {
namespace graph {

template<class StepType>
class BatchedLifoQueue {
 public:
  static constexpr bool RequiresWeight = false;
  using Step = StepType;
  // TODO: Add Sorting (Performance - will be implemented in the future -
  // cluster relevant)
  // -> loose ends to the end

  explicit BatchedLifoQueue(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor{resourceMonitor} {}
  ~BatchedLifoQueue() { this->clear(); }

  bool isBatched() { return true; }

  void clear() {
    if (!_queue.empty()) {
      // TODO
      for (auto const& item : _queue) {
        if (std::holds_alternative<Step>(item)) {
          _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
        } else {
          _resourceMonitor.decreaseMemoryUsage(sizeof(Expansion));
        }
      }
      _queue.clear();
    }
  }

  void append(QueueEntry<Step> step) {
    if (std::holds_alternative<Step>(step)) {
      arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Step));
      // if push_front() throws, no harm is done, and the memory usage increase
      // will be rolled back
      _queue.push_front(std::move(step));
      guard.steal();  // now we are responsible for tracking the memory
    } else {
      arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Expansion));
      // if push_front() throws, no harm is done, and the memory usage increase
      // will be rolled back
      _queue.push_front(std::move(step));
      guard.steal();  // now we are responsible for tracking the memory
    }
  }

  void setStartContent(std::vector<Step> startSteps) {
    arangodb::ResourceUsageScope guard(_resourceMonitor,
                                       sizeof(Step) * startSteps.size());
    TRI_ASSERT(_queue.empty());
    for (auto& s : startSteps) {
      // For LIFO just append to the back,
      // The handed in vector will then be processed from start to end.
      // And appending would queue BEFORE items in the queue.
      _queue.push_back({std::move(s)});
    }
    guard.steal();  // now we are responsible for tracking the memory
  }

  bool firstIsVertexFetched() const {
    if (!isEmpty()) {
      auto const& first = _queue.front();
      if (std::holds_alternative<Step>(first)) {
        return std::get<Step>(first).vertexFetched();
      } else {
        return false;  // next batch needs to be fetched
      }
    }
    return false;
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
  }

  QueueEntry<Step> pop() {
    TRI_ASSERT(!isEmpty());
    auto first = std::move(_queue.front());
    LOG_TOPIC("9cda4", TRACE, Logger::GRAPHS)
        // LOG_DEVEL << "<BatchedLifoQueue> Pop: "
        << (std::holds_alternative<Step>(first)
                ? std::get<Step>(first).toString()
                : "next batch");
    if (std::holds_alternative<Step>(first)) {
      _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
    } else {
      _resourceMonitor.decreaseMemoryUsage(sizeof(Expansion));
    }
    _queue.pop_front();
    return first;
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

 private:
  /// @brief queue datastore
  std::deque<QueueEntry<Step>> _queue;

  /// @brief query context
  arangodb::ResourceMonitor& _resourceMonitor;
};

}  // namespace graph
}  // namespace arangodb
