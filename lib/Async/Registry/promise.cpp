#include "promise.h"

#include "Async/Registry/registry_variable.h"
#include "Async/Registry/thread_registry.h"

using namespace arangodb::async_registry;

Promise::Promise(Promise* next, std::shared_ptr<ThreadRegistry> registry,
                 std::source_location entry_point)
    : thread{registry->thread},
      entry_point{std::move(entry_point)},
      registry{registry},
      next{next} {}

auto Promise::mark_for_deletion() noexcept -> void {
  registry->mark_for_deletion(this);
}

AddToAsyncRegistry::AddToAsyncRegistry(std::source_location loc)
    : promise{get_thread_registry().add_promise(std::move(loc))} {}
AddToAsyncRegistry::~AddToAsyncRegistry() {
  if (promise != nullptr) {
    promise->mark_for_deletion();
  }
}
