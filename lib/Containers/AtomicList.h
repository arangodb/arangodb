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
#include <vector>
#include <type_traits>

namespace arangodb {

// In this file we implement a high performance append-only bounded
// list. The list is bounded in that one can specify a limit on the used
// memory and older entries are automatically freed. Appending is fast
// in nearly all cases (two atomic operations). If an older chunk has
// to be freed, deallocation of the old list entries must be performed.
// With the exception of this relatively rare operation, the class is
// lock-free. This is used to keep the most recent API calls and to be
// able to deliver them via some API.

// The class AtomicList is a simple lock-free implementation of a singly
// linked list. One can only prepend new items and get a snapshot in form
// of a raw pointer to a Node. With this one can traverse the list but
// one must not free it.
template<typename T>
class AtomicList {
 public:
  // Note that Nodes use bare pointers, since AtomicList guards the allocation
  // of the whole list.
  struct Node {
    T _data;
    Node* _next;

    Node(const T& value) : _data(value), _next(nullptr) {}
    Node* next() { return _next; }
  };

 private:
  std::atomic<Node*> _head;

 public:
  AtomicList() : _head(nullptr) {}

  // It is *not* safe to destruct the AtomicList whilst other threads are
  // still prepending! The user of the class has to ensure that this is
  // done properly!
  ~AtomicList() {
    Node* n = _head.load();
    while (n != nullptr) {
      Node* next = n->next();
      delete n;
      n = next;
    }
    _head.store(nullptr);
  }

  void prepend(T&& value) noexcept {
    Node* new_node;
    try {
      new_node = new Node(std::move(value));
    } catch (...) {
      // We intentionally ignore out-of-memory errors and simply drop the
      // item to be noexcept.
      return;
    }
    Node* old_head;

    do {
      old_head = _head.load();
      new_node->_next = old_head;
      // The following compare_exchange_weak synchronizes with the
      // load in getSnapShot, therefore we use release semantics here.
      // In the case that the compare_exchange_weak fails the reload
      // uses acquire semantics to synchronize with another
      // compare_exchange_weak which might have released a pointer.
    } while (!_head.compare_exchange_weak(old_head, new_node,
                                          std::memory_order_release,
                                          std::memory_order_acquire));
  }

  // Returns a snapshot of the list at the moment of calling.
  // Result must not be freed externally! It is the AtomicList which
  // guards the allocation of all its Nodes!
  Node* getSnapshot() const noexcept {
    // This load synchronizes with the compare_exchange_weak in `prepend`
    // to ensure that any thread which gets the snapshot can actually
    // see modifications in the object the Node pointer is pointing to.
    return _head.load(std::memory_order_acquire);
  }
};

// The class BoundedList implements a high-performance nearly lock-free
// bounded list. It is bounded by the memory usage. One can only prepend
// items and this operation is normally very fast. Every once in a while
// a prepend operation can be slightly slower if a batch of old items
// has to be freed. One can give an upper limit for the memory usage in
// the "current" list (argument _memoryThreshold) and once this limit is
// reached, a new current list is started and the old one is moved to a
// ring buffer of historic lists. The length of this ring buffer can be
// configured with the _maxHistory argument. The total upper limit for the
// memory usage (which can occasionally overshoot a bit) is thus
//   _memoryThreshold * _maxHistory.
// The forItems method provides a convenient way
// to iterate over all items in the list from newest to oldest, executing a
// callback function for each item. Note that it internally takes a snapshot
// of the current list and of all historic lists, so it is safe to call this
// method from multiple threads concurrently.
// The type T must be an object which has a move constructor and has
// a method called `memoryUsage` which estimates the memory usage
// (including all substructures) in bytes. It should always return a
// positive value, but this is intentionally not enforced.
template<typename T>
class BoundedList {
 public:  // just for debugging, remove this later!
  static_assert(std::is_object_v<T>, "T must be an object type");
  static_assert(
      std::is_same_v<decltype(std::declval<T>().memoryUsage()), size_t>,
      "T must have a memoryUsage() method returning size_t");
  std::atomic<std::shared_ptr<AtomicList<T>>> _current;
  std::atomic<std::size_t> _memoryUsage;
  char padding[64];  // Put subsequent entries on a different cache line
  std::atomic<bool> _isRotating{false};  // Flag to coordinate rotation
  std::vector<std::shared_ptr<AtomicList<T>>> _history;
  std::vector<std::shared_ptr<AtomicList<T>>> _trash;
  size_t _ringBufferPos;
  mutable std::mutex
      _mutex;  // protecting write access to _current and _history
  const std::size_t _memoryThreshold;
  const std::size_t _maxHistory;

