#include "registry_variable.h"

namespace arangodb::async_registry {

Registry coroutine_registry;

auto get_thread_registry() noexcept -> ThreadRegistry& {
  struct ThreadRegistryGuard {
    ThreadRegistryGuard() : _registry{coroutine_registry.add_thread()} {}
    ~ThreadRegistryGuard() {
      coroutine_registry.remove_thread(std::move(_registry));
    }

    std::shared_ptr<ThreadRegistry> _registry;
  };
  static thread_local auto registry_guard = ThreadRegistryGuard{};
  return *registry_guard._registry;
}

}  // namespace arangodb::async_registry
