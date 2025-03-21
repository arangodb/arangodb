////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include "Containers/AtomicList.h"
#include "Containers/ResourceManager.h"

// In this file we implement a high performance append-only bounded
// list. The list is bounded in that one can specify a limit on the used
// memory and older entries are automatically freed. Appending is fast
// in nearly all cases (two atomic operations). If an older chunk has
// to be freed, deallocation of the old list entries must be performed.
// With the exception of this relatively rare operation, the class is
// lock-free. This is used to keep the most recent API calls and to be
// able to deliver them via some API.

namespace arangodb {

// The class BoundedList implements a high-performance bounded list using
// ResourceManager for the current AtomicList and implements rotation logic.
// One can only prepend items and this operation is normally very fast.
// Every once in a while a prepend operation can be slightly slower if a
// batch of old items has to be freed. One can give an upper limit for
// the memory usage in the "current" list (argument _memoryThreshold)
// and once this limit is reached, a new current list is started and
// the old one is moved to a ring buffer of historic lists. The length
// of this ring buffer can be configured with the _maxHistory argument.
// The total upper limit for the memory usage (which can occasionally
// overshoot a bit) is thus
//   _memoryThreshold * _maxHistory.
// The type T must be an object which has a move constructor and has
// a method called `memoryUsage` which estimates the memory usage
// (including all substructures) in bytes. It should always return a
// positive value, but this is intentionally not enforced.
template<typename T>
class BoundedList {
 private:
  static_assert(std::is_object_v<T>, "T must be an object type");
  static_assert(
      std::is_same_v<decltype(std::declval<T>().memoryUsage()), size_t>,
      "T must have a memoryUsage() method returning size_t");

  // ResourceManager for the current list
  ResourceManager<std::shared_ptr<AtomicList<T>>> _resourceManager;

  // Memory usage tracking
  std::atomic<std::size_t> _memoryUsage;

  char padding[64];  // Put subsequent entries on a different cache line
  std::atomic<bool> _isRotating{false};  // Flag to coordinate rotation
                                         //
  // Ring buffer for historic lists
  std::vector<std::shared_ptr<AtomicList<T>>> _history;
  size_t _ringBufferPos;
  std::vector<std::shared_ptr<AtomicList<T>>> _trash;

  // Mutex for protecting the ring buffer and trash
  mutable std::mutex _mutex;

  // Configuration
  const std::size_t _memoryThreshold;
  const std::size_t _maxHistory;

 public:
  // The actual memory usage is max_history * memory_threshold and some minor
  // overshooting is possible!
  BoundedList(std::size_t memoryThreshold, std::size_t maxHistory)
      : _resourceManager(std::make_unique<std::shared_ptr<AtomicList<T>>>(
            std::make_shared<AtomicList<T>>())),
        _memoryUsage(0),
        _history(maxHistory),
        _ringBufferPos(0),
        _memoryThreshold(memoryThreshold),
        _maxHistory(maxHistory) {
    // The constructor exception could be more specific:
    if (memoryThreshold == 0 || maxHistory < 2) {
      throw std::invalid_argument(
          "BoundedList: memoryThreshold must be > 0 and maxHistory must be >= "
          "2");
    }
  }

  void prepend(T&& value) {
    // Can throw in out of memory situations!
    auto mem_usage = value.memoryUsage();

    // Use ResourceManager to access the current list (counts as a read access)
    _resourceManager.read(
        [&value](std::shared_ptr<AtomicList<T>> const& current) {
          current->prepend(std::move(value));
        });

    // Update memory usage
    size_t newUsage =
        _memoryUsage.fetch_add(mem_usage, std::memory_order_relaxed) +
        mem_usage;

    // Check if we need to rotate lists
    if (newUsage >= _memoryThreshold) {
      tryRotateLists();
    }
  }

  // Try to rotate lists without blocking.
  void tryRotateLists() {
    // For a specific value of _current, we want that only one thread actually
    // does the rotation. So, when the threshold is reached, we race on the
    // _isRotating flag, which is only reset by the winner, once _current is
    // changed.
    bool expected = false;
    if (!_isRotating.compare_exchange_strong(expected, true,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
      // Another thread is already handling rotation
      return;
    }

    // Reset memory usage counter, so that not more threads get held up:
    _memoryUsage.store(0, std::memory_order_relaxed);

    // Create a new empty list
    auto newList = std::make_shared<AtomicList<T>>();

    // Update the current list using ResourceManager
    // This returns the old list and its retirement epoch
    auto [oldList, epoch] = _resourceManager.update(
        std::make_unique<std::shared_ptr<AtomicList<T>>>(newList));

    // Wait for all readers to finish with the old list
    _resourceManager.wait_reclaim(epoch);

    // Update the ring buffer under mutex protection
    {
      std::lock_guard<std::mutex> guard(_mutex);

      // Move the old list into the ring buffer
      auto toDelete = std::move(_history[_ringBufferPos]);
      _history[_ringBufferPos] = std::move(*oldList);
      _ringBufferPos = (_ringBufferPos + 1) % _maxHistory;

      // Schedule the old list for deletion
      if (toDelete != nullptr) {
        _trash.emplace_back(std::move(toDelete));
      }
    }

    // Release the rotation lock
    _isRotating.store(false, std::memory_order_release);
  }

  // The forItems method provides a convenient way to iterate over all items
  // in the list from newest to oldest, executing a callback function for each
  // item. Note that it internally takes a snapshot of the current list and of
  // all historic lists, so it is safe to call this method from multiple threads
  // concurrently.
  template<typename F>
  requires std::is_invocable_v<F, T const&>
  void forItems(F&& callback) {
    // Get snapshots under lock
    std::vector<std::shared_ptr<AtomicList<T>>> snapshots;
    {
      std::lock_guard<std::mutex> guard(_mutex);
      snapshots.reserve(_history.size() + 1);

      // Get current list snapshot
      _resourceManager.read(
          [&snapshots](std::shared_ptr<AtomicList<T>> const& current) {
            snapshots.push_back(current);
          });

      // Get historic lists
      for (size_t i = 0; i < _maxHistory; ++i) {
        size_t pos = (_ringBufferPos + _maxHistory - 1 - i) % _maxHistory;
        if (_history[pos] != nullptr) {
          snapshots.push_back(_history[pos]);
        }
      }
    }

    // Process items from newest to oldest
    for (const auto& list : snapshots) {
      auto* node = list->getSnapshot();
      while (node != nullptr) {
        callback(node->_data);
        node = node->next();
      }
    }
  }

  size_t clearTrash() {
    // This method is called by a cleanup thread to free old batches.
    // Returns the number of batches that were freed.
    std::lock_guard<std::mutex> guard(_mutex);
    size_t freedBatches = _trash.size();
    _trash.clear();
    return freedBatches;
  }
};

}  // namespace arangodb
