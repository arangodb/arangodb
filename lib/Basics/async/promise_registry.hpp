#pragma once

#include "Assertions/ProdAssert.h"
#include "Basics/Result.h"
#include "promise.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <unordered_set>
#include <thread>

namespace arangodb::coroutine {

struct PromiseRegistry {
  auto garbage_collect() -> void {
    auto guard = std::lock_guard(mutex);
    for (auto current = free_head.exchange(nullptr); current != nullptr;) {
      auto next = current->next_to_free.load(std::memory_order_relaxed);
      erase(current);
      current = next;
    }
  }

  /**
     Add a promise on the current thread to the list.

     Returns an error if this function is called on a differnt thread than the
     thread the registry lives on.
   */
  auto add(PromiseInList* promise) -> Result {
    if (std::this_thread::get_id() != thread_id) {
      return Result{TRI_ERROR_INTERNAL,
                    "You cannot add a promise of another thread to this "
                    "promise list."};
    }
    auto current_head = promise_head.load(std::memory_order_relaxed);
    promise->next.store(current_head, std::memory_order_relaxed);
    if (current_head != nullptr) {
      current_head->previous.store(promise, std::memory_order_relaxed);
    }
    // (1) sets value read by (2)
    promise_head.store(promise, std::memory_order_release);
    return Result{};
  }

  /**
     Execute a function on each promise in the list.

     This function can be called from any thread. It makes sure that it only
     runs on a valid list and not during removal of items.
   */
  auto for_promise(std::function<void(PromiseInList*)> function) -> void {
    auto guard = std::lock_guard(mutex);
    // (2) reads value set by (1)
    for (auto current = promise_head.load(std::memory_order_acquire);
         current != nullptr;
         // (5) read value set by (3) or (4)
         current = current->next.load(std::memory_order_acquire)) {
      function(current);
    }
  }

  auto register_to_delete(PromiseInList* promise) -> Result {
    // makes sure that promise is really in this list
    ADB_PROD_ASSERT(promise->registry == this);
    // (1) loads value set by (2)
    auto current_head = free_head.load(std::memory_order_acquire);
    do {
      promise->next_to_free.store(current_head, std::memory_order_relaxed);
      // (2) sets value load by (1)
    } while (not free_head.compare_exchange_weak(current_head, promise,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));
    return Result{};
  }

  const std::thread::id thread_id = std::this_thread::get_id();
  std::atomic<PromiseInList*> free_head = nullptr;
  std::atomic<PromiseInList*> promise_head = nullptr;
  std::mutex mutex;

  /**
     Erase a promise on the current thread.

     This removes the promise from the list and destroys the promise.
     Returns an error if this function is called for a promise that is not part
     of this list.
   */
 private:
  auto erase(PromiseInList* promise) -> void {
    auto next = promise->next.load(std::memory_order_relaxed);
    auto previous = promise->previous.load(std::memory_order_relaxed);
    if (previous == nullptr) {  // promise is current head
      // (4) sets value read by (5)
      promise_head.store(next, std::memory_order_release);
    } else {
      // (3) sets value read by (5)
      previous->next.store(next, std::memory_order_release);
    }
    if (next != nullptr) {
      next->previous.store(previous, std::memory_order_relaxed);
    }
    promise->destroy();
  }
};

}  // namespace arangodb::coroutine
