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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Containers/Concurrent/thread.h"
#include "Containers/Concurrent/snapshot.h"
#include "Containers/Concurrent/metrics.h"
#include "Inspection/Format.h"
#include "fmt/core.h"

#include <atomic>
#include <memory>

namespace arangodb::containers {

template<typename T>
concept CanBeSetToDeleted = requires(T t) {
  t.set_to_deleted();
};
/**
   This list is owned by one thread: nodes can only be added on this thread but
   other threads can read the list and mark nodes for deletion.

   Nodes have to manually be marked for deletion, otherwise nodes are not
   deleted and therfore also not this list because each node includes a
   shared_ptr to this list. Garbage collection can run either on the owning
   thread or on another thread (there are two distinct functions for gc).
 */
template<typename T>
requires HasSnapshot<T> && CanBeSetToDeleted<T>
struct ThreadOwnedList
    : public std::enable_shared_from_this<ThreadOwnedList<T>> {
  using Item = T;
  basics::ThreadId const thread;

  struct Node {
    T data;
    Node* next = nullptr;
    // this needs to be an atomic because it is accessed during garbage
    // collection which can happen in a different thread. This thread will
    // load the value. Since there is only one transition, i.e. from nullptr
    // to non-null ptr, any missed update will result in a pessimistic
    // execution and not an error. More precise, the item might not be
    // deleted, although it is not in head position and can be deleted. It
    // will be deleted next round.
    std::atomic<Node*> previous = nullptr;
    Node* next_to_free = nullptr;
    // identifies the promise list it belongs to, to be able to mark for
    // deletion
    std::shared_ptr<ThreadOwnedList<T>> list;
  };

 private:
  std::atomic<Node*> _head = nullptr;
  std::atomic<Node*> _free_head = nullptr;
  std::mutex _mutex;  // gc and reading cannot happen at same time (perhaps I
                      // can get rid of this with epoch based reclamation)
  std::shared_ptr<Metrics> _metrics;

 public:
  static auto make(
      std::shared_ptr<Metrics> metrics = std::make_shared<Metrics>()) noexcept
      -> std::shared_ptr<ThreadOwnedList> {
    struct MakeShared : ThreadOwnedList {
      MakeShared(std::shared_ptr<Metrics> shared_metrics)
          : ThreadOwnedList(shared_metrics) {}
    };
    return std::make_shared<MakeShared>(metrics);
  }

  ~ThreadOwnedList() noexcept {
    if (_metrics) {
      _metrics->decrement_existing_lists();
    }
    cleanup();
  }

  template<typename F>
  requires std::invocable<F, typename T::Snapshot>
  auto for_node(F&& function) noexcept -> void {
    auto guard = std::lock_guard(_mutex);
    // (2) - this load synchronizes with store in (1) and (3)
    for (auto current = _head.load(std::memory_order_acquire);
         current != nullptr; current = current->next) {
      function(current->data.snapshot());
    }
  }

  auto add(T data) noexcept -> Node* {
    auto current_thread = basics::ThreadId::current();
    ADB_PROD_ASSERT(current_thread == thread) << fmt::format(
        "ThreadOwnedList::add_promise was called from thread {} but needs to "
        "be "
        "called from ThreadOwnedList's owning thread {}. {}",
        inspection::json(current_thread), inspection::json(thread),
        (void*)this);
    auto current_head = _head.load(std::memory_order_relaxed);
    auto node = new Node{.data = std::move(data),
                         .next = current_head,
                         .list = this->shared_from_this()};
    if (current_head != nullptr) {
      current_head->previous.store(node);  // TODO memory order
    }
    // (1) - this store synchronizes with load in (2)
    _head.store(node, std::memory_order_release);
    if (_metrics) {
      _metrics->increment_registered_nodes();
      _metrics->increment_total_nodes();
    }
    return node;
  }

  auto mark_for_deletion(Node* node) noexcept -> void {
    // makes sure that promise is really in this list
    ADB_PROD_ASSERT(node->list.get() == this);

    node->data.set_to_deleted();

    // keep a local copy of the shared pointer. This promise might be the
    // last of the registry.
    auto self = std::move(node->list);

    auto current_head = _free_head.load(std::memory_order_relaxed);
    do {
      node->next_to_free = current_head;
      // (4) - this compare_exchange_weak synchronizes with exchange in (5)
    } while (not _free_head.compare_exchange_weak(current_head, node,
                                                  std::memory_order_release,
                                                  std::memory_order_acquire));
    // DO NOT access promise after this line. The owner thread might already
    // be running a cleanup and promise might be deleted.

    if (_metrics) {
      _metrics->decrement_registered_nodes();
      _metrics->increment_ready_for_deletion_nodes();
    }

    // self destroyed here. registry might be destroyed here as well.
  }

  auto garbage_collect() noexcept -> void {
    auto current_thread = basics::ThreadId::current();
    ADB_PROD_ASSERT(current_thread == thread) << fmt::format(
        "ThreadOwnedList::garbage_collect was called from thread {} but needs "
        "to "
        "be called from ThreadOwnedList's owning thread {}. {}",
        inspection::json(current_thread), inspection::json(thread),
        (void*)this);
    auto guard = std::lock_guard(_mutex);
    cleanup();
  }

  auto garbage_collect_external() noexcept -> void {
    // acquire the lock. This prevents the owning thread and the observer
    // from accessing promises. Note that the owing thread only adds new
    // promises to the head of the list.
    auto guard = std::lock_guard(_mutex);
    // we can make the following observation. Once a promise is enqueued in the
    // list, it previous and next pointer is never updated, except for the
    // current head element. Also, Promises are only removed, after the mutex
    // has been acquired. This implies that we can clean up all promises, that
    // are not in head position right now.
    Node* maybe_head_ptr = nullptr;
    Node *current,
        *next = _free_head.exchange(
            nullptr, std::memory_order_acquire);  // TODO check memory order
    while (next != nullptr) {
      current = next;
      next = next->next_to_free;
      if (current->previous != nullptr) {  // TODO memory order
        if (_metrics) {
          _metrics->decrement_ready_for_deletion_nodes();
        }
        remove(current);
        delete current;
      } else {
        // if this is the head of the promise list, we cannot delete it because
        // additional promises could have been added in the meantime
        // (if these new promises would have been marked in the meantime, they
        // would be in the new free list due to the exchange earlier)
        ADB_PROD_ASSERT(maybe_head_ptr == nullptr);
        maybe_head_ptr = current;
      }
    }
    // After the clean up we have to add the potential head back into the free
    // list.
    if (maybe_head_ptr) {
      auto current_head = _free_head.load(std::memory_order_relaxed);
      do {
        maybe_head_ptr->next_to_free = current_head;
        // (4) - this compare_exchange_weak synchronizes with exchange in (5)
        // TODO check memory order
      } while (not _free_head.compare_exchange_weak(
          current_head, maybe_head_ptr, std::memory_order_release,
          std::memory_order_acquire));
    }
  }

  auto set_metrics(std::shared_ptr<Metrics> metrics) -> void {
    _metrics = metrics;
  }

 private:
  ThreadOwnedList(std::shared_ptr<Metrics> metrics) noexcept
      : thread{basics::ThreadId::current()}, _metrics{metrics} {
    // is now done in ListOfLists
    // if (_metrics) {
    //   _metrics->increment_total_lists();
    //   _metrics->increment_existing_lists();
    // }
  }

  auto cleanup() noexcept -> void {
    // (5) - this exchange synchronizes with compare_exchange_weak in (4)
    Node *current,
        *next = _free_head.exchange(nullptr, std::memory_order_acquire);
    while (next != nullptr) {
      current = next;
      next = next->next_to_free;
      if (_metrics) {
        _metrics->decrement_ready_for_deletion_nodes();
      }
      remove(current);
      delete current;
    }
  }

  auto remove(Node* node) -> void {
    auto* next = node->next;
    auto* previous = node->previous.load();  // TODO memory order
    if (previous == nullptr) {               // promise is current head
      // (3) - this store synchronizes with the load in (2)
      _head.store(next, std::memory_order_release);
    } else {
      previous->next = next;
    }
    if (next != nullptr) {
      next->previous = previous;  // TODO memory order
    }
  }
};

}  // namespace arangodb::containers
