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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

#include <queue>

namespace arangodb {
namespace graph {

template<class StepType>
class FifoQueue {
 public:
  static constexpr bool RequiresWeight = false;
  using Step = StepType;
  // TODO: Add Sorting (Performance - will be implemented in the future -
  // cluster relevant)
  // -> loose ends to the front

  explicit FifoQueue(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor{resourceMonitor} {}
  ~FifoQueue() { this->clear(); }

  void clear() {
    if (!_queue.empty()) {
      _resourceMonitor.decreaseMemoryUsage(_queue.size() * sizeof(Step));
      _queue.clear();
    }
  }

  void append(Step step) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Step));
    // if push_back() throws, no harm is done, and the memory usage increase
    // will be rolled back
    _queue.push_back(std::move(step));
    guard.steal();  // now we are responsible for tracking the memory
  }

  void setStartContent(std::vector<Step> startSteps) {
    arangodb::ResourceUsageScope guard(_resourceMonitor,
                                       sizeof(Step) * startSteps.size());
    TRI_ASSERT(_queue.empty());
    for (auto& s : startSteps) {
      // For FIFO just append to the back,
      // The handed in vector will then be processed start from to end.
      _queue.push_back(std::move(s));
    }
    guard.steal();  // now we are responsible for tracking the memory
  }

  bool firstIsVertexFetched() const {
    if (not isEmpty()) {
      auto const& first = _queue.front();
      return first.vertexFetched();
    }
    return false;
  }

  // todo: rename to firstElementIsProcessable
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

  std::vector<Step*> getStepsWithoutFetchedVertex() {
    std::vector<Step*> steps;
    for (auto& step : _queue) {
      if (not step.vertexFetched()) {
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

  Step const& peek() const {
    // Currently only implemented and used in WeightedQueue
    TRI_ASSERT(false);
    auto const& first = _queue.front();
    return first;
  }

  Step pop() {
    TRI_ASSERT(!isEmpty());
    Step first = std::move(_queue.front());
    LOG_TOPIC("9cd65", TRACE, Logger::GRAPHS)
        << "<FifoQueue> Pop: " << first.toString();
    _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
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
