#pragma once

#include "thread_registry.h"

#include <functional>

namespace arangodb::coroutine {

// the single owner of all promise registries
struct Registry {
  // all threads can call this
  auto add_thread() -> void {
    auto guard = std::lock_guard(mutex);
    registries.push_back(std::make_shared<ThreadRegistry>());
    thread_registry = registries.back().get();
  }

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
