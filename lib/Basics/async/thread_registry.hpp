#include <functional>
#include "Basics/async/promise_registry.hpp"
#include "Basics/async/feature.hpp"

namespace arangodb::coroutine {

// the single owner of all promise registries
struct ThreadRegistryForPromises {
  // all threads can call this
  auto create() -> void {
    auto guard = std::lock_guard(mutex);
    promise_registry = std::make_shared<coroutine::PromiseRegistry>();
    registries.push_back(promise_registry);
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

  std::vector<std::shared_ptr<PromiseRegistry>> registries;
  std::mutex mutex;
};

}  // namespace arangodb::coroutine
