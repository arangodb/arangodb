#include "thread_registry.h"
#include "feature.h"

namespace arangodb::coroutine {

auto get_thread_registry() noexcept -> ThreadRegistry& {
  struct ThreadRegistryGuard {
    ThreadRegistryGuard() : _registry{coroutine_registry.add_thread()} {}
    ~ThreadRegistryGuard() { coroutine_registry.remove_thread(_registry); }

    ThreadRegistry* _registry;
  };
  static thread_local auto registry_guard = ThreadRegistryGuard{};
  return *registry_guard._registry;
}

}  // namespace arangodb::coroutine
