////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

// Must be included early to avoid
//   "explicit specialization of 'std::char_traits<unsigned char>' after
//   instantiation"
// errors.
#include "../3rdParty/iresearch/core/utils/string.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"
#include "Replication2/Mocks/ReplicatedStateMetricsMock.h"
#include "Replication2/Mocks/FakeReplicatedState.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/ParticipantsFactoryMock.h"
#include "Replication2/Mocks/MockVocbase.h"
#include "Replication2/Mocks/StorageEngineMethodsMock.h"
#include "Mocks/Servers.h"

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedState.tpp"
#include "Replication2/IScheduler.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

namespace {
struct FakeState {
  using LeaderType = test::EmptyLeaderType<FakeState>;
  using FollowerType = test::EmptyFollowerType<FakeState>;
  using EntryType = test::DefaultEntryType;
  using FactoryType = test::DefaultFactory<LeaderType, FollowerType>;
  using CoreType = test::TestCoreType;
  using CoreParameterType = void;
  using CleanupHandlerType = void;
};
}  // namespace

struct FakeScheduler : IScheduler {
  auto delayedFuture(std::chrono::steady_clock::duration delay,
                     [[maybe_unused]] std::string_view name)
      -> futures::Future<futures::Unit> override {
    auto p = futures::Promise<futures::Unit>{};
    auto f = p.getFuture();

    auto thread = std::jthread(
        [delay, p = std::move(p), name = std::string(name)]() mutable noexcept {
          // for debugging
          __attribute__((used)) thread_local std::string const _name =
              std::move(name);
          std::this_thread::sleep_for(delay);
          p.setValue();
        });
    thread.detach();

    return f;
  }

  auto queueDelayed(std::string_view name,
                    std::chrono::steady_clock::duration delay,
                    fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle override {
    auto thread =
        std::jthread([delay, handler = std::move(handler)]() mutable noexcept {
          std::this_thread::sleep_for(delay);
          std::move(handler)(false);
        });
    thread.detach();
    return nullptr;
  }

  void queue(fu2::unique_function<void()> cb) noexcept override {
    auto thread = std::jthread(
        [cb = std::move(cb)]() mutable noexcept { std::move(cb)(); });
    thread.detach();
  }
};

struct DelayedScheduler : IScheduler {
  auto delayedFuture(std::chrono::nanoseconds duration, std::string_view name)
      -> arangodb::futures::Future<arangodb::futures::Unit> override {
    ADB_PROD_CRASH() << "not implemented";
  }
  auto queueDelayed(std::string_view name, std::chrono::nanoseconds delay,
                    fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle override {
    // just queue immediately, ignore the delay
    queue([handler = std::move(handler)]() mutable noexcept {
      std::move(handler)(false);
    });
    return nullptr;
  }
  void queue(fu2::unique_function<void()> function) noexcept override {
    _queue.push_back(std::move(function));
  }

  void runOnce() noexcept {
    auto f = std::invoke([this] {
      ADB_PROD_ASSERT(not _queue.empty());
      auto f = std::move(_queue.front());
      _queue.pop_front();
      return f;
    });
    f.operator()();
  }

  void runAll() noexcept {
    while (not _queue.empty()) {
      runOnce();
    }
  }

  auto hasWork() const noexcept -> bool { return not _queue.empty(); }

  ~DelayedScheduler() {
    EXPECT_TRUE(_queue.empty())
        << "Unresolved item(s) in the DelayedScheduler queue";
  }

 private:
  std::deque<fu2::unique_function<void()>> _queue;
};

struct FakeFollowerFactory
    : replication2::replicated_log::IAbstractFollowerFactory {
  FakeFollowerFactory(TRI_vocbase_t& vocbase, replication2::LogId id)
      : vocbase(vocbase), id(id) {}
  auto constructFollower(const ParticipantId& participantId) -> std::shared_ptr<
      replication2::replicated_log::AbstractFollower> override {
    return nullptr;
  }

  auto constructLeaderCommunicator(ParticipantId const& participantId)
      -> std::shared_ptr<replicated_log::ILeaderCommunicator> override {
    return nullptr;
  }

  TRI_vocbase_t& vocbase;
  replication2::LogId id;
};

struct StateManagerTest : testing::Test {
  GlobalLogIdentifier gid = GlobalLogIdentifier("db", LogId{1});
  arangodb::tests::mocks::MockServer mockServer =
      arangodb::tests::mocks::MockServer();
  replication2::tests::MockVocbase vocbaseMock =
      replication2::tests::MockVocbase(mockServer.server(),
                                       "documentStateMachineTestDb", 2);
  std::shared_ptr<test::DelayedExecutor> executor =
      std::make_shared<test::DelayedExecutor>();
  std::shared_ptr<test::FakeStorageEngineMethodsContext> storageContext =
      std::make_shared<test::FakeStorageEngineMethodsContext>(
          12, gid.id, executor, LogRange{},
          replicated_state::PersistedStateInfo{
              .stateId = gid.id,
              .snapshot = {.status = SnapshotStatus::kCompleted,
                           .timestamp = {},
                           .error = {}},
              .generation = {},
              .specification = {}});
  replicated_state::IStorageEngineMethods* methodsPtr =
      storageContext->getMethods().release();
  std::shared_ptr<test::ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<test::ReplicatedLogMetricsMock>();
  std::shared_ptr<test::ReplicatedStateMetricsMock> stateMetricsMock =
      std::make_shared<test::ReplicatedStateMetricsMock>("foo");
  std::shared_ptr<ReplicatedLogGlobalSettings> optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
  LoggerContext loggerContext = LoggerContext(Logger::REPLICATION2);
  agency::ServerInstanceReference const myself = {"SELF", RebootId{1}};
  agency::ServerInstanceReference const other = {"OTHER", RebootId{1}};

