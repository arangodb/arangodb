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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Containers/Concurrent/thread.h"
#include "Containers/Concurrent/snapshot.h"
#include "Containers/Concurrent/metrics.h"
#include "Inspection/Format.h"

#include <atomic>
#include <concepts>
#include <format>
#include <memory>
#include <shared_mutex>

namespace arangodb::containers {

/**
   This list is supposed to be owned by one thread. Nodes can only be added on
   its owning thread. But other threads can read the list and mark nodes for
   deletion. The list exists as long a the owning thread lives or a node is
   referenced somewhere.

   Nodes are automatically marked for deletion when they go out of scope.
   Garbage collection for these marked nodes has to be manually started either
   on the owning thread (via garbage_collect()) or on another thread (via
   garbage_collect_external()).

   A thread owned list contains an atomic list of nodes. If a node is marked
   for deletion, it stays in this list, but is additionally added to the atomic
   free list. The garbage collection goes through this free list, removes each
   node in this free list from the node list (and from the free list as
   well) and then destroys the node.
 */
template<typename T>
requires HasSnapshot<T>
struct ThreadOwnedList
    : public std::enable_shared_from_this<ThreadOwnedList<T>> {
  using Item = T;
  basics::ThreadId const thread;

  struct MarkForDeletion;

  struct Node {
    friend MarkForDeletion;
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
    // identifies the promise list it belongs to, to be able to mark itself for
    // deletion
    ThreadOwnedList<T>& list;
    std::atomic<bool> is_marked_for_deletion = false;

    Node() = default;

    template<typename F>
    requires requires(F f) {
      { f() } -> std::same_as<T>;
    }
    Node(F&& create_data, Node* next, ThreadOwnedList<T>& list)
        : data{create_data()}, next{next}, list{list} {}

    template<typename U = T>
    requires std::movable<U> Node(T data, ThreadOwnedList<T>& list)
        : data{std::move(data)}, list{list} {}

    virtual ~Node() = default;

   private:
    auto mark_for_deletion() -> void { list.mark_for_deletion(*this); }
  };

 private:
  std::atomic<Node*> _head = nullptr;
  std::atomic<Node*> _free_head = nullptr;
  std::shared_mutex _mutex;  // gc and reading cannot happen at same time
  std::shared_ptr<Metrics> _metrics;

 public:
  static auto make(std::shared_ptr<Metrics> metrics = nullptr) noexcept
      -> std::shared_ptr<ThreadOwnedList> {
    // (7) - this load synchronizes with the store in (6)
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

  /**
     Deleter for references to nodes in the list.

     Nodes are owned by the list, therefore only the list can delete them in a
     garbage collection. The last reference to a node marks the node for
     deletion such that the next garbage collection will delete it. This deleter
     makes sure that the list is still alive when mark_for_deletion is called on
     the node.
   */
  struct MarkForDeletion {
    // keeps list alive until mark_for_deletion is called
    std::shared_ptr<ThreadOwnedList> list;
    auto operator()(Node* node) const noexcept -> void {
      node->mark_for_deletion();
    }
  };
  template<std::derived_from<Node> NodeType = Node>
  using Handle = std::unique_ptr<NodeType, MarkForDeletion>;

  /**
     Adds a node to the list with data that is created in the closure.

     Can only be called on the owning thread, crashes otherwise. Input data
     needs to be given as a callback to be able to use data types that are
     non-movable and non-copyable.
   */
  template<typename F>
  requires requires(F f) {
    { f() } -> std::same_as<T>;
  }
  auto add(F&& create_data) noexcept -> Handle<> {
    auto current_thread = basics::ThreadId::current();
    ADB_PROD_ASSERT(current_thread == thread)
        << "ThreadOwnedList::add was called from thread "
        << inspection::json(current_thread)
        << " but needs to be called from ThreadOwnedList's owning thread "
        << inspection::json(thread) << ". " << (void*)this;
    auto current_head = _head.load(std::memory_order_relaxed);
    auto node = new Node{create_data, current_head, *this};
    if (current_head != nullptr) {
      // (6) - this store synchronizes with the load in (7) and (9)
      current_head->previous.store(node, std::memory_order_release);
    }
    // (1) - this store synchronizes with load in (2)
    _head.store(node, std::memory_order_release);
    if (_metrics) {
      _metrics->increment_registered_nodes();
      _metrics->increment_total_nodes();
    }
    return Handle<>{node, MarkForDeletion{this->shared_from_this()}};
  }

  /**
   Adds a node to the list.

   Can only be called on the owning thread, crashes otherwise. The node can be
   of any type derived from Node, so additional data can live in the node
   without an extra allocation.
 */
  template<std::derived_from<Node> DerivedNode>
  auto add(std::unique_ptr<DerivedNode> node) noexcept -> Handle<DerivedNode> {
    auto current_thread = basics::ThreadId::current();
    ADB_PROD_ASSERT(current_thread == thread)
        << "ThreadOwnedList::add was called from thread "
        << inspection::json(current_thread)
        << " but needs to be called from ThreadOwnedList's owning thread "
        << inspection::json(thread) << ". " << (void*)this;
    TRI_ASSERT(&node->list == this)
        << "ThreadOwnedList::add was called for a node with an incorrect "
           "list.";
    auto current_head = _head.load(std::memory_order_relaxed);
    auto* const raw_node = node.release();
    raw_node->next = current_head;
    if (current_head != nullptr) {
      // (6) - this store synchronizes with the load in (7) and (9)
      current_head->previous.store(raw_node, std::memory_order_release);
    }
    // (1) - this store synchronizes with load in (2)
    _head.store(raw_node, std::memory_order_release);
    if (_metrics) {
      _metrics->increment_registered_nodes();
      _metrics->increment_total_nodes();
    }
    return Handle<DerivedNode>{raw_node,
                               MarkForDeletion{this->shared_from_this()}};
  }

  /**
     Executes a function on each node in the list that is not yet
     marked-for-deletion.

     Can be called from any thread. It makes sure that all
     items stay valid during iteration (i.e. are not deleted in the meantime).
   */
  template<typename F>
  requires std::invocable<F, typename T::Snapshot>
  auto for_node(F&& function) noexcept -> void {
    auto guard = std::shared_lock(_mutex);
    // (2) - this load synchronizes with store in (1) and (3)
    for (auto current = _head.load(std::memory_order_acquire);
         current != nullptr; current = current->next) {
      if (not current->is_marked_for_deletion.load(std::memory_order_relaxed)) {
        // This can still execute 'function' when the node was just marked for
        // deletion, leading to slightly inconsistent results. But this cannot
        // lead to any errors because garbage collection cannot run at the same
        // time as this 'for_node' function.
        function(current->data.snapshot());
      }
    }
  }

  auto size() noexcept -> size_t {
    size_t count = 0;
    auto guard = std::shared_lock(_mutex);
    // (2) - this load synchronizes with store in (1) and (3)
    for (auto current = _head.load(std::memory_order_acquire);
         current != nullptr; current = current->next) {
      count++;
    }
    return count;
  }

  /**
     Deletes all nodes that are marked for deletion.

     Can only be called on the owning thread, crashes otherwise.
   */
  auto garbage_collect() noexcept -> size_t {
    auto current_thread = basics::ThreadId::current();
    ADB_PROD_ASSERT(current_thread == thread)
        << "ThreadOwnedList::garbage_collect was called from thread "
        << inspection::json(current_thread)
        << " but needs to be called from ThreadOwnedList's owning thread "
        << inspection::json(thread) << ". " << (void*)this;
    auto guard = std::lock_guard(_mutex);
    return cleanup();
  }

  /**
     Runs external garbage collection.

     This can be called from any thread. Cannot delete the head of the
     list, calling this will therefore result in at least one
     marked-for-deletion node.
   */
  auto garbage_collect_external() noexcept -> size_t {
    // acquire the lock. This prevents the owning thread and the observer
    // from accessing nodes. Note that the owing thread only adds new
    // nodes to the head of the list.
    auto guard = std::lock_guard(_mutex);
    // we can make the following observation. Once a node is enqueued in the
    // list, its previous and next pointer is never updated, except for the
    // current head element. Also, nodes are only removed, after the mutex
    // has been acquired. This implies that we can clean up all nodes that
    // are not in head position right now.
    Node* maybe_head_ptr = nullptr;
    Node* current;
    Node* next = _free_head.exchange(nullptr, std::memory_order_acquire);
    size_t count = 0;
    while (next != nullptr) {
      current = next;
      next = next->next_to_free;
      // (9) - this load synchronizes with the store in (6) and (8)
      if (current->previous.load(std::memory_order_acquire) != nullptr) {
        if (_metrics) {
          _metrics->decrement_ready_for_deletion_nodes();
        }
        remove(current);
        delete current;
        count++;
      } else {
        // if this is the head of the list, we cannot delete it because
        // additional nodes could have been added in the meantime
        // (if these new nodes would have been marked in the meantime, they
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
      } while (not _free_head.compare_exchange_weak(
          current_head, maybe_head_ptr, std::memory_order_release,
          std::memory_order_acquire));
    }
    return count;
  }

 private:
  ThreadOwnedList(std::shared_ptr<Metrics> metrics) noexcept
      : thread{basics::ThreadId::current()}, _metrics{metrics} {
    // is now done in ListOfLists
    if (_metrics) {
      _metrics->increment_total_lists();
      _metrics->increment_existing_lists();
    }
  }

  /**
     Marks a node in the list for deletion.

     Can be called from any thread. The node needs to be part of the list,
     crashes otherwise.
     Caller needs to make sure that this is not called twice: otherwise there
     will be a double free.
   */
  auto mark_for_deletion(Node& node) noexcept -> void {
    // makes sure that node is really in this list
    ADB_PROD_ASSERT(&node.list == this);
    bool was_marked = false;
    node.is_marked_for_deletion.compare_exchange_strong(
        was_marked, true, std::memory_order_relaxed, std::memory_order_relaxed);
    ADB_PROD_ASSERT(not was_marked)
        << "ThreadOwnedList::mark_for_deletion: Node cannot be marked for "
           "deletion more than once";

    auto current_head = _free_head.load(std::memory_order_relaxed);
    do {
      node.next_to_free = current_head;
      // (4) - this compare_exchange_weak synchronizes with exchange in (5)
    } while (not _free_head.compare_exchange_weak(current_head, &node,
                                                  std::memory_order_release,
                                                  std::memory_order_acquire));
    // DO NOT access node after this line. The owner thread might already
    // be running a cleanup and node might be deleted.

    if (_metrics) {
      _metrics->decrement_registered_nodes();  // crash here
      _metrics->increment_ready_for_deletion_nodes();
    }
  }

  auto cleanup() noexcept -> size_t {
    size_t count = 0;
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
      count++;
    }
    return count;
  }

  auto remove(Node* node) -> void {
    auto* next = node->next;
    // (7) - this load synchronizes with the store in (6) and (8)
    auto* previous = node->previous.load(std::memory_order_acquire);
    if (previous == nullptr) {  // promise is current head
      // (3) - this store synchronizes with the load in (2)
      _head.store(next, std::memory_order_release);
    } else {
      previous->next = next;
    }
    if (next != nullptr) {
      // (8) - this store synchronizes with the load in (7) and (9)
      next->previous.store(previous, std::memory_order_release);
    }
  }
};

}  // namespace arangodb::containers
