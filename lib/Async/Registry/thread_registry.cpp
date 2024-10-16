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
#include "thread_registry.h"
#include <source_location>
#include <thread>

#include "Assertions/ProdAssert.h"
#include "Async/Registry/Metrics.h"
#include "Async/Registry/promise.h"
#include "Async/Registry/registry.h"
#include "Basics/Thread.h"
#include "Inspection/Format.h"
#include "Logger/LogMacros.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"

using namespace arangodb::async_registry;

auto ThreadRegistry::make(std::shared_ptr<const Metrics> metrics,
                          Registry* registry)
    -> std::shared_ptr<ThreadRegistry> {
  struct MakeSharedThreadRegistry : ThreadRegistry {
    MakeSharedThreadRegistry(std::shared_ptr<const Metrics> metrics,
                             Registry* registry)
        : ThreadRegistry{metrics, registry} {}
  };
  return std::make_shared<MakeSharedThreadRegistry>(metrics, registry);
}

ThreadRegistry::ThreadRegistry(std::shared_ptr<const Metrics> metrics,
                               Registry* registry)
    : thread{Thread{.name = std::string{ThreadNameFetcher{}.get()},
                    .id = std::this_thread::get_id()}},
      registry{registry},
      metrics{metrics} {
  if (metrics->total_threads != nullptr) {
    metrics->total_threads->count();
  }
  if (metrics->running_threads != nullptr) {
    metrics->running_threads->fetch_add(1);
  }
}

ThreadRegistry::~ThreadRegistry() noexcept {
  if (metrics->running_threads != nullptr) {
    metrics->running_threads->fetch_sub(1);
  }
  if (registry != nullptr) {
    registry->remove_thread(this);
  }
  cleanup();
}

auto ThreadRegistry::add_promise(std::source_location location) noexcept
    -> Promise* {
  // promise needs to live on the same thread as this registry
  ADB_PROD_ASSERT(std::this_thread::get_id() == thread.id)
      << "ThreadRegistry::add was called from thread "
      << std::this_thread::get_id()
      << " but needs to be called from ThreadRegistry's owning thread "
      << thread.id << ". " << this;
  if (metrics->total_functions != nullptr) {
    metrics->total_functions->count();
  }
  auto current_head = promise_head.load(std::memory_order_relaxed);
  auto promise =
      new Promise{current_head, shared_from_this(), std::move(location)};
  if (current_head != nullptr) {
    current_head->previous = promise;
  }
  // (1) - this store synchronizes with load in (2)
  promise_head.store(promise, std::memory_order_release);
  if (metrics->active_functions != nullptr) {
    metrics->active_functions->fetch_add(1);
  }
  return promise;
}

auto ThreadRegistry::mark_for_deletion(Promise* promise) noexcept -> void {
  // makes sure that promise is really in this list
  ADB_PROD_ASSERT(promise->registry.get() == this);
  // keep a local copy of the shared pointer. This promise might be the
  // last of the registry.
  auto self = std::move(promise->registry);

  auto current_head = free_head.load(std::memory_order_relaxed);
  do {
    promise->next_to_free = current_head;
    // (4) - this compare_exchange_weak synchronizes with exchange in (5)
  } while (not free_head.compare_exchange_weak(current_head, promise,
                                               std::memory_order_release,
                                               std::memory_order_acquire));
  // DO NOT access promise after this line. The owner thread might already
  // be running a cleanup and promise might be deleted.

  if (metrics->active_functions != nullptr) {
    metrics->active_functions->fetch_sub(1);
  }
  if (metrics->ready_for_deletion_functions != nullptr) {
    metrics->ready_for_deletion_functions->fetch_add(1);
  }

  // self destroyed here. registry might be destroyed here as well.
}

auto ThreadRegistry::garbage_collect() noexcept -> void {
  ADB_PROD_ASSERT(std::this_thread::get_id() == thread.id)
      << "ThreadRegistry::garbage_collect was called from thread "
      << std::this_thread::get_id()
      << " but needs to be called from ThreadRegistry's owning thread "
      << thread.id << ". " << this;
  auto guard = std::lock_guard(mutex);
  cleanup();
}

auto ThreadRegistry::cleanup() noexcept -> void {
  // (5) - this exchange synchronizes with compare_exchange_weak in (4)
  Promise *current,
      *next = free_head.exchange(nullptr, std::memory_order_acquire);
  while (next != nullptr) {
    current = next;
    next = next->next_to_free;
    if (metrics->ready_for_deletion_functions != nullptr) {
      metrics->ready_for_deletion_functions->fetch_sub(1);
    }
    remove(current);
    delete current;
  }
}

auto ThreadRegistry::garbage_collect_external() noexcept -> void {
  // acquire the lock. This prevents the owning thread and the observer
  // from accessing promises. Note that the owing thread only adds new
  // promises to the head of the list.
  auto guard = std::lock_guard(mutex);
  // we can make the following observation. Once a promise is enqueued in the
  // list, it previous and next pointer is never updated, except for the current
  // head element. Also, Promises are only removed, after the mutex has been
  // acquired. This implies that we can clean up all promises, that are not
  // in head position right now.
  Promise* maybe_head_ptr = nullptr;
  Promise *current,
      *next = free_head.exchange(nullptr, std::memory_order_acquire);
  while (next != nullptr) {
    current = next;
    next = next->next_to_free;
    if (current->previous != nullptr) {
      if (metrics->ready_for_deletion_functions != nullptr) {
        metrics->ready_for_deletion_functions->fetch_sub(1);
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
    auto current_head = free_head.load(std::memory_order_relaxed);
    do {
      maybe_head_ptr->next_to_free = current_head;
      // (4) - this compare_exchange_weak synchronizes with exchange in (5)
    } while (not free_head.compare_exchange_weak(current_head, maybe_head_ptr,
                                                 std::memory_order_release,
                                                 std::memory_order_acquire));
  }
}

auto ThreadRegistry::remove(Promise* promise) -> void {
  auto* next = promise->next;
  auto* previous = promise->previous.load();
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
