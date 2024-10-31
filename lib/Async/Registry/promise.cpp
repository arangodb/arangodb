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

#include "Async/Registry/registry_variable.h"
#include "Async/Registry/thread_registry.h"

using namespace arangodb::async_registry;

Promise::Promise(Promise* next, std::shared_ptr<ThreadRegistry> registry,
                 std::source_location entry_point)
    : thread{registry->thread},
      source_location{entry_point.file_name(), entry_point.function_name(),
                      entry_point.line()},
      registry{std::move(registry)},
      next{next} {}

auto Promise::mark_for_deletion() noexcept -> void {
  registry->mark_for_deletion(this);
}

AddToAsyncRegistry::AddToAsyncRegistry(std::source_location loc)
    : promise_in_registry{get_thread_registry().add_promise(std::move(loc))} {}
AddToAsyncRegistry::~AddToAsyncRegistry() {
  if (promise_in_registry != nullptr) {
    promise_in_registry->mark_for_deletion();
  }
}
