#include "registry.h"

#include "Assertions/ProdAssert.h"
#include "Async/Registry/Metrics.h"

using namespace arangodb::async_registry;

Registry::Registry() : _metrics{std::make_shared<Metrics>()} {}

auto Registry::add_thread() -> std::shared_ptr<ThreadRegistry> {
  auto guard = std::lock_guard(mutex);
  ADB_PROD_ASSERT(_metrics != nullptr);
  registries.push_back(std::make_shared<ThreadRegistry>(_metrics));
  return registries.back();
}

auto Registry::remove_thread(std::shared_ptr<ThreadRegistry> registry) -> void {
  auto guard = std::lock_guard(mutex);
  std::erase(registries, registry);
}
