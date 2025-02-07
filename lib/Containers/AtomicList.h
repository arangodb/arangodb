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

// In this file we implement a high performance append-only bounded list.
// The list is bounded in that one can specify a limit on the used memory and
// older entries are automatically freed. Appending is fast in nearly all cases
// (two atomic operations). If an older chunk has to be freed,
// deallocation of the old list entries must be performed. With the
// exception of this relatively rare operation, the class is lock-free.
// This is used to keep the most recent API calls and to be able to deliver them
// via some API.

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

  void prepend(T&& value) {
    Node* new_node = new Node(std::move(value));
    Node* old_head;

    do {
      old_head = _head.load();
      new_node->_next = old_head;
    } while (!_head.compare_exchange_weak(old_head, new_node));
  }

  // Returns a snapshot of the list at the moment of calling.
  // Result must not be freed externally! It is the AtomicList which
  // guards the allocation of all its Nodes!
  Node* getSnapshot() const { return _head.load(std::memory_order_acquire); }
};

template<typename T>
class BoundedList {
  std::atomic<std::shared_ptr<AtomicList<T>>> _current;
  std::atomic<std::size_t> _memoryUsage;
  std::vector<std::shared_ptr<AtomicList<T>>> _history;
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
        _maxHistory(maxHistory) {}

  void prepend(T&& value) {
    auto mem_usage = value.memoryUsage();
    std::shared_ptr<AtomicList<T>> current =
        _current.load(std::memory_order_acquire);
    current->prepend(std::move(value));
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
        // We first store a new empty list in _current, so that any other thread
        // will prepend to that one as soon as possible.
        _memoryUsage.fetch_sub(newUsage);
        _history[_ringBufferPos] = std::move(current);
        _ringBufferPos = (_ringBufferPos + 1) % _maxHistory;
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
      // to prepend to the AtomicList, whilst this hread here copies the
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
    return _current.load(std::memory_order_acquire);
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
    for (size_t i = 0; i < _maxHistory; ++i) {
      size_t pos = (_ringBufferPos + _maxHistory - 1 - i) % _maxHistory;
      if (_history[pos] != nullptr) {
        snapshots.push_back(_history[pos]);
      }
    }
    return snapshots;
  }
};

}  // namespace arangodb
