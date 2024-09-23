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

#include "Assertions/ProdAssert.h"
#include "Async/Registry/Metrics.h"
#include "Basics/Thread.h"
#include "Inspection/Format.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/Counter.h"

using namespace arangodb::async_registry;

auto ThreadRegistry::make(std::shared_ptr<const Metrics> metrics)
    -> std::shared_ptr<ThreadRegistry> {
  return std::shared_ptr<ThreadRegistry>(new ThreadRegistry{metrics});
}

ThreadRegistry::ThreadRegistry(std::shared_ptr<const Metrics> metrics)
    : thread_name{ThreadNameFetcher{}.get()},
      running_threads{metrics->running_threads.get(), 1},
      metrics{metrics} {
  if (metrics->total_threads != nullptr) {
    metrics->total_threads->count();
  }
}

auto ThreadRegistry::add(PromiseInList* promise) noexcept -> void {
  ADB_PROD_ASSERT(promise != nullptr);
  // promise needs to live on the same thread as this registry
  ADB_PROD_ASSERT(std::this_thread::get_id() == owning_thread)
      << "ThreadRegistry::add was called from thread "
      << std::this_thread::get_id()
      << " but needs to be called from ThreadRegistry's owning thread "
      << owning_thread << ".";
  if (metrics->total_functions != nullptr) {
    metrics->total_functions->count();
  }
  auto current_head = promise_head.load(std::memory_order_relaxed);
  promise->next = current_head;
  promise->registry = shared_from_this();
  promise->thread_name = thread_name;
  promise->thread_id = owning_thread;
  if (current_head != nullptr) {
    current_head->previous = promise;
  }
  // (1) - this store synchronizes with load in (2)
  promise_head.store(promise, std::memory_order_release);
  if (metrics->active_functions != nullptr) {
    metrics->active_functions->fetch_add(1);
  }
}

auto ThreadRegistry::mark_for_deletion(PromiseInList* promise) noexcept
    -> void {
  // makes sure that promise is really in this list
  ADB_PROD_ASSERT(promise->registry.get() == this);
  auto current_head = free_head.load(std::memory_order_relaxed);
  do {
    promise->next_to_free = current_head;
    // (4) - this compare_exchange_weak synchronizes with exchange in (5)
  } while (not free_head.compare_exchange_weak(current_head, promise,
                                               std::memory_order_release,
                                               std::memory_order_acquire));

  if (metrics->active_functions != nullptr) {
    metrics->active_functions->fetch_sub(1);
  }
  if (metrics->ready_for_deletion_functions != nullptr) {
    metrics->ready_for_deletion_functions->fetch_add(1);
  }
  // decrement the registries ref-count
  promise->registry.reset();
}

auto ThreadRegistry::garbage_collect() noexcept -> void {
  ADB_PROD_ASSERT(std::this_thread::get_id() == owning_thread);
  auto guard = std::lock_guard(mutex);
  cleanup();
}

auto ThreadRegistry::cleanup() noexcept -> void {
  // (5) - this exchange synchronizes with compare_exchange_weak in (4)
  PromiseInList *current,
      *next = free_head.exchange(nullptr, std::memory_order_acquire);
  while (next != nullptr) {
    current = next;
    next = next->next_to_free;
    if (metrics->ready_for_deletion_functions != nullptr) {
      metrics->ready_for_deletion_functions->fetch_sub(1);
    }
    remove(current);
    current->destroy();
  }
}

auto ThreadRegistry::remove(PromiseInList* promise) -> void {
  auto* next = promise->next;
  auto* previous = promise->previous;
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
