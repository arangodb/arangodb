#pragma once

#include "Async/Registry/promise.h"
#include "Metrics/GaugeCounterGuard.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace arangodb::metrics {
template<typename T>
class Gauge;
}
namespace arangodb::async_registry {

struct Metrics;

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
  static auto make(std::shared_ptr<const Metrics> metrics)
      -> std::shared_ptr<ThreadRegistry>;

  ~ThreadRegistry() noexcept { cleanup(); }

  /**
     Adds a promise on the registry's thread to the registry.

     Can only be called on the owning thread, crashes
     otherwise.
   */
  auto add(PromiseInList* promise) noexcept -> void;

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
  auto mark_for_deletion(PromiseInList* promise) noexcept -> void;

  /**
     Deletes all promises that are marked for deletion.

     Can only be called on the owning thread or last thread working with this
     registry, crashes otheriwse.
   */
  auto garbage_collect() noexcept -> void;

 private:
  const std::thread::id owning_thread = std::this_thread::get_id();
  std::atomic<PromiseInList*> free_head = nullptr;
  std::atomic<PromiseInList*> promise_head = nullptr;
  std::mutex mutex;
  metrics::GaugeCounterGuard<uint64_t> running_threads;
  std::shared_ptr<const Metrics> metrics;

  ThreadRegistry(std::shared_ptr<const Metrics> metrics);
  auto cleanup() noexcept -> void;

  /**
     Removes the promise from the registry.

     Caller needs to make sure that the given promise is part of this registry
     (which also means that this function should only be called on the owning
     thread.)
   */
  auto remove(PromiseInList* promise) -> void;
};

}  // namespace arangodb::async_registry
