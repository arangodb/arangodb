#pragma once

#include "Async/Registry/Metrics.h"
#include "thread_registry.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace arangodb::async_registry {

/**
   Registry of all active coroutines.

   Includes a list of coroutine thread registries, one for each initialized
   thread.
 */
struct Registry {
  /**
     Creates a new coroutine thread registry and adds it to this registry.

     Each thread needs to call this once to be able to add promises to the
     coroutine registry.
   */
  auto add_thread() -> std::shared_ptr<ThreadRegistry> {
    auto guard = std::lock_guard(mutex);
    registries.push_back(std::make_shared<ThreadRegistry>(_metrics));
    return registries.back();
  }

  /**
     Removes a coroutine thread registry from this registry.
   */
  auto remove_thread(std::shared_ptr<ThreadRegistry> registry) -> void {
    auto guard = std::lock_guard(mutex);
    std::erase(registries, registry);
  }

  /**
     Executes a function on each coroutine in the registry.

     Can be called from any thread. It makes sure that all
     items stay valid during iteration (i.e. are not deleted in the meantime).
   */
  template<typename F>
  requires requires(F f, PromiseInList* promise) { {f(promise)}; }
  auto for_promise(F&& function) -> void {
    auto regs = [&] {
      auto guard = std::lock_guard(mutex);
      return registries;
    }();

    for (auto& registry : regs) {
      registry->for_promise(function);
    }
  }

  /**
     Exchange metrics.

     New and existing threads will use this new metrics objects.
   */
  auto set_metrics(std::shared_ptr<const Metrics> metrics) -> void {
    _metrics = metrics;
  }

 private:
  std::vector<std::shared_ptr<ThreadRegistry>> registries;
  std::mutex mutex;
  std::shared_ptr<const Metrics> _metrics = nullptr;
};

}  // namespace arangodb::async_registry
