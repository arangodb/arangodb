////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/Conductor/State.h"
#include "Pregel/PregelOptions.h"

#include <gtest/gtest.h>

using namespace arangodb::pregel;
using namespace arangodb::pregel::conductor;

struct LookupInfoMock : conductor::LookupInfo {
  ~LookupInfoMock() = default;
};

class ConductorStateTest : public ::testing::Test {
 protected:
  LookupInfoMock _lookupInfoMock{};

  ConductorStateTest() {}

  auto createLookupInfoMock() -> std::unique_ptr<conductor::LookupInfo> {
    return std::make_unique<conductor::LookupInfo>(_lookupInfoMock);
  }
};

TEST_F(ConductorStateTest, must_always_be_initialized_with_initial_state) {
  auto dummy = ExecutionSpecifications();
  auto cState = ConductorState(dummy, createLookupInfoMock());
  ASSERT_EQ(cState._executionState->name(), "initial");
}

/*
 * 0.) Create ConductorState (without vocbase) (Resolver struct ||
 * ClusterResolverTest || SingleServerResolverTest) 1.) Possibility to create
 * (initial state) 2.) Create loading state (loading state requires vocbase)
 * Create ConductorActor
 * => Handle Messages (create worker, simulate behaviour, etc.)
 *
 */
