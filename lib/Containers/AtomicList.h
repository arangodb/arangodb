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
    // Note that this
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
// It is possible to take a snapshot of the current list and of all
// historic lists. These snapshots may be traversed, but it is not allowed
// to delete the Node pointers.
// The type T must be an object which has a move constructor and has
// a method called `memoryUsage` which estimates the memory usage
// (including all substructures) in bytes. It should always return a
// positive value, but this is intentionally not enforced.
template<typename T>
class BoundedList {
  static_assert(std::is_object_v<T>, "T must be an object type");
  static_assert(
      std::is_same_v<decltype(std::declval<T>().memoryUsage()), size_t>,
      "T must have a memoryUsage() method returning size_t");
  std::atomic<std::shared_ptr<AtomicList<T>>> _current;
  std::atomic<std::size_t> _memoryUsage;
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
    size_t prev_usage =
        _memoryUsage.fetch_add(mem_usage, std::memory_order_relaxed);
    size_t newUsage = prev_usage + mem_usage;
    if (prev_usage < _memoryThreshold && newUsage >= _memoryThreshold) {
      // We are the chosen one which is supposed to rotate _current into
      // _history!
      std::lock_guard<std::mutex> guard(_mutex);
      if (_current.load(std::memory_order_relaxed) == current) {
        // If somebody else came before us, we do not bother to rotate!
        _current.store(std::make_shared<AtomicList<T>>(),
                       std::memory_order_seq_cst);
        // We use a sequentially consistent store here because it happens
        // only rarely. This synchronizes with the load above.

        // We first store a new empty list in _current, so that any other thread
        // will prepend to that one as soon as possible.
        _memoryUsage.fetch_sub(newUsage);
        auto toDelete = std::move(_history[_ringBufferPos]);
        _history[_ringBufferPos] = std::move(current);
        _ringBufferPos = (_ringBufferPos + 1) % _maxHistory;
        if (toDelete != nullptr) {
          _trash.emplace_back(std::move(toDelete));
        }
        // If somebody prepended something to his copy of _current before
        // we are completely done here, it will be prepended to the list,
        // which is already or will be soon in _history.
      }
      // Some comments about correctness are in order here: Only one thread
      // will be in this critical section here at a time. Here, we do the
      // following operations:
      //  - copy _current to the ring buffer
      //  - free a potentially overwritten entry in the ring buffer
      //  - set _current to a new empty AtomicList
      // It can happen that another thread copies the shared_ptr in _current
      // to prepend to the AtomicList, whilst this thread here copies the
      // shared_ptr to the ring buffer. This is OK.
      // That is, it is possible that another thread prepends to an AtomicList
      // which is already in the ring buffer. We need to prove that it cannot
      // happen that this other thread prepends to the AtomicList and this
      // thread here destroys it at the same time. But this is guaranteed since
      // we make a copy of the shared_ptr for each `prepend` call. Everybody
      // will eventually destroy its shared_ptr and the atomic logic in
      // shared_ptr will see to it that only one thread frees it. In most
      // cases this will be the one in the critical section, but even if the
      // critical section frees its shared_ptr first, then it won't destruct
      // the AtomicList and the other thread which prepends to it will do it.
      // This is very unlikely to happen, since `prepend` will probably not
      // lose a lot of time between copying _current and then prepending
      // to the AtomicList. Usually, the critical section here will delete
      // a different AtomicList. But this proof shows that even in the unlikely
      // event that both work on the same AtomicList, nothing is leaked or
      // crashes.
    }
  }

  // Get current list snapshot
  std::shared_ptr<AtomicList<T>> getCurrentSnapshot() const {
    // Use the `getSnapshot` method of the AtomicList and then follow
    // the Nodes, but do not delete any Node! The allocation is safe as
    // long as you keep the shared_ptr. Note however, that it is possible
    // that Nodes are prepended to the list whilst you hold it!
    // This can throw in out of memory situations.
    return _current.load(std::memory_order_acquire);
    // This synchronizes with a store when a new list is stored in the
    // `prepend` method above.
  }

  // Get historical snapshots
  std::vector<std::shared_ptr<AtomicList<T>>> getHistoricalSnapshot() const {
    // Use the `getSnapshot` method of the AtomicLists and then follow
    // the Nodes, but do not delete any Node! The allocation is safe as long
    // as you keep the shared_ptrs.
    std::lock_guard<std::mutex> guard(_mutex);
    std::vector<std::shared_ptr<AtomicList<T>>> snapshots;
    snapshots.reserve(_history.size() + 1);
    snapshots.push_back(_current.load(std::memory_order_acquire));
    // This synchronizes with the storing of a new empty list in the
    // `prepend` method.
    for (size_t i = 0; i < _maxHistory; ++i) {
      size_t pos = (_ringBufferPos + _maxHistory - 1 - i) % _maxHistory;
      if (_history[pos] != nullptr) {
        snapshots.push_back(_history[pos]);
      }
    }
    return snapshots;
  }

  std::vector<std::shared_ptr<AtomicList<T>>> getTrash() {
    // This method is to extract the trashed batches for external removal.
    std::lock_guard<std::mutex> guard(_mutex);
    std::vector<std::shared_ptr<AtomicList<T>>> trashHeap;
    trashHeap.reserve(_trash.size());
    // This synchronizes with the storing of a new empty list in the
    // `prepend` method.
    for (size_t i = 0; i < _trash.size(); ++i) {
      trashHeap.push_back(_trash[i]);
    }
    _trash.clear();
    return trashHeap;
  }
};

}  // namespace arangodb
