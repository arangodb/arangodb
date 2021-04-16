////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_QUEUE_H
#define ARANGOD_GRAPH_QUEUE_H 1

#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

#include <queue>

namespace arangodb {
namespace graph {

template <class StepType>
class FifoQueue {
 public:
  using Step = StepType;
  // TODO: Add Sorting (Performance - will be implemented in the future - cluster relevant)
  // -> loose ends to the end

  explicit FifoQueue(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor{resourceMonitor} {}
  ~FifoQueue() { this->clear(); }

  void clear() {
    if (_queue.size() > 0) {
      _resourceMonitor.decreaseMemoryUsage(_queue.size() * sizeof(Step));
      _queue.clear();
    }
  }

  void append(Step step) {
    _resourceMonitor.increaseMemoryUsage(sizeof(Step));
    _queue.push_back(std::move(step));
  }

  bool hasProcessableElement() const {
    if (!isEmpty()) {
      auto const& first = _queue.front();
      return first.isProcessable();
    }

    return false;
  }

  size_t size() const { return _queue.size(); }

  bool isEmpty() const { return _queue.empty(); }

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

  Step pop() {
    TRI_ASSERT(!isEmpty());
    Step first = std::move(_queue.front());
    _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
    LOG_TOPIC("9cd65", TRACE, Logger::GRAPHS) << "<FifoQueue> Pop: " << first.toString();
    _queue.pop_front();
    return first;
  }

 private:
  /// @brief queue datastore
  std::deque<Step> _queue;

  /// @brief query context
  arangodb::ResourceMonitor& _resourceMonitor;
};

}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_QUEUE_H
