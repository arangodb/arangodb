#pragma once

#include "thread_registry.h"

namespace arangodb::coroutine {

/**
   Registry of all active coroutines.

   Includes a list of coroutine thread registries, one for each initialized
   thread.
 */
struct Registry {
  /**
     Initializes the coroutine thread registry of the current thread and adds it
     to this registry.

     Each thread needs to call this once before executing any async<T>
     coroutine.
   */
  auto initialize_current_thread() -> void {
    auto guard = std::lock_guard(mutex);
    registries.push_back(std::make_shared<ThreadRegistry>());
    thread_registry = registries.back().get();
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

  std::vector<std::shared_ptr<ThreadRegistry>> registries;
  std::mutex mutex;
};

}  // namespace arangodb::coroutine
