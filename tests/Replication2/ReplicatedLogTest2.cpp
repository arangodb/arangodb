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

#include <Replication2/ReplicatedLog.h>

#include "MockLog.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct ReplicatedLogTest : ::testing::Test {
  auto makeLogCore(LogId id) -> std::unique_ptr<LogCore> {
    auto persisted = std::make_shared<MockLog>(id);
    _persistedLogs[id] = persisted;
    return std::make_unique<LogCore>(persisted);
  }

  auto getPersistedLogById(LogId id) -> std::shared_ptr<MockLog> {
    return _persistedLogs.at(id);
  }

  std::unordered_map<LogId, std::shared_ptr<MockLog>> _persistedLogs;
};

// TODO remove this once the LogFollower implements the OldLogFollower
struct FakeOldFollower : LogFollower, OldLogFollower {
  using LogFollower::LogFollower;

  auto participantId() const noexcept -> ParticipantId override {
    return LogFollower::getParticipantId();
  }
  auto appendEntries(AppendEntriesRequest request)
      -> arangodb::futures::Future<AppendEntriesResult> override {
    return LogFollower::appendEntries(std::move(request));
  }
};

struct DelayedFollowerLog : FakeOldFollower {
  using FakeOldFollower::FakeOldFollower;
  auto appendEntries(AppendEntriesRequest req)
      -> arangodb::futures::Future<AppendEntriesResult> override {
    auto future = _asyncQueue.doUnderLock([&](auto& queue) {
      return queue.emplace_back(std::make_shared<AsyncRequest>(std::move(req)))
          ->promise.getFuture();
    });
    return std::move(future).thenValue([this](auto&& result) mutable {
      if (!result.has_value()) {
        return futures::Future<AppendEntriesResult>{AppendEntriesResult{false}};
      }
      return LogFollower::appendEntries(std::forward<decltype(result)>(result).value());
    });
  }

  void runAsyncAppendEntries() {
    auto asyncQueue = _asyncQueue.doUnderLock([](auto& _queue) {
      auto queue = std::move(_queue);
      _queue.clear();
      return queue;
    });

    for (auto& p : asyncQueue) {
      p->promise.setValue(std::move(p->request));
    }
  }

  using WaitForAsyncPromise = futures::Promise<std::optional<AppendEntriesRequest>>;

  struct AsyncRequest {
    explicit AsyncRequest(AppendEntriesRequest request)
        : request(std::move(request)) {}
    AppendEntriesRequest request;
    WaitForAsyncPromise promise;
  };
  [[nodiscard]] auto pendingAppendEntries() const
      -> std::deque<std::shared_ptr<AsyncRequest>> {
    return _asyncQueue.copy();
  }
  [[nodiscard]] auto hasPendingAppendEntries() const -> bool {
    return _asyncQueue.doUnderLock(
        [](auto const& queue) { return !queue.empty(); });
  }

 private:
  Guarded<std::deque<std::shared_ptr<AsyncRequest>>> _asyncQueue;
};

