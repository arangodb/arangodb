#include "registry.h"

#include "Assertions/ProdAssert.h"
#include "Async/Registry/Metrics.h"

using namespace arangodb::async_registry;

Registry::Registry() : _metrics{std::make_shared<Metrics>()} {}

auto Registry::add_thread() -> std::shared_ptr<ThreadRegistry> {
  auto guard = std::lock_guard(mutex);
  registries.push_back(ThreadRegistry::make(_metrics));
  if (_metrics->registered_threads != nullptr) {
    _metrics->registered_threads->fetch_add(1);
  }
  return registries.back();
}

auto Registry::remove_thread(std::shared_ptr<ThreadRegistry> registry) -> void {
  auto guard = std::lock_guard(mutex);
  if (_metrics->registered_threads != nullptr) {
    _metrics->registered_threads->fetch_sub(1);
  }
  std::erase(registries, registry);
}
