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
#include "registry_variable.h"

namespace arangodb::async_registry {

Registry registry;

auto get_thread_registry() noexcept -> ThreadRegistry& {
  struct ThreadRegistryGuard {
    ThreadRegistryGuard() : _registry{registry.add_thread()} {}

    /**
       Runs when the current thread is deleted
     */
    ~ThreadRegistryGuard() {}

    std::shared_ptr<ThreadRegistry> _registry;
  };
  static thread_local auto registry_guard = ThreadRegistryGuard{};
  return *registry_guard._registry;
}

}  // namespace arangodb::async_registry
