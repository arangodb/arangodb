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
#include <cassert>
#include <mutex>

#include "Futures/Exceptions.h"
#include "Futures/Future.h"

namespace arangodb::futures {

struct SharedLock {
  virtual ~SharedLock() = default;
  virtual void unlock() noexcept = 0;
};

// TODO - perhaps use a custom LockGuard type
using SharedLockGuard = std::unique_lock<SharedLock>;

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
      return {SharedLockGuard(*node, std::adopt_lock)};
    }

    auto predState = pred->state.load();
    // we have to wait for our predecessor to establish whether it is active
    // or blocked...
    while (predState == State::kSharedInitializing) {
      // backoff
      predState = pred->state.load();
    }

    if (predState == State::kSharedActiveLeader ||
        predState == State::kSharedActiveFollower ||
        predState == State::kSharedFinished) {
      node->state.store(State::kSharedActiveFollower);
      pred->next.store(node);
      return {SharedLockGuard(*node, std::adopt_lock)};
    }
    assert(predState == State::kExclusive ||
           predState == State::kSharedBlocked);
    node->state.store(State::kSharedBlocked);
    pred->next.store(node);
    return node->promise.getFuture();
  }

  FutureType asyncLockExclusive() {
    auto* node = new Node(*this, ExclusiveTag{});
    auto* pred = _tail.exchange(node);
    if (pred == nullptr) {
      return {SharedLockGuard(*node, std::adopt_lock)};
    }
    pred->next.store(node);
    return node->promise.getFuture();
  }

  // TODO - add support for lock timeout

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // this method should only be used in the unit test to simulate different
  // scenarios!
  Node* tail() { return _tail.load(); }
#endif

  enum class State {
    kExclusive,
    kSharedInitializing,
    kSharedBlocked,
    kSharedActiveLeader,
    kSharedActiveFollower,
    kSharedFinished,
  };

 private:
  void removeNode(std::unique_ptr<Node> node) {
    auto state = node->state.load();
    assert(state == State::kExclusive || state == State::kSharedActiveLeader);
    bool isSharedLeader = state == State::kSharedActiveLeader;

  restart:
    auto next = node->next.load(std::memory_order_relaxed);
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
        // backoff
        next = node->next.load();
      }
    }

    if (isSharedLeader) {
      // we are the leader of a group of shared locks
      state = next->state.load();
      // our next pointer has already been updated, so the next node cannot be
      // initializing
      assert(state != State::kSharedInitializing);
      if (state == State::kSharedActiveFollower) {
        // we have an active follower, so let's try to make it the new leader
        if (next->state.exchange(State::kSharedActiveLeader) ==
            State::kSharedActiveFollower) {
          // next node is now leader -> we are done!
          return;
        }
      }
      if (state == State::kSharedFinished) {
        // next node finished before we could make it leader -> we take over
        // ownership
        node.reset(next);
        goto restart;
      }
    }

    node.reset();

    state = next->state.load();
    assert(state == State::kExclusive || state == State::kSharedBlocked);
    if (state == State::kExclusive) {
      scheduleExclusive(next);
    } else {
      scheduleShared(next);
    }
  }

  void scheduleExclusive(Node* node) {
    // the next node now owns the lock - schedule the function that resolves
    // the node's promise
    scheduleNode(node);
  }

  void scheduleShared(Node* node) {
    assert(node->state.load() == State::kSharedBlocked);
    // iterate the list of nodes, marking all shared requests as active and
    // scheduling them.
    // once we schedule our immediate successor (the group leader) it takes over
    // ownership and may unlock at any time, starting to cleaning, so we must
    // schedule all other shared locks first, otherwise we could not safely
    // traverse the list
    node->state.store(State::kSharedActiveLeader);
    auto next = node->next.load();
    while (next) {
      auto state = next->state.load();
      assert(state == State::kExclusive || state == State::kSharedBlocked);
      if (state == State::kExclusive) {
        break;
      }
      next->state.store(State::kSharedActiveFollower);
      scheduleNode(next);
      next = next->next.load();
    }
    scheduleNode(node);
  }

  void scheduleNode(Node* node) {
    assert(node != nullptr);
    _scheduler.queue([node]() {
      node->promise.setValue(SharedLockGuard(*node, std::adopt_lock));
    });
  }

  struct ExclusiveTag {};
  struct SharedTag {};

  struct Node : SharedLock {
    Node(FutureSharedLock& lock, ExclusiveTag)
        : lock(lock), state(State::kExclusive) {}
    Node(FutureSharedLock& lock, SharedTag)
        : lock(lock), state(State::kSharedInitializing) {}

    FutureSharedLock& lock;
    Promise<SharedLockGuard> promise;
    std::atomic<Node*> next{nullptr};
    std::atomic<State> state;

    void unlock() noexcept override {
      auto s = state.load();
      assert(s != State::kSharedInitializing && s != State::kSharedFinished &&
             s != State::kSharedBlocked);
      if (s == State::kSharedActiveFollower) {
        s = state.exchange(State::kSharedFinished);
        if (s == State::kSharedActiveFollower) {
          // we marked ourselves finished before the leader finished
          // -> the leader will clean up; we are done!
          return;
        }
      }

      lock.removeNode(std::unique_ptr<Node>(this));
    }
  };

  Scheduler& _scheduler;
  std::atomic<Node*> _tail;
};

}  // namespace arangodb::futures
