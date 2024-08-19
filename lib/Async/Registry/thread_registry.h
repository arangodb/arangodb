#pragma once

#include "Assertions/ProdAssert.h"
#include "promise.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace arangodb::async_registry {

/**
   This registry belongs to a specific thread (the owning thread) and owns a
   list of promises that live on this thread.

   A promise can be marked for deletion on any thread, the garbage collection
   needs to be called manually on the owning thread and destroys all marked
   promises. A promise can only be added on the owning thread, therefore adding
   and garbage collection cannot happen concurrently. The garbage collection can
   also not run during an iteration over all promises in the list.

   This registry destroys itself when its ref counter is decremented to 0.
 */
struct ThreadRegistry : std::enable_shared_from_this<ThreadRegistry> {
  ThreadRegistry() = default;

  ~ThreadRegistry() noexcept { garbage_collect(); }

  /**
     Adds a promise on the registry's thread to the registry.

     Can only be called on the owning thread, crashes
     otherwise.
   */
  auto add(PromiseInList* promise) noexcept -> void {
    // promise needs to live on the same thread as this registry
    ADB_PROD_ASSERT(std::this_thread::get_id() == owning_thread);
    auto current_head = promise_head.load(std::memory_order_relaxed);
    promise->next = current_head;
    promise->registry = shared_from_this();
    if (current_head != nullptr) {
      current_head->previous = promise;
    }
    // (1) - this store synchronizes with load in (2)
    promise_head.store(promise, std::memory_order_release);
  }

  /**
     Executes a function on each promise in the registry.

     Can be called from any thread. It makes sure that all
     items stay valid during iteration (i.e. are not deleted in the meantime).
   */
  template<typename F>
  requires std::invocable<F, PromiseInList*>
  auto for_promise(F&& function) noexcept -> void {
    auto guard = std::lock_guard(mutex);
    // (2) - this load synchronizes with store in (1) and (3)
    for (auto current = promise_head.load(std::memory_order_acquire);
         current != nullptr; current = current->next) {
      function(current);
    }
  }

  /**
     Marks a promise in the registry for deletion.

     Can be called from any thread. The promise needs to be included in
     the registry list, crashes otherwise.
   */
  auto mark_for_deletion(PromiseInList* promise) noexcept -> void {
    // makes sure that promise is really in this list
    ADB_PROD_ASSERT(promise->registry.get() == this);
    auto current_head = free_head.load(std::memory_order_relaxed);
    do {
      promise->next_to_free = current_head;
      // (4) - this compare_exchange_weak synchronizes with exchange in (5)
    } while (not free_head.compare_exchange_weak(current_head, promise,
                                                 std::memory_order_release,
                                                 std::memory_order_acquire));
    // decrement the registries ref-count
    promise->registry.reset();
  }

  /**
     Deletes all promises that are marked for deletion.

     Can only be called on the owning thread or last thread working with this
     registry, crashes otheriwse.
   */
  auto garbage_collect() noexcept -> void {
    ADB_PROD_ASSERT(weak_from_this().expired() ||
                    std::this_thread::get_id() == owning_thread);
    // (5) - this exchange synchronizes with compare_exchange_weak in (4)
    PromiseInList *current,
        *next = free_head.exchange(nullptr, std::memory_order_acquire);
    auto guard = std::lock_guard(mutex);
    while (next != nullptr) {
      current = next;
      next = next->next_to_free;
      remove(current);
      current->destroy();
    }
  }

 private:
  const std::thread::id owning_thread = std::this_thread::get_id();
  std::atomic<PromiseInList*> free_head = nullptr;
  std::atomic<PromiseInList*> promise_head = nullptr;
  std::mutex mutex;

  /**
     Removes the promise from the registry.

     Caller needs to make sure that the given promise is part of this registry
     (which also means that this function should only be called on the owning
     thread.)
   */
  auto remove(PromiseInList* promise) -> void {
    auto next = promise->next;
    auto previous = promise->previous;
    if (previous == nullptr) {  // promise is current head
      // (3) - this store synchronizes with the load in (2)
      promise_head.store(next, std::memory_order_release);
    } else {
      previous->next = next;
    }
    if (next != nullptr) {
      next->previous = previous;
    }
  }
};

}  // namespace arangodb::async_registry
