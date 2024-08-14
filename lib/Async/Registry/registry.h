#pragma once

#include "thread_registry.h"

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
  auto add_thread() -> ThreadRegistry* {
    auto guard = std::lock_guard(mutex);
    registries.push_back(ThreadRegistry::make());
    auto registry = registries.back();
    registry->increment_ref_count();
    return registry;
  }

  /**
     Removes a coroutine thread registry from this registry.
   */
  auto remove_thread(ThreadRegistry* registry) -> void {
    auto guard = std::lock_guard(mutex);
    std::erase(registries, registry);
    registry->decrement_ref_count();
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

 private:
  std::vector<ThreadRegistry*> registries;
  std::mutex mutex;
};

}  // namespace arangodb::async_registry
