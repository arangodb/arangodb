////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "promise.h"
#include <optional>

#include "Async/Registry/registry_variable.h"
#include "Async/Registry/thread_registry.h"
#include "Basics/Thread.h"
#include "Inspection/Format.h"

using namespace arangodb::async_registry;

auto ThreadId::current() noexcept -> ThreadId {
  return ThreadId{.posix_id = arangodb::Thread::currentThreadId(),
                  .kernel_id = arangodb::Thread::currentKernelThreadId()};
}
auto ThreadId::name() -> std::string {
  return std::string{ThreadNameFetcher{posix_id}.get()};
}

auto Requester::current_thread() -> Requester { return {ThreadId::current()}; }

Promise::Promise(Promise* next, std::shared_ptr<ThreadRegistry> registry,
                 Requester requester, std::source_location entry_point)
    : thread{registry->thread},
      source_location{entry_point.file_name(), entry_point.function_name(),
                      entry_point.line()},
      requester{requester},
      registry{std::move(registry)},
      next{next} {}

auto Promise::mark_for_deletion() noexcept -> void {
  registry->mark_for_deletion(this);
}

AddToAsyncRegistry::AddToAsyncRegistry(std::source_location loc)
    : promise_in_registry{get_thread_registry().add_promise(
          *get_current_coroutine(), std::move(loc))} {}
AddToAsyncRegistry::~AddToAsyncRegistry() {
  if (promise_in_registry != nullptr) {
    promise_in_registry->mark_for_deletion();
  }
}
auto AddToAsyncRegistry::update_requester(Requester new_requester) -> void {
  if (promise_in_registry != nullptr) {
    promise_in_registry->requester.store(new_requester);
  }
}
auto AddToAsyncRegistry::id() -> void* {
  if (promise_in_registry != nullptr) {
    return promise_in_registry->id();
  } else {
    return nullptr;
  }
}
auto AddToAsyncRegistry::update_source_location(std::source_location loc)
    -> void {
  if (promise_in_registry != nullptr) {
    promise_in_registry->source_location.line.store(loc.line());
  }
}
auto AddToAsyncRegistry::update_state(State state) -> std::optional<State> {
  if (promise_in_registry != nullptr) {
    return promise_in_registry->state.exchange(state);
  } else {
    return std::nullopt;
  }
}
