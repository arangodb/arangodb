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
#include <cstdint>
#include <mutex>

#include "Assertions/Assert.h"
#include "Futures/Exceptions.h"
#include "Futures/Future.h"
#include "Scheduler/Scheduler.h"

namespace arangodb::futures {

struct SharedLock {
  virtual ~SharedLock() = default;
  virtual void unlock() noexcept = 0;
};

struct SharedLockGuard {
  SharedLockGuard() = default;

  SharedLockGuard(SharedLockGuard const&) = delete;
  SharedLockGuard& operator=(SharedLockGuard const&) = delete;

  SharedLockGuard(SharedLockGuard&& r) noexcept : _lock(r._lock) {
    r._lock = nullptr;
  }

  SharedLockGuard& operator=(SharedLockGuard&& r) noexcept {
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

  ~SharedLockGuard() {
    if (_lock) {
      _lock->unlock();
    }
  }

 private:
  template<class>
  friend struct FutureSharedLock;

  explicit SharedLockGuard(SharedLock* lock) : _lock(lock) {}
  SharedLock* _lock = nullptr;
};

template<class Scheduler>
struct FutureSharedLock {
 private:
  struct Node;

 public:
  using FutureType = Future<SharedLockGuard>;

  explicit FutureSharedLock(Scheduler& scheduler) : _scheduler(scheduler) {}

  FutureType asyncLockShared() {
    auto* node = new Node(*this, SharedTag{});
    auto* pred = _tail.exchange(node);
    if (pred == nullptr) {
      node->state.store(State::kSharedActiveLeader);
      return {SharedLockGuard(node)};
    }

    auto predState = pred->state.load();
    if (predState == State::kExclusiveLeader ||
        predState == State::kExclusiveBlocked) {
      // current owner is exclusive, so we are blocked
      node->state.store(State::kSharedBlocked);
      pred->next.store(node);
      return node->promise.getFuture();
    }

    TRI_ASSERT(predState == State::kSharedInitializing ||
               predState == State::kSharedBlocked ||
               predState == State::kSharedActiveLeader ||
               predState == State::kSharedActiveFollower ||
               predState == State::kSharedFinished);
    // we already store our node in the next pointer so that the current leader
    // can find us, even though we have not yet established our state.
    // this is necessary to prevent a situation where we observe our predecessor
    // as blocked, but before we can store our node in the next pointer, the
    // predecessor gets activated, but since the next pointer is not set in
    // cannot follow up the chain. In this case, we would end up blocked even
    // though we could be active. The new leader can later activate us, but we
    // would wait unnecessarily.
    pred->next.store(node);

    // we have to reload the state _after_ we stored our next pointer
    predState = pred->state.load();
    // we have to wait for our predecessor to establish whether it is active
    // or blocked...
    while (predState == State::kSharedInitializing) {
      backoff();
      predState = pred->state.load();
    }

    if (predState == State::kSharedActiveLeader ||
        predState == State::kSharedActiveFollower ||
        predState == State::kSharedFinished) {
      node->state.store(State::kSharedActiveFollower);
      return {SharedLockGuard(node)};
    }
    TRI_ASSERT(predState == State::kExclusiveLeader ||
               predState == State::kExclusiveBlocked ||
               predState == State::kSharedBlocked);
    node->state.store(State::kSharedBlocked);
    return node->promise.getFuture();
  }

  FutureType asyncLockExclusive() {
    auto* node = new Node(*this, ExclusiveTag{});
    auto* pred = _tail.exchange(node);
    if (pred == nullptr) {
      node->state.store(State::kExclusiveLeader);
      return {SharedLockGuard(node)};
    }
    pred->next.store(node);
    return node->promise.getFuture();
  }

  // TODO - add support for lock timeout
  // FutureType asyncTryLockSharedFor(std::chrono::microseconds timeout) {}

  // FutureType asyncTryLockExclusiveFor(std::chrono::microseconds timeout) {}

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // this method should only be used in the unit test to simulate different
  // scenarios!
  Node* tail() { return _tail.load(); }
#endif

  enum class State {
    kExclusiveLeader = 1,
    kExclusiveBlocked,
    kSharedInitializing,
    kSharedBlocked,
    kSharedActiveLeader,
    kSharedActiveFollower,
    kSharedFinished,
  };

 private:
  void backoff() {
#if (defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) ||   \
     defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) || \
     defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) ||    \
     defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) ||   \
     defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_8A__) ||   \
     defined(__aarch64__))
    asm volatile("yield" ::: "memory");
#elif (defined(__i386) || defined(_M_IX86) || defined(__x86_64__) || \
       defined(_M_X64))
    asm volatile("pause" ::: "memory");
