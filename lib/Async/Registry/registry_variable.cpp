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

#include "Async/Registry/promise.h"

#include <thread>

namespace arangodb::async_registry {

containers::ListOfLists<containers::ThreadOwnedList<Promise>> registry;

/**
   Gives the thread registry of the current thread.

   Creates the thread registry when called for the first time.
 */
auto get_thread_registry() noexcept -> containers::ThreadOwnedList<Promise>& {
  struct ThreadRegistryGuard {
    ThreadRegistryGuard()
        : _registry{containers::ThreadOwnedList<Promise>::make()} {
      registry.add(_registry);
    }

    std::shared_ptr<containers::ThreadOwnedList<Promise>> _registry;
  };
  static thread_local auto registry_guard = ThreadRegistryGuard{};
  return *registry_guard._registry;
}

/**
   Gives a ptr to the currently running coroutine on the thread or to the
   current thread if no coroutine is running at the moment.
 */
auto get_current_coroutine() noexcept -> Requester* {
  struct Guard {
    // initialized with current thread
    Guard() : _identifier{Requester::current_thread()} {}

    Requester _identifier;
  };
  static thread_local auto guard = Guard{};
  return &guard._identifier;
}

}  // namespace arangodb::async_registry
