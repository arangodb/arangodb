#pragma once

#include "Assertions/ProdAssert.h"
#include "promise.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace arangodb::coroutine {

/**
   Get registry of all active coroutine promises on this thread.
 */
auto get_thread_registry() noexcept -> ThreadRegistry&;

/**
   This registry belongs to a specific thread (the owning thread) and owns a
   list of promises that live on this thread.

   A promise can be marked for deletion on any thread, the garbage collection
   needs to be called manually on the owning thread and destroys all marked
   promises. A promise can only be added on the owning thread, therefore adding
   and garbage collection cannot happen concurrently. The garbage collection can
   also not run during an iteration over all promises in the list.
 */
struct ThreadRegistry {
  /**
     Adds a promise on the registry's thread to the registry.

     Can only be called on the owning thread, crashes
     otherwise.
   */
  auto add(PromiseInList* promise) noexcept -> void {
    // promise needs to live on the same thread as this registry
    ADB_PROD_ASSERT(std::this_thread::get_id() == owning_thread);
    auto current_head = promise_head.load(std::memory_order_relaxed);
    promise->next.store(current_head, std::memory_order_relaxed);
    promise->registry = this;
    if (current_head != nullptr) {
      current_head->previous.store(promise, std::memory_order_relaxed);
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
  requires requires(F f, PromiseInList* promise) { {f(promise)}; }
  auto for_promise(F&& function) noexcept -> void {
    auto guard = std::lock_guard(mutex);
    // (2) - this load synchronizes with store in (1)
    for (auto current = promise_head.load(std::memory_order_acquire);
         current != nullptr;
         // (5) - this load synchronizes with store in (3) and (4)
         current = current->next.load(std::memory_order_acquire)) {
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
    ADB_PROD_ASSERT(promise->registry == this);
    // (1) - this load synchronizes with compare_exchange_weak in (2)
    auto current_head = free_head.load(std::memory_order_acquire);
    do {
      promise->next_to_free.store(current_head, std::memory_order_relaxed);
      // (2) - this compare_exchange_weak synchronizes with load in (1)
    } while (not free_head.compare_exchange_weak(current_head, promise,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));
  }

  /**
     Deletes all promises that are marked for deletion.

     Can only be called on the owning thread, crashes otheriwse.
   */
  auto garbage_collect() noexcept -> void {
    ADB_PROD_ASSERT(std::this_thread::get_id() == owning_thread);
    PromiseInList* head;
    {
      auto guard = std::lock_guard(mutex);
      // (7) - this exchange synchronizes with load in (1) and store in (2)
      head = free_head.exchange(nullptr, std::memory_order_acq_rel);
    }
    for (auto current = head; current != nullptr;) {
      auto next = current->next_to_free.load(std::memory_order_relaxed);
      remove(current);
      current->destroy();
      current = next;
    }
  }

  const std::thread::id owning_thread = std::this_thread::get_id();
  std::atomic<PromiseInList*> free_head = nullptr;
  std::atomic<PromiseInList*> promise_head = nullptr;
  std::mutex mutex;

 private:
  /**
     Removes the promise from the registry.

     Caller needs to make sure that the given promise is part of this registry
     (which also means that this function should only be called on the owning
     thread.)
   */
  auto remove(PromiseInList* promise) -> void {
    auto next = promise->next.load(std::memory_order_relaxed);
    auto previous = promise->previous.load(std::memory_order_relaxed);
    if (previous == nullptr) {  // promise is current head
      // (3) - this store synchronizes with the load in (5)
      promise_head.store(next, std::memory_order_release);
    } else {
      // (4) - this store synchronizes with the load in (5)
      previous->next.store(next, std::memory_order_release);
    }
    if (next != nullptr) {
      next->previous.store(previous, std::memory_order_relaxed);
    }
  }
};

}  // namespace arangodb::coroutine
