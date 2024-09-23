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
#include "registry.h"

#include "Assertions/ProdAssert.h"
#include "Async/Registry/Metrics.h"

using namespace arangodb::async_registry;

Registry::Registry() : _metrics{std::make_shared<Metrics>()} {}

auto Registry::add_thread() -> std::shared_ptr<ThreadRegistry> {
  auto guard = std::lock_guard(mutex);
  auto registry = registries.emplace_back(ThreadRegistry::make(_metrics));
  if (_metrics->registered_threads != nullptr) {
    _metrics->registered_threads->fetch_add(1);
  }
  return registry;
}

auto Registry::remove_thread(std::shared_ptr<ThreadRegistry> registry) -> void {
  auto guard = std::lock_guard(mutex);
  if (_metrics->registered_threads != nullptr) {
    _metrics->registered_threads->fetch_sub(1);
  }
  std::erase(registries, registry);
}
