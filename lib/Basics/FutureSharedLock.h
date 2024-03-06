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
  struct SharedUnlocker {
    void operator()(SharedState* lock) { lock->unlockShared(); }
  };

  struct ExclusiveUnlocker {
    void operator()(SharedState* lock) { lock->unlockExclusive(); }
  };

  template<typename Unlocker>
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

    void release() noexcept { _lock = nullptr; }

    void unlock() noexcept {
      TRI_ASSERT(_lock != nullptr);
      Unlocker{}(_lock);
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

  using FutureSharedType = Future<LockGuard<SharedUnlocker>>;
  using FutureExclusiveType = Future<LockGuard<ExclusiveUnlocker>>;

  explicit FutureSharedLock(Scheduler& scheduler)
      : _sharedState(std::make_shared<SharedState>(scheduler)) {}

  FutureSharedType asyncLockShared() {
    return _sharedState->asyncLockShared([](auto) {});
  }

  FutureExclusiveType asyncLockExclusive() {
    return _sharedState->asyncLockExclusive([](auto) {});
  }

  FutureSharedType asyncTryLockSharedFor(std::chrono::milliseconds timeout) {
    return _sharedState->asyncLockShared([this, timeout](auto node) {
      _sharedState->scheduleTimeout(node, timeout);
    });
  }

  FutureExclusiveType asyncTryLockExclusiveFor(
      std::chrono::milliseconds timeout) {
    return _sharedState->asyncLockExclusive([this, timeout](auto node) {
      _sharedState->scheduleTimeout(node, timeout);
    });
  }

  void unlockShared() { unlock(); }
  void unlockExclusive() { unlock(); }
  void unlock() { _sharedState->unlock(); }

 private:
  struct NotifyGotLock {};

  struct Node {
    virtual ~Node() = default;
    virtual void resolve(SharedState* lock) = 0;
    virtual void resolveWithError(::ErrorCode) = 0;
    typename Scheduler::WorkHandle _workItem;
    bool exclusive = false;
  };

  template<typename Unlocker>
  struct TypedNode : Node {
    TypedNode() {
      this->exclusive = std::is_same_v<Unlocker, ExclusiveUnlocker>;
    }

    void resolve(SharedState* lock) final {
      promise.setValue(LockGuard<Unlocker>{lock});
    }

    void resolveWithError(::ErrorCode code) final {
      promise.setException(::arangodb::basics::Exception(code, ADB_HERE));
    }

    Promise<LockGuard<Unlocker>> promise;
  };

  struct SharedState : std::enable_shared_from_this<SharedState> {
    explicit SharedState(Scheduler& scheduler) : _scheduler(scheduler) {}

    template<class Func>
    FutureExclusiveType asyncLockExclusive(Func blockedFunc) {
      std::lock_guard lock(_mutex);
      if (_lockCount == 0) {
        TRI_ASSERT(_queue.empty());
        ++_lockCount;
        _exclusive = true;
        return {LockGuard<ExclusiveUnlocker>(this)};
      }

      auto [node, it] = insertNode<ExclusiveUnlocker>();
      blockedFunc(it);
      return node->promise.getFuture();
    }

    template<class Func>
    FutureSharedType asyncLockShared(Func blockedFunc) {
      std::lock_guard lock(_mutex);
      if (_lockCount == 0 || (!_exclusive && _queue.empty())) {
        ++_lockCount;
        _exclusive = false;
        return {LockGuard<SharedUnlocker>(this)};
      }

      auto [node, it] = insertNode<SharedUnlocker>();
      blockedFunc(it);
      return node->promise.getFuture();
    }

    void unlock() {
      std::lock_guard lock(_mutex);
      TRI_ASSERT(_lockCount > 0);
      if (--_lockCount == 0 && !_queue.empty()) {
        // we were the last lock holder -> schedule the next node
        auto& node = _queue.back();
        _lockCount = 1;
        _exclusive = node->exclusive;
        scheduleNode(std::move(node));
        _queue.pop_back();

        // if we are in shared mode, we can schedule all following shared
        // nodes
        if (!_exclusive) {
          while (!_queue.empty() && !_queue.back()->exclusive) {
            ++_lockCount;
            scheduleNode(std::move(_queue.back()));
            _queue.pop_back();
          }
        }
      }
    }

    template<typename Unlocker>
    auto insertNode() {
      auto node = std::make_shared<TypedNode<Unlocker>>();
      _queue.emplace_front(node);
      return std::make_pair(node, _queue.begin());
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
            scheduleNode(std::move(_queue.back()));
            _queue.pop_back();
          }
        }
      } else {
        // we are somewhere in the middle -> just remove ourselves
        _queue.erase(queueIterator);
      }
    }

    void scheduleNode(std::shared_ptr<Node>&& node) {
      // TODO in theory `this` can die before execute callback
      //  so probably better to use `shared_from_this()`, but it's slower
      _scheduler.queue(
          [node = std::move(node), this]() mutable { node->resolve(this); });
    }

    void scheduleTimeout(
        typename std::list<std::shared_ptr<Node>>::iterator queueIterator,
        std::chrono::milliseconds timeout) {
      (*queueIterator)->_workItem = _scheduler.queueDelayed(
          [self = this->weak_from_this(),
           node = std::weak_ptr<Node>(*queueIterator),
           queueIterator](bool cancelled) mutable {
            if (auto me = self.lock(); me) {
              if (auto nodePtr = node.lock(); nodePtr) {
                std::unique_lock lock(me->_mutex);
                if (nodePtr.use_count() != 1) {
                  // if use_count == 1, this means that the promise has already
                  // been scheduled and the node has been removed from the queue
                  // otherwise the iterator must still be valid!
                  me->removeNode(queueIterator);
                  lock.unlock();
                  nodePtr->resolveWithError(cancelled
                                                ? TRI_ERROR_REQUEST_CANCELED
                                                : TRI_ERROR_LOCK_TIMEOUT);
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
