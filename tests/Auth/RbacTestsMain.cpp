////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

// Entry point for the standalone RBAC test binary. These tests mock the RBAC
// Service/Backend and never spin up a server, so almost no global ArangoDB
// setup is required. The one exception is the async registry: constructing a
// coroutine future (the mock backend uses `co_return`) registers a promise
// with the async registry, whose `get_metrics()` spins until metrics have been
// set once. Initialise it (to nullptr) exactly as tests/main.cpp does.

#include "gtest/gtest.h"

#include "Activities/RegistryGlobalVariable.h"
#include "Async/Registry/registry_variable.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  arangodb::async_registry::registry.set_metrics(nullptr);
  arangodb::activities::registry.setMetrics(nullptr);

  return RUN_ALL_TESTS();
}
