////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Mocks/LogLevels.h"
#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/Streams.h"

#include "StateMachines/MyStateMachine.h"

#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateFeatureTest : ReplicatedLogTest {};

TEST_F(ReplicatedStateFeatureTest, create_feature) {
  auto feature = std::make_shared<ReplicatedStateFeature>();
}

TEST_F(ReplicatedStateFeatureTest, register_state_machine) {
  auto feature = std::make_shared<ReplicatedStateFeature>();
  feature->registerStateType<MyState>("my-state");
}

TEST_F(ReplicatedStateFeatureTest, create_state_machine) {
  auto feature = std::make_shared<ReplicatedStateFeature>();
  feature->registerStateType<MyState>("my-state");

  auto log = makeReplicatedLog(LogId{1});
  auto instance = feature->createReplicatedState("my-state", log);
}

TEST_F(ReplicatedStateFeatureTest, create_non_existing_state_machine) {
  auto feature = std::make_shared<ReplicatedStateFeature>();
  feature->registerStateType<MyState>("my-state");

  auto log = makeReplicatedLog(LogId{1});
  ASSERT_THROW({ auto instance = feature->createReplicatedState("FOOBAR", log); },
               basics::Exception);
}

TEST_F(ReplicatedStateFeatureTest, register_duplicated_state_machine_DeathTest) {
  auto feature = std::make_shared<ReplicatedStateFeature>();
  feature->registerStateType<MyState>("my-state");
  ASSERT_DEATH_IF_SUPPORTED({feature->registerStateType<MyState>("my-state");}, ".*");
}
