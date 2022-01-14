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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

#include <queue>
#include <vector>

namespace arangodb {
namespace graph {

template<class StepType>
class WeightedQueue {
 public:
  static constexpr bool RequiresWeight = true;
  using Step = StepType;
  // TODO: Add Sorting (Performance - will be implemented in the future -
  // cluster relevant)
  // -> loose ends to the end

  explicit WeightedQueue(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor{resourceMonitor} {}
  ~WeightedQueue() { this->clear(); }

  void clear() {
    if (!_queue.empty()) {
      _resourceMonitor.decreaseMemoryUsage(_queue.size() * sizeof(Step));
      _queue.clear();
    }
  }

  void append(Step step) {
    arangodb::ResourceUsageScope guard(_resourceMonitor, sizeof(Step));
    // if emplace() throws, no harm is done, and the memory usage increase
    // will be rolled back
    _queue.emplace_back(std::move(step));
    guard.steal();  // now we are responsible for tracking the memory
    // std::push_heap takes the last element in the queue, assumes that all
    // other elements are in heap structure, and moves the last element into
    // the correct position in the heap (incl. rebalancing of other elements)
    // The heap structure guarantees that the first element in the queue
    // is the "largest" element (in our case it is the smallest, as we inverted
    // the comperator)
    std::push_heap(_queue.begin(), _queue.end(), _cmpHeap);
  }

  bool hasProcessableElement() const {
    if (!isEmpty()) {
      // The heap structure guarantees that the first element in the queue
      // is the "largest" element (in our case it is the smallest, as we
      // inverted the comperator)
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
    // std::pop_heap will move the front element (the one we would like to
    // steal) to the back of the vector, keeping the tree intact otherwise. Now
    // we steal the last element.
    std::pop_heap(_queue.begin(), _queue.end(), _cmpHeap);
    Step first = std::move(_queue.back());
    LOG_TOPIC("9cd66", TRACE, Logger::GRAPHS)
        << "<WeightedQueue> Pop: " << first.toString();
    _resourceMonitor.decreaseMemoryUsage(sizeof(Step));
    _queue.pop_back();
    return first;
  }

 private:
  struct WeightedComparator {
    bool operator()(Step const& a, Step const& b) {
      if (a.getWeight() == b.getWeight()) {
        // Only false if A is not processable but B is.
        return !a.isProcessable() || b.isProcessable();
      }
      return a.getWeight() > b.getWeight();
    }
  };

  WeightedComparator _cmpHeap{};

  /// @brief queue datastore
  /// Note: Mutable is a required for hasProcessableElement right now which is
  /// const. We can easily make it non const here.
  mutable std::vector<Step> _queue;

  /// @brief query context
  arangodb::ResourceMonitor& _resourceMonitor;
};

}  // namespace graph
}  // namespace arangodb
