#pragma once

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

/**
   List of coroutine promises on one thread.

   Add and remove are called on this thread, therefore they are not supposed
   to run concurrently. for_promise is called from a different thread and can
   run concurrently to add. But remove and for_promise are mutually exclusive
   via a mutex to make sure that for_promise always iterates over a valid list.

 */
struct PromiseRegistryOnThread {
  /**
     Add a promise on the current thread to the list.

     Returns an error if this function is called on a differnt thread than the
     thread the registry lives on.
   */
  auto add(PromiseInList* promise) -> Result {
    if (std::this_thread::get_id() != thread_id) {
      return Result{TRI_ERROR_INTERNAL,
                    "You cannot add a promise of another thread to this "
                    "promise registry."};
    }
    auto current_head = head.load(std::memory_order_relaxed);
    promise->next.store(current_head, std::memory_order_relaxed);
    if (current_head != nullptr) {
      current_head->previous.store(promise, std::memory_order_relaxed);
    }
    // (1) sets value read by (2)
    head.store(promise, std::memory_order_release);
    return Result{};
  }

  /**
     Erase a promise on the current thread.

     This removes the promise from the list and destroys the promise.
     Returns an error if this function is called on a differnt thread than the
     thread the registry lives on.
   */
  auto erase(PromiseInList* promise) -> Result {
    if (std::this_thread::get_id() != thread_id) {
      return Result{TRI_ERROR_INTERNAL,
                    "You cannot erase a promise of another thread from this "
                    "promise registry."};
    }
    auto guard = std::lock_guard(mutex);
    auto next = promise->next.load(std::memory_order_relaxed);
    auto previous = promise->previous.load(std::memory_order_relaxed);
    if (previous == nullptr) {  // promise is current head
      // (4) sets value read by (5)
      head.store(next, std::memory_order_release);
    } else {
      // (3) sets value read by (5)
      previous->next.store(next, std::memory_order_release);
    }
    if (next != nullptr) {
      next->previous.store(previous, std::memory_order_relaxed);
    }
    // TODO delete promise
    return Result{};
  }

  /**
     Execute a function on each promise in the list.

     This function can be called from any thread.
   */
  auto for_promise(std::function<void(PromiseInList*)> function) -> void {
    auto guard = std::lock_guard(mutex);
    // (2) reads value set by (1)
    for (auto current = head.load(std::memory_order_acquire);
         current != nullptr;
         // (5) read value set by (3) or (4)
         current = current->next.load(std::memory_order_acquire)) {
      function(current);
    }
  }

  const std::thread::id thread_id = std::this_thread::get_id();
  std::atomic<PromiseInList*> head = nullptr;
  std::atomic<std::shared_ptr<PromiseRegistryOnThread>> next;
  std::mutex mutex;
};

}  // namespace arangodb::coroutine
