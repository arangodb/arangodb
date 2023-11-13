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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <atomic>

// This lock-free multi-producer-single-consumer-queue is inspired by the
// code published at
//
// https://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
//
// under a Apache-2.0 license.
namespace arangodb::actor {

template<typename T>
struct MPSCQueue {
  struct Node {
    virtual ~Node() = default;
    std::atomic<Node*> next{nullptr};
  };

  MPSCQueue() noexcept : stub{}, head(&stub), tail(&stub) {}
  MPSCQueue(MPSCQueue const&) = delete;
  MPSCQueue(MPSCQueue&&) = delete;
  MPSCQueue& operator=(MPSCQueue const&) = delete;
  MPSCQueue& operator=(MPSCQueue&&) = delete;

  auto push_internal(Node* value) noexcept -> void {
    value->next.store(nullptr);
    auto prev = head.exchange(value);
    prev->next.store(value);
  }

  auto push(std::unique_ptr<T> value) noexcept -> void {
    auto ptr = value.release();
    push_internal(ptr);
  }

  auto empty() noexcept -> bool {
    // We read first tail and then head, because
    auto currentTail = tail.load();
    auto currentHead = head.load();

    return currentHead == currentTail;
  }

  [[nodiscard]] auto pop() noexcept -> std::unique_ptr<T> {
    auto current = tail.load();
    auto next = current->next.load();

    if (current == &stub) {
      // stub->next == nullptr means the queue
      // currently does not contain reachable
      // elements
      if (nullptr == next) {
        return nullptr;
      }
      // otherwise just move on past stub.
      tail.store(next);
      current = next;
      next = next->next.load();
    }

    // not reached the current head yet.
    if (next != nullptr) {
      //  move tail
      tail.store(next);
      // don't leak a pointer into the queue
      current->next.store(nullptr);
      return std::unique_ptr<T>(static_cast<T*>(current));
    }

    // we are now at the end of the
    // *visible* linear list;
    auto currenth = head.load();
    if (current != currenth) {
      return nullptr;
    }

    // we are in the situation where we popped
    // everything up to the last element (which
    // head points at!)
    // Since someone else could still be trying
    // to add stuff to us, we add the stub so
    // they can
    push_internal(&stub);

    // might be stub. or anything else. but
    // we don't care, we just move on
    next = current->next.load();

    if (next != nullptr) {
      tail.store(next);
      // do not leak pointer into queue
      current->next.store(nullptr);
      return std::unique_ptr<T>(static_cast<T*>(current));
    }

    return nullptr;
  }

  auto flush() noexcept -> void {
    auto node = pop();
    while (node != nullptr) {
      auto ptr = node.release();
      delete ptr;
      node = pop();
    }
  }

  ~MPSCQueue() noexcept { flush(); }

 private:
  Node stub{};
  std::atomic<Node*> head;  // pushed to
  std::atomic<Node*> tail;  // popped from
};

}  // namespace arangodb::actor
