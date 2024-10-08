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

#include "Async/Registry/thread_registry.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace arangodb::async_registry {

struct Metrics;

/**
   Registry of all active coroutines.

   Includes a list of coroutine thread registries, one for each initialized
   thread.
 */
struct Registry {
  Registry();

  /**
     Creates a new coroutine thread registry and adds it to this registry.

     Each thread needs to call this once to be able to add promises to the
     coroutine registry.
   */
  auto add_thread() -> std::shared_ptr<ThreadRegistry>;

  /**
     Removes a coroutine thread registry from this registry.
   */
  auto remove_thread(ThreadRegistry* registry) -> void;

  /**
     Executes a function on each coroutine in the registry.

     Can be called from any thread. It makes sure that all
     items stay valid during iteration (i.e. are not deleted in the meantime).
   */
  template<typename F>
  requires std::invocable<F, Promise*>
  auto for_promise(F&& function) -> void {
    auto regs = [&] {
      auto guard = std::lock_guard(mutex);
      return registries;
    }();

    for (auto& registry_weak : regs) {
      if (auto registry = registry_weak.lock()) {
        registry->for_promise(function);
      }
    }
  }

  /**
     Exchange metrics.

     New and existing threads will use this new metrics objects.
   */
  auto set_metrics(std::shared_ptr<const Metrics> metrics) -> void {
    _metrics = metrics;
  }

  /**
     Runs an external clean up.
   */
  void run_external_cleanup() noexcept {
    auto regs = [&] {
      auto guard = std::lock_guard(mutex);
      return registries;
    }();

    for (auto& registry_weak : regs) {
      if (auto registry = registry_weak.lock()) {
        registry->garbage_collect_external();
      }
    }
  }

 private:
  std::vector<std::weak_ptr<ThreadRegistry>> registries;
  std::mutex mutex;
  std::shared_ptr<const Metrics> _metrics;
};

}  // namespace arangodb::async_registry
