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

#include <utility>

#include "Logger/LogMacros.h"
#include "LogLevels.h"

#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/PersistedLog.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

struct FollowerSnapshotTest
    : testing::Test,
      tests::LogSuppressor<Logger::REPLICATED_STATE,
                                     LogLevel::DEBUG> {
  struct State {
    using LeaderType = test::EmptyLeaderType<State>;
    using FollowerType = test::FakeFollowerType<State>;
    using EntryType = test::DefaultEntryType;
    using FactoryType = test::DefaultFactory<LeaderType, FollowerType>;
  };

  using RepState = ReplicatedState<State>;

  FollowerSnapshotTest() { feature->registerStateType<State>("my-state"); }

  auto makeReplicatedLog() -> std::shared_ptr<replicated_log::ReplicatedLog> {
    auto persisted = std::make_shared<test::MockLog>(LogId{1});
    auto core = std::make_unique<replicated_log::LogCore>(persisted);
    return std::make_shared<replicated_log::ReplicatedLog>(
        std::move(core), _logMetricsMock, _optionsMock,
        LoggerContext(Logger::REPLICATION2));
  }

  auto getInputStreamForFollower(
      std::shared_ptr<replicated_log::ILogFollower> f)
      -> std::shared_ptr<streams::ProducerStream<State::EntryType>> {
    auto term = f->getTerm();
    TRI_ASSERT(term.has_value());
    auto leaderId = f->getLeader();
    TRI_ASSERT(leaderId.has_value());
    auto log = makeReplicatedLog();
    auto leader = log->becomeLeader(
        LogConfig(2, 2, 2, false), *leaderId, *term,
        std::vector<std::shared_ptr<replicated_log::AbstractFollower>>{f});
    leader->triggerAsyncReplication();
    auto mux =
        streams::LogMultiplexer<ReplicatedStateStreamSpec<State>>::construct(
            leader);
    struct Wrapper {
      explicit Wrapper(
          std::shared_ptr<replicated_log::ReplicatedLog> log,
          std::shared_ptr<streams::ProducerStream<State::EntryType>> stream)
          : log(std::move(log)), stream(std::move(stream)) {}
      std::shared_ptr<replicated_log::ReplicatedLog> log;
      std::shared_ptr<streams::ProducerStream<State::EntryType>> stream;
    };

    auto wrapper = std::make_shared<Wrapper>(log, mux->getStreamById<1>());
    return std::shared_ptr<streams::ProducerStream<State::EntryType>>{
        wrapper, wrapper->stream.get()};
  }

  auto makeCore() -> std::unique_ptr<ReplicatedStateCore> {
    return std::make_unique<ReplicatedStateCore>();
  }

  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();

  std::shared_ptr<ReplicatedLogMetricsMock> _logMetricsMock =
      std::make_shared<ReplicatedLogMetricsMock>();
  std::shared_ptr<ReplicatedLogGlobalSettings> _optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
};

#include "Replication2/ReplicatedState/ReplicatedState.tpp"

TEST_F(FollowerSnapshotTest, check_acquire_snapshot) {
  auto log = makeReplicatedLog();
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto state = feature->createReplicatedStateAs<State>("my-state", log);
  state->flush();

  {
    auto status = state->getStatus().asFollowerStatus();
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->state.state, FollowerInternalState::kWaitForLeaderConfirmation);
  }

  // required for leader to become established
  auto producer = getInputStreamForFollower(follower);


  // we expect a snapshot to be requested, because the snapshot state was uninitialized
  {
    auto status = state->getStatus().asFollowerStatus();
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->state.state, FollowerInternalState::kTransferSnapshot);
  }


}