  std::shared_ptr<DelayedScheduler> scheduler =
      std::make_shared<DelayedScheduler>();
  std::shared_ptr<FakeFollowerFactory> fakeFollowerFactory =
      std::make_shared<FakeFollowerFactory>(vocbaseMock, gid.id);
  std::shared_ptr<DefaultParticipantsFactory> participantsFactory =
      std::make_shared<replicated_log::DefaultParticipantsFactory>(
          fakeFollowerFactory, scheduler);

  std::shared_ptr<ReplicatedLog> log =
      std::make_shared<replicated_log::ReplicatedLog>(
          std::unique_ptr<replicated_state::IStorageEngineMethods>{methodsPtr},
          logMetricsMock, optionsMock, participantsFactory, loggerContext,
          myself);

  std::shared_ptr<FakeState::FactoryType> stateFactory =
      std::make_shared<FakeState::FactoryType>();
  std::unique_ptr<FakeState::CoreType> stateCore =
      std::make_unique<FakeState::CoreType>();
  LoggerContext const loggerCtx{Logger::REPLICATED_STATE};
  std::shared_ptr<ReplicatedState<FakeState>> state =
      std::make_shared<ReplicatedState<FakeState>>(
          gid, log, stateFactory, loggerCtx, stateMetricsMock, scheduler);
  ReplicatedLogConnection connection =
      log->connect(state->createStateHandle(vocbaseMock, std::nullopt));
};

TEST_F(StateManagerTest, get_leader_state_machine_early) {
  // Overview:
  // - establish leadership
  // - check leader state machine: it should still be nullptr, but the leader
  //   status should be available at this point
  // - let recovery finish
  // - check leader state machine again: should now be available

  auto const term = agency::LogPlanTermSpecification{LogTerm{1}, myself};
  auto const config = agency::ParticipantsConfig{
      1, agency::ParticipantsFlagsMap{{myself.serverId, {}}},
      agency::LogPlanConfig{}};
  log->updateConfig(term, config, myself);
  {
    auto logStatus = log->getQuickStatus();
    ASSERT_EQ(logStatus.role, ParticipantRole::kLeader);
    ASSERT_EQ(logStatus.localState, LocalStateMachineStatus::kUnconfigured);
  }
  auto const leader =
      std::dynamic_pointer_cast<ILogLeader>(log->getParticipant());
  ASSERT_NE(leader, nullptr);
  // Note that we have to check the (quick) status, rather than using
  // `waitForLeadership()`, because the futures are resolved asynchronously.
  // This means recovery might also already be completed at that point, but we
  // want to do some checks while leadership has been established, but before
  // recovery has completed.
  EXPECT_FALSE(log->getQuickStatus().leadershipEstablished);
  EXPECT_TRUE(executor->hasWork() or scheduler->hasWork());
  bool runAtLeastOnce = false;
  while (not log->getQuickStatus().leadershipEstablished and
         (executor->hasWork() or scheduler->hasWork())) {
    runAtLeastOnce = true;
    // while leadership isn't established yet, the leader state manager isn't
    // instantiated yet, so it can't return a status.
    auto stateStatus = state->getStatus();
    ASSERT_TRUE(stateStatus.has_value());
    EXPECT_EQ(stateStatus->asLeaderStatus(), nullptr);
    // the state machine must not be available until after recovery
    auto stateMachine = state->getLeader();
    EXPECT_EQ(stateMachine, nullptr);

    if (scheduler->hasWork()) {
      scheduler->runOnce();
    } else {
      executor->runOnce();
    }
  }
  EXPECT_TRUE(runAtLeastOnce);

  // leadership was established, but recovery hasn't been completed. That means
  // the status should be available (as a leader status), but the state machine
  // must still be inaccessible.
  EXPECT_EQ(log->getQuickStatus().localState,
            LocalStateMachineStatus::kRecovery);
  auto stateStatus = state->getStatus();
  ASSERT_TRUE(stateStatus.has_value());
  ASSERT_NE(stateStatus->asLeaderStatus(), nullptr);
  EXPECT_EQ(state->getLeader(), nullptr);
  scheduler->runOnce();
  EXPECT_EQ(log->getQuickStatus().localState,
            LocalStateMachineStatus::kOperational);
  EXPECT_NE(state->getLeader(), nullptr);
}

TEST_F(StateManagerTest, get_follower_state_machine_early) {
  auto const term = LogTerm{1};
  auto const planTerm = agency::LogPlanTermSpecification{term, other};
  auto const config = agency::ParticipantsConfig{
      1, agency::ParticipantsFlagsMap{{myself.serverId, {}}},
      agency::LogPlanConfig{}};
  log->updateConfig(planTerm, config, myself);
  auto const status = log->getQuickStatus();
  ASSERT_EQ(status.role, ParticipantRole::kFollower);

  auto const stateMachine = state->getFollower();
  EXPECT_EQ(stateMachine, nullptr);

  auto const follower =
      std::dynamic_pointer_cast<ILogFollower>(log->getParticipant());
  ASSERT_NE(follower, nullptr);
  auto const leaderId = other.serverId;
  // send an append entries, only with the leader-establishling log entry.
  // TODO try to access the follower state machine in between, and assert that
  //      it's inaccessible
  auto appendEntriesFuture = std::invoke([&] {
    auto const waitForSync = true;
    auto meta = LogMetaPayload::FirstEntryOfTerm{.leader = leaderId,
                                                 .participants = config};
    auto payload = LogMetaPayload{std::move(meta)};
    auto const termIndexPair = TermIndexPair(term, LogIndex(1));
    auto logEntry = InMemoryLogEntry(
        PersistingLogEntry(termIndexPair, std::move(payload)), waitForSync);
    auto request = AppendEntriesRequest(
        term, leaderId, TermIndexPair(LogTerm(0), LogIndex(0)), LogIndex(0),
        LogIndex(0), MessageId(1), waitForSync,
        AppendEntriesRequest::EntryContainer({std::move(logEntry)}));
    return follower->appendEntries(request);
  });
  EXPECT_FALSE(appendEntriesFuture.isReady());

  executor->runOnce();
  EXPECT_FALSE(executor->hasWork());
  EXPECT_FALSE(scheduler->hasWork());

  ASSERT_TRUE(appendEntriesFuture.isReady());
  ASSERT_TRUE(appendEntriesFuture.hasValue());
  auto const appendEntriesResponse = appendEntriesFuture.get();
  EXPECT_TRUE(appendEntriesResponse.isSuccess())
      << appendEntriesResponse.errorCode;
  EXPECT_TRUE(appendEntriesResponse.snapshotAvailable);
  // TODO:
  //  - either:
  //    - start with a valid snapshot, then
  //      - send a truncating append entries (invalidating the snapshot)
  //    - or start with an invalid snapshot
  //    - or start with a valid snapshot and don't truncate it
  //  - in either order:
  //    - send a (successful) append entries
  //    - finish acquiring the snapshot (if one was started)
  //  At every point, check that the stateMachine is not available until the
  //  very end.
  ASSERT_TRUE(false) << "Finish writing the test!";
}
