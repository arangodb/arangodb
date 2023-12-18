////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>

#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Futures/Exceptions.h"
#include "Futures/Future.h"
#include "Scheduler/Scheduler.h"

namespace arangodb::futures {

template<class Scheduler>
struct FutureSharedLock {
 private:
  struct Node;
  struct SharedState;

 public:
  struct LockGuard {
    LockGuard() = default;

    LockGuard(LockGuard const&) = delete;
    LockGuard& operator=(LockGuard const&) = delete;

    LockGuard(LockGuard&& r) noexcept : _lock(r._lock) { r._lock = nullptr; }

    LockGuard& operator=(LockGuard&& r) noexcept {
      if (&r != this) {
        _lock = r._lock;
        r._lock = nullptr;
      }
      return *this;
    }

    bool isLocked() const noexcept { return _lock != nullptr; }

    void unlock() noexcept {
      TRI_ASSERT(_lock != nullptr);
      _lock->unlock();
      _lock = nullptr;
    }

    ~LockGuard() {
      if (_lock) {
        _lock->unlock();
      }
    }

   private:
    friend struct FutureSharedLock;
    explicit LockGuard(SharedState* lock) : _lock(lock) {}
    SharedState* _lock = nullptr;
  };

  using FutureType = Future<LockGuard>;

  explicit FutureSharedLock(Scheduler& scheduler)
      : _sharedState(std::make_shared<SharedState>(scheduler)) {}

  FutureType asyncLockShared() {
    return _sharedState->asyncLockShared([](auto) {});
  }

  FutureType asyncLockExclusive() {
    return _sharedState->asyncLockExclusive([](auto) {});
  }

  FutureType asyncTryLockSharedFor(std::chrono::milliseconds timeout) {
    return _sharedState->asyncLockShared([this, timeout](auto node) {
      _sharedState->scheduleTimeout(node, timeout);
    });
  }

  FutureType asyncTryLockExclusiveFor(std::chrono::milliseconds timeout) {
    return _sharedState->asyncLockExclusive([this, timeout](auto node) {
      _sharedState->scheduleTimeout(node, timeout);
    });
  }

  void unlock() { _sharedState->unlock(); }

 private:
  struct Node {
    explicit Node(bool exclusive) : exclusive(exclusive) {}

    Promise<LockGuard> promise;
    bool exclusive;
  };

  struct SharedState : std::enable_shared_from_this<SharedState> {
    explicit SharedState(Scheduler& scheduler) : _scheduler(scheduler) {}

    template<class Func>
    FutureType asyncLockExclusive(Func blockedFunc) {
      std::lock_guard lock(_mutex);
      if (_lockCount == 0) {
        TRI_ASSERT(_queue.empty());
        ++_lockCount;
        _exclusive = true;
        return {LockGuard(this)};
      }

      auto it = insertNode(true);
      blockedFunc(it);
      return (*it)->promise.getFuture();
    }

    template<class Func>
    FutureType asyncLockShared(Func blockedFunc) {
      std::lock_guard lock(_mutex);
      if (_lockCount == 0 || (!_exclusive && _queue.empty())) {
        ++_lockCount;
        _exclusive = false;
        return {LockGuard(this)};
      }

      auto it = insertNode(false);
      blockedFunc(it);
      return (*it)->promise.getFuture();
    }

    void unlock() {
      std::lock_guard lock(_mutex);
      TRI_ASSERT(_lockCount > 0);
      if (--_lockCount == 0 && !_queue.empty()) {
        // we were the last lock holder -> schedule the next node
        auto& node = _queue.back();
        _lockCount = 1;
        _exclusive = node->exclusive;
        scheduleNode(*node);
        _queue.pop_back();

        // if we are in shared mode, we can schedule all following shared
        // nodes
        if (!_exclusive) {
          while (!_queue.empty() && !_queue.back()->exclusive) {
            ++_lockCount;
            scheduleNode(*_queue.back());
            _queue.pop_back();
          }
        }
      }
    }

    auto insertNode(bool exclusive) {
      _queue.emplace_front(std::make_shared<Node>(exclusive));
      return _queue.begin();
    }

    void removeNode(
        typename std::list<std::shared_ptr<Node>>::iterator queueIterator) {
      TRI_ASSERT(!_queue.empty());
      TRI_ASSERT(_lockCount > 0);
      if (std::addressof(_queue.back()) == std::addressof(*queueIterator)) {
        // we are the first entry in the list -> remove ourselves and check if
        // we can schedule the next node(s)
        _queue.pop_back();
        if (!_exclusive) {
          while (!_queue.empty() && !_queue.back()->exclusive) {
            ++_lockCount;
            scheduleNode(*_queue.back());
            _queue.pop_back();
          }
        }
      } else {
        // we are somewhere in the middle -> just remove ourselves
        _queue.erase(queueIterator);
      }
    }

    void scheduleNode(Node& node) {
      // TODO in theory `this` can die before execute callback
      //  so probably better to use `shared_from_this()`, but it's slower
      _scheduler.queue([promise = std::move(node.promise), this]() mutable {
        promise.setValue(LockGuard(this));
      });
    }

    void scheduleTimeout(
        typename std::list<std::shared_ptr<Node>>::iterator queueIterator,
        std::chrono::milliseconds timeout) {
      _scheduler.queueDelayed(
          [self = this->weak_from_this(),
           node = std::weak_ptr<Node>(*queueIterator),
           queueIterator]() mutable {
            if (auto me = self.lock(); me) {
              if (auto nodePtr = node.lock(); nodePtr) {
                std::lock_guard lock(me->_mutex);
                if (nodePtr.use_count() != 1) {
                  // if use_count == 1, this means that the promise has already
                  // been scheduled and the node has been removed from the queue
                  // otherwise the iterator must still be valid!
                  me->removeNode(queueIterator);
                }
              }
            }
          },
          timeout);
    }

    Scheduler& _scheduler;
    std::mutex _mutex;
    // the list is ordered back to front, i.e., new items are added at the
    // front; the last item (back) is the next to be scheduled
    std::list<std::shared_ptr<Node>> _queue;
    bool _exclusive{false};
    std::uint32_t _lockCount{0};
  };
  std::shared_ptr<SharedState> _sharedState;
};

}  // namespace arangodb::futures