 public:
  // The actual memory usage is max_history * memory_threshold and some minor
  // overshooting is possible!
  BoundedList(std::size_t memoryThreshold, std::size_t maxHistory)
      : _current(std::make_shared<AtomicList<T>>()),
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
    // This load may synchronize with a store which has inserted a new
    // list in this very method below, therefore acquire semantics.
    std::shared_ptr<AtomicList<T>> current =
        _current.load(std::memory_order_acquire);
    current->prepend(std::move(value));

    // We assume throughout that `size_t` is at least 64bits and over- or
    // underflow is not a problem.
    size_t newUsage =
        _memoryUsage.fetch_add(mem_usage, std::memory_order_relaxed) +
        mem_usage;

    if (newUsage >= _memoryThreshold) {
      tryRotateLists(current);
    }
  }

  // Try to rotate lists without blocking.
  void tryRotateLists(std::shared_ptr<AtomicList<T>>& expectedCurrent) {
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

    // Verify that the current list is still the one we expect
    // This prevents a race condition where another thread might have
    // already rotated the current list and we still have the old one.
    // We might have been delayed for whatever reason and now only want
    // to rotate if our current list is still the one we expect. This
    // ensures that for a given instance of the current list, only one
    // thread will do the rotation.
    if (_current.load(std::memory_order_relaxed) != expectedCurrent) {
      // The current list has already been rotated by another thread
      _isRotating.store(false, std::memory_order_release);
      return;
    }

    // First reset the memory usage counter to prevent other threads from
    // triggering additional rotations
    _memoryUsage.store(0, std::memory_order_relaxed);

    // Create a new empty list that will become the current list
    auto newList = std::make_shared<AtomicList<T>>();

    // Then replace the current list
    _current.store(newList, std::memory_order_release);
    // From now on, new threads entering `prepend` will see the new list
    // and append there. Note that it is possible that some threads have
    // still prepended to the old current list, but have already accounted
    // for the memory usage in _memoryUsage, which is already counted towards
    // the new list. We tolerate this because it is harmless and we want to
    // reduce contention on _isRotating.

    // Now update the ring buffer:
    {
      // This mutex is needed to protect the ring buffer from being read
      // by some other thread in the forItems method. Writing threads cannot
      // reach this place because of the _isRotating flag.
      std::lock_guard<std::mutex> guard(_mutex);

      // Move the old current list into the ring buffer
      auto toDelete = std::move(_history[_ringBufferPos]);
      _history[_ringBufferPos] = std::move(expectedCurrent);
      _ringBufferPos = (_ringBufferPos + 1) % _maxHistory;

      // Schedule the old list for deletion
      if (toDelete != nullptr) {
        _trash.emplace_back(std::move(toDelete));
      }
    }

    // Release the rotation lock
    _isRotating.store(false, std::memory_order_release);
  }

  template<typename F>
  requires std::is_invocable_v<F, T const&>
  void forItems(F&& callback) const {
    // Get snapshots under lock
    std::vector<std::shared_ptr<AtomicList<T>>> snapshots;
    {
      std::lock_guard<std::mutex> guard(_mutex);
      snapshots.reserve(_history.size() + 1);
      snapshots.push_back(_current.load(std::memory_order_acquire));

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
