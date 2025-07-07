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

#include "Containers/Concurrent/ThreadOwnedList.h"
#include "Async/Registry/registry_variable.h"
#include "Containers/Concurrent/thread.h"

using namespace arangodb::async_registry;

Promise::Promise(CurrentRequester requester, std::source_location entry_point)
    : owning_thread{basics::ThreadInfo::current()},
      requester{
          AtomicRequester::from(requester)},  // TODO hand this in via function
      state{State::Running},
      running_thread{basics::ThreadId::current()},
      source_location{entry_point.file_name(), entry_point.function_name(),
                      entry_point.line()} {}

auto arangodb::async_registry::get_current_coroutine() noexcept
    -> CurrentRequester* {
  struct Guard {
    CurrentRequester identifier = {basics::ThreadInfo::current()};
  };
  // make sure that this is only created once on a thread
  static thread_local auto current = Guard{};
  return &current.identifier;
}

AddToAsyncRegistry::AddToAsyncRegistry(std::source_location loc)
    : node_in_registry{get_thread_registry().add([&]() {
        return Promise{*get_current_coroutine(), std::move(loc)};
      })} {}

AddToAsyncRegistry::~AddToAsyncRegistry() {
  if (node_in_registry != nullptr) {
    node_in_registry->list->mark_for_deletion(node_in_registry.get());
  }
}
auto AddToAsyncRegistry::update_requester(std::optional<PromiseId> requester)
    -> void {
  if (node_in_registry != nullptr && requester.has_value()) {
    node_in_registry->data.requester.store(requester.value().id);
  }
}
auto AddToAsyncRegistry::update_requester_to_current_thread() -> void {
  if (node_in_registry != nullptr) {
    node_in_registry->data.requester.store(basics::ThreadInfo::current());
  }
}
auto AddToAsyncRegistry::id() -> std::optional<PromiseId> {
  if (node_in_registry != nullptr) {
    return {node_in_registry->data.id()};
  } else {
    return std::nullopt;
  }
}
auto AddToAsyncRegistry::update_source_location(std::source_location loc)
    -> void {
  if (node_in_registry != nullptr) {
    node_in_registry->data.source_location.line.store(loc.line());
  }
}
auto AddToAsyncRegistry::update_state(State state) -> std::optional<State> {
  if (node_in_registry != nullptr) {
    if (state == State::Running) {
      node_in_registry->data.running_thread.store(basics::ThreadId::current(),
                                                  std::memory_order_release);
    } else {
      node_in_registry->data.running_thread.store(std::nullopt,
                                                  std::memory_order_release);
    }
    return node_in_registry->data.state.exchange(state);
  } else {
    return std::nullopt;
  }
}