#endif
  }

  void removeNode(std::unique_ptr<Node> node) {
    auto myState = node->state.load();
    TRI_ASSERT(myState == State::kExclusiveLeader ||
               myState == State::kSharedActiveLeader)
        << (int)myState;
    bool isSharedLeader = myState == State::kSharedActiveLeader;

  restart:
    auto next = node->next.load();
    if (next == nullptr) {
      auto expected = node.get();
      if (_tail.compare_exchange_strong(expected, nullptr)) {
        // no other nodes queued and we just reset tail to nullptr, so the
        // lock is now free again
        return;
      }
      // wait until predecessor fills in the next field
      next = node->next.load();
      while (next == nullptr) {
        backoff();
        next = node->next.load();
      }
    }

    auto nextState = next->state.load();
    // we need to wait until the next node has established its state
    while (nextState == State::kSharedInitializing) {
      backoff();
      nextState = next->state.load();
    }

    if (isSharedLeader) {
      // we are the leader of a group of shared locks
      if (nextState == State::kSharedActiveFollower) {
        // we have an active follower, so let's try to make it the new leader
        nextState = next->state.exchange(State::kSharedActiveLeader);
        if (nextState == State::kSharedActiveFollower) {
          // next node is now leader -> we are done!
          return;
        }
      }
      TRI_ASSERT(nextState == State::kSharedFinished ||
                 nextState == State::kExclusiveBlocked)
          << (int)nextState;
      if (nextState == State::kSharedFinished) {
        // next node is finished -> we can remove it
        node.reset(next);
        goto restart;
      }
    }

    node.reset();

    TRI_ASSERT(nextState == State::kExclusiveBlocked ||
               nextState == State::kSharedBlocked);
    if (nextState == State::kExclusiveBlocked) {
      scheduleExclusive(next);
    } else {
      scheduleShared(next);
    }
  }

  void scheduleExclusive(Node* node) {
    // the next node now owns the lock - schedule the function that resolves
    // the node's promise
    node->state.store(State::kExclusiveLeader);
    scheduleNode(node);
  }

  void scheduleShared(Node* leader) {
    TRI_ASSERT(leader->state.load() == State::kSharedBlocked);
    // iterate the list of nodes, marking all shared requests as active and
    // scheduling them.
    // once we schedule our immediate successor (the group leader) it takes over
    // ownership and may unlock at any time, starting to clean up, so we must
    // schedule all other shared locks first, otherwise we could not safely
    // traverse the list
    leader->state.store(State::kSharedActiveLeader);
    auto next = leader->next.load();
    while (next) {
      auto state = next->state.load();
      // we need to wait until the next node has established its state
      while (state == State::kSharedInitializing) {
        backoff();
        state = next->state.load();
      }
      TRI_ASSERT(state == State::kExclusiveBlocked ||
                 state == State::kSharedBlocked ||
                 state == State::kSharedActiveFollower ||
                 state == State::kSharedFinished);
      if (state == State::kSharedBlocked) {
        next->state.store(State::kSharedActiveFollower);
        scheduleNode(next);
        next = next->next.load();
      } else {
        // if the next node is an active follower or finished, all later shared
        // nodes will also be active/finished, so we can stop here
        TRI_ASSERT(state == State::kExclusiveBlocked ||
                   state == State::kSharedActiveFollower ||
                   state == State::kSharedFinished);
        break;
      }
    }

    scheduleNode(leader);
  }

  void scheduleNode(Node* node) {
    TRI_ASSERT(node != nullptr);
    _scheduler.queue(
        [node]() { node->promise.setValue(SharedLockGuard(node)); });
  }

  struct ExclusiveTag {};
  struct SharedTag {};

  struct Node : SharedLock {
    Node(FutureSharedLock& lock, ExclusiveTag)
        : lock(lock), state(State::kExclusiveBlocked) {}
    Node(FutureSharedLock& lock, SharedTag)
        : lock(lock), state(State::kSharedInitializing) {}

    FutureSharedLock& lock;
    Promise<SharedLockGuard> promise;
    std::atomic<Node*> next{nullptr};
    std::atomic<State> state;

    void unlock() noexcept override {
      auto s = state.load();
      TRI_ASSERT(s != State::kSharedInitializing &&
                 s != State::kSharedFinished && s != State::kSharedBlocked);
      if (s == State::kSharedActiveFollower) {
        if (state.compare_exchange_strong(s, State::kSharedFinished)) {
          // we marked ourselves finished before the leader finished
          // -> the leader will clean up; we are done!
          return;
        }
        // we we cannot mark ourselves finished, the leader must have
        // transferred leadership to us
        TRI_ASSERT(s == State::kSharedActiveLeader);
      }

      lock.removeNode(std::unique_ptr<Node>(this));
    }
  };

  Scheduler& _scheduler;
  std::atomic<Node*> _tail;
};

}  // namespace arangodb::futures
