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

#include "Replication2/ReplicatedLog/TestHelper.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateImpl.tpp"
#include "Replication2/ReplicatedState/ReplicatedStateMetrics.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/FakeLeader.h"

#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Mocks/MockStatePersistorInterface.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::test;

struct ReplicatedStateCleanupTest : test::ReplicatedLogTest {
  struct CleanupHandler {
    void drop(std::unique_ptr<test::TestCoreType> core) {
      cores.emplace_back(std::move(core));
    }

    std::vector<std::unique_ptr<test::TestCoreType>> cores;
  };

  template<typename LeaderType, typename FollowerType>
  struct Factory : test::RecordingFactory<LeaderType, FollowerType> {
    auto constructCleanupHandler() -> std::shared_ptr<CleanupHandler> {
      return lastCleanupHandler = std::make_shared<CleanupHandler>();
    }

    std::shared_ptr<CleanupHandler> lastCleanupHandler;
  };

  struct State {
    using LeaderType = test::FakeLeaderType<State>;
    using FollowerType = test::EmptyFollowerType<State>;
    using EntryType = test::DefaultEntryType;
    using FactoryType = Factory<LeaderType, FollowerType>;
    using CoreType = test::TestCoreType;
    using CoreParameterType = void;
    using CleanupHandlerType = CleanupHandler;
  };

  std::shared_ptr<State::FactoryType> factory =
      std::make_shared<State::FactoryType>();
  std::shared_ptr<test::MockStatePersistorInterface> _persistor =
      std::make_shared<test::MockStatePersistorInterface>();
  LoggerContext const loggerCtx{Logger::REPLICATED_STATE};
  std::shared_ptr<ReplicatedStateMetrics> _metrics =
      std::make_shared<ReplicatedStateMetricsMock>("foo");
};

TEST_F(ReplicatedStateCleanupTest, complete_run_without_resign) {
  auto log = makeReplicatedLog(LogId{12});
  auto state = std::make_shared<ReplicatedState<State>>(log, factory, loggerCtx,
                                                        _metrics, _persistor);

  auto const stateGeneration = StateGeneration{1};
  state->start(std::make_unique<ReplicatedStateToken>(stateGeneration),
               std::nullopt);

  std::move(*state).drop();
  auto cleanupHandler = factory->lastCleanupHandler;
  ASSERT_NE(cleanupHandler, nullptr);
  EXPECT_EQ(cleanupHandler->cores.size(), 1U);
}
