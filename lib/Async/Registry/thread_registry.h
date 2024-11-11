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

#include "Async/Registry/promise.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <source_location>
#include <thread>
#include "fmt/format.h"
#include "fmt/std.h"

namespace arangodb::metrics {
template<typename T>
class Gauge;
}
namespace arangodb::async_registry {

struct Metrics;
struct Registry;

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
  static auto make(std::shared_ptr<const Metrics> metrics,
                   Registry* registry = nullptr)
      -> std::shared_ptr<ThreadRegistry>;

  ~ThreadRegistry() noexcept;

  /**
     Adds a promise on the registry's thread to the registry.

     Can only be called on the owning thread, crashes
     otherwise.
   */
  auto add_promise(std::source_location location =
                       std::source_location::current()) noexcept -> Promise*;

  /**
     Executes a function on each promise in the registry that is not deleted yet
     (includes promises that are marked for deletion).

     Can be called from any thread. It makes sure that all
     items stay valid during iteration (i.e. are not deleted in the meantime).
   */
  template<typename F>
  requires std::invocable<F, PromiseSnapshot>
  auto for_promise(F&& function) noexcept -> void {
    auto guard = std::lock_guard(mutex);
    // (2) - this load synchronizes with store in (1) and (3)
    for (auto current = promise_head.load(std::memory_order_acquire);
         current != nullptr; current = current->next) {
      function(current->snapshot());
    }
  }

  /**
     Marks a promise in the registry for deletion.

     Can be called from any thread. The promise needs to be included in
     the registry list, crashes otherwise.
   */
  auto mark_for_deletion(Promise* promise) noexcept -> void;

  /**
     Deletes all promises that are marked for deletion.

     Can only be called on the owning thread, crashes otherwise.
   */
  auto garbage_collect() noexcept -> void;

  /**
     Runs external garbage collection.

     This can be called from a different thread. Cannot delete the head of the
     promise list, calling this will therefore result in at least one
     ready-for-deletion promise.
   */
  auto garbage_collect_external() noexcept -> void;

  Thread const thread;

 private:
  Registry* registry = nullptr;
  std::atomic<Promise*> free_head = nullptr;
  std::atomic<Promise*> promise_head = nullptr;
  std::mutex mutex;
  std::shared_ptr<const Metrics> metrics;

  ThreadRegistry(std::shared_ptr<const Metrics> metrics, Registry* registry);
  auto cleanup() noexcept -> void;

  /**
     Removes the promise from the registry.

     Caller needs to make sure that the given promise is part of this registry
     (which also means that this function should only be called on the owning
     thread.)
   */
  auto remove(Promise* promise) -> void;
};

}  // namespace arangodb::async_registry