TEST_F(ReplicatedLogTest, write_single_entry_to_follower) {
  auto coreA = makeLogCore(LogId{1});
  auto coreB = makeLogCore(LogId{2});

  auto leaderId = ParticipantId{"leader"};
  auto followerId = ParticipantId{"follower"};

  auto follower = std::make_shared<DelayedFollowerLog>(followerId, std::move(coreB),
                                                       LogTerm{1}, leaderId);
  auto leader =
      std::make_shared<LogLeader>(leaderId, std::move(coreA), LogTerm{1},
                                  std::vector<std::shared_ptr<OldLogFollower>>{follower}, 1);

  {
    // Nothing written on the leader
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }
  {
    // Nothing written on the follower
    auto status = std::get<FollowerStatus>(follower->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }

  {
    // Insert first entry on the follower, expect the spearhead to be one
    auto idx = leader->insert(LogPayload{"first entry"});
    {
      auto status = std::get<LeaderStatus>(leader->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{0});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }
    {
      auto status = std::get<FollowerStatus>(follower->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{0});
      EXPECT_EQ(status.local.spearHead, LogIndex{0});
    }
    auto f = leader->waitFor(idx);
    EXPECT_FALSE(f.isReady());

    // Nothing sent to the follower yet, but only after runAsyncStep
    EXPECT_FALSE(follower->hasPendingAppendEntries());
    leader->runAsyncStep();
    EXPECT_TRUE(follower->hasPendingAppendEntries());

    {
      // check the leader log, there should be one entry written
      auto entry = std::optional<LogEntry>{};
      auto followerLog = getPersistedLogById(LogId{1});
      auto iter = followerLog->read(LogIndex{1});

      entry = iter->next();
      EXPECT_TRUE(entry.has_value())
          << "expect one entry in leader log, found nothing";
      if (entry.has_value()) {
        EXPECT_EQ(entry->logIndex(), LogIndex{1});
        EXPECT_EQ(entry->logTerm(), LogTerm{1});
        EXPECT_EQ(entry->logPayload(), LogPayload{"first entry"});
      }

      entry = iter->next();
      EXPECT_FALSE(entry.has_value());
    }

    // Run async step, now the future should be fulfilled
    EXPECT_FALSE(f.isReady());
    follower->runAsyncAppendEntries();
    EXPECT_TRUE(f.isReady());

    {
      // Leader commit index is 1
      auto status = std::get<LeaderStatus>(leader->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{1});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }
    {
      // Follower has spearhead 1 and commitIndex still 0
      auto status = std::get<FollowerStatus>(follower->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{0});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }

    {
      // check the follower log, there should be one entry written
      auto entry = std::optional<LogEntry>{};
      auto followerLog = getPersistedLogById(LogId{2});
      auto iter = followerLog->read(LogIndex{1});

      entry = iter->next();
      ASSERT_TRUE(entry.has_value())
          << "expect one entry in follower log, found nothing";
      EXPECT_EQ(entry->logIndex(), LogIndex{1});
      EXPECT_EQ(entry->logTerm(), LogTerm{1});
      EXPECT_EQ(entry->logPayload(), LogPayload{"first entry"});

      entry = iter->next();
      EXPECT_FALSE(entry.has_value());
    }

    {
      // Expect the quorum to consist of the follower only
      auto quorum = f.get();
      EXPECT_EQ(quorum->index, LogIndex{1});
      EXPECT_EQ(quorum->term, LogTerm{1});
      EXPECT_EQ(quorum->quorum, std::vector<ParticipantId>{followerId});
    }

    // Follower should have pending append entries
    // containing the commitIndex update
    EXPECT_TRUE(follower->hasPendingAppendEntries());
    follower->runAsyncAppendEntries();

    {
      // Follower has commitIndex 1
      auto status = std::get<FollowerStatus>(follower->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{1});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }

    EXPECT_FALSE(follower->hasPendingAppendEntries());
  }
}


TEST_F(ReplicatedLogTest, wake_up_as_leader_with_persistent_data) {
  auto coreA = makeLogCore(LogId{1});
  auto coreB = makeLogCore(LogId{2});

  {
    auto leaderLog = getPersistedLogById(LogId{1});
    leaderLog->setEntry(LogIndex{1}, LogTerm{1}, LogPayload{"first entry"});
    leaderLog->setEntry(LogIndex{2}, LogTerm{1}, LogPayload{"second entry"});
    leaderLog->setEntry(LogIndex{3}, LogTerm{2}, LogPayload{"third entry"});
  }

  auto leaderId = ParticipantId{"leader"};
  auto followerId = ParticipantId{"follower"};

  auto follower = std::make_shared<DelayedFollowerLog>(followerId, std::move(coreB),
                                                       LogTerm{3}, leaderId);
  auto leader =
      std::make_shared<LogLeader>(leaderId, std::move(coreA), LogTerm{3},
                                  std::vector<std::shared_ptr<OldLogFollower>>{follower}, 1);

  {
    // Leader should know it spearhead, but commitIndex is 0
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{3});
  }
  {
    // Nothing written on the follower
    auto status = std::get<FollowerStatus>(follower->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }

  // Nothing should be ready
  auto f = leader->waitFor(LogIndex{3});
  EXPECT_FALSE(f.isReady());

  // this should trigger a sendAppendEntries to all follower
  EXPECT_FALSE(follower->hasPendingAppendEntries());
  leader->runAsyncStep();
  EXPECT_TRUE(follower->hasPendingAppendEntries());


}
