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

#include <gtest/gtest.h>

#include "Activities/RegistryGlobalVariable.h"
#include "Async/Registry/registry_variable.h"
#include "Basics/VelocyPackHelper.h"

// Entry point for the standalone storage-engine test binary.
//
// The engine's transaction machinery creates coroutine promises that register
// with the process-wide async/activities registries. Those registries
// spin-wait in get_metrics() until their metrics have been configured -- which
// normally happens once during server startup. Configure them here so the
// tests do not spin forever. The combined arangodbtests binary performs the
// same initialisation in tests/main.cpp.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  // Sets up the VelocyPack attribute translator (compact encoding of the
  // system attributes _key/_id/_rev/_from/_to) in VPackOptions::Defaults. The
  // document storage path depends on it. Normally done via ArangoGlobalContext
  // at startup.
  arangodb::basics::VelocyPackHelper::initialize();

  arangodb::async_registry::registry.set_metrics(nullptr);
  arangodb::activities::registry.setMetrics(nullptr);

  return RUN_ALL_TESTS();
}
