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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/MockLog.h"
#include "Replication2/TestHelper.h"

#include <Basics/Exceptions.h>
#include <Replication2/ReplicatedLog.h>

#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::replication2;

struct ReplicatedLogTest : ::testing::Test {};
struct ReplicatedLogTest2 : ::testing::Test {


  auto getNextLogId() -> LogId {
    return _nextLogId = LogId{_nextLogId.id() + 1};
  }

  auto addLogInstance(ParticipantId const& id) -> std::shared_ptr<ReplicatedLog> {
    auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
    auto persistedLog = std::make_shared<MockLog>(getNextLogId());
    auto log = std::make_shared<ReplicatedLog>(id, state, persistedLog);
    return log;
  }

  auto addFollowerLogInstance(ParticipantId const& id) -> std::shared_ptr<DelayedFollowerLog> {
    auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
    auto persistedLog = std::make_shared<MockLog>(getNextLogId());
    auto log = std::make_shared<DelayedFollowerLog>(id, state, persistedLog);
    return log;
  }

  LogId _nextLogId{0};
};

TEST_F(ReplicatedLogTest2, stop_follower_and_rejoin) {
  auto leader = addLogInstance("leader");
  auto follower = addFollowerLogInstance("follower");

  {
    // write a single entry on both servers
    leader->becomeLeader(LogTerm{1}, {follower}, 2);
    follower->becomeFollower(LogTerm{1}, leader->participantId());
    auto idx = leader->insert(LogPayload{"first entry"});
    auto f = leader->waitFor(idx);
    leader->runAsyncStep();
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }
    ASSERT_TRUE(f.isReady());
  }

  {
    // leader continues alone
    leader->becomeLeader(LogTerm{2}, {}, 1);
    auto idx = leader->insert(LogPayload{"second entry"});
    auto f = leader->waitFor(idx);
    leader->runAsyncStep();
    ASSERT_FALSE(follower->hasPendingAppendEntries());
    ASSERT_TRUE(f.isReady());
  }

  // check statistics
  {
    auto stats = leader->getLocalStatistics();
    EXPECT_EQ(stats.spearHead, LogIndex{2});
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
  }
  {
    auto stats = follower->getLocalStatistics();
    EXPECT_EQ(stats.spearHead, LogIndex{1});
    EXPECT_EQ(stats.commitIndex, LogIndex{1});
  }

  // now write another entry to both
  {
    leader->becomeLeader(LogTerm{3}, {follower}, 2);
    follower->becomeFollower(LogTerm{3}, leader->participantId());
    auto idx = leader->insert(LogPayload{"third entry"});
    auto f = leader->waitFor(idx);
    leader->runAsyncStep();
    ASSERT_TRUE(follower->hasPendingAppendEntries());
    {
      auto request = follower->pendingAppendEntries()[0].request;
      EXPECT_EQ(request.leaderId, leader->participantId());
      EXPECT_EQ(request.leaderTerm, LogTerm{3});
      EXPECT_EQ(request.leaderCommit, LogIndex{0});
      EXPECT_EQ(request.prevLogTerm, LogTerm{2});
      EXPECT_EQ(request.prevLogIndex, LogIndex{2});
    }

    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }
    ASSERT_TRUE(f.isReady());
  }

  {
    auto stats = leader->getLocalStatistics();
    EXPECT_EQ(stats.spearHead, LogIndex{3});
    EXPECT_EQ(stats.commitIndex, LogIndex{3});
  }
  {
    auto stats = follower->getLocalStatistics();
    EXPECT_EQ(stats.spearHead, LogIndex{3});
    EXPECT_EQ(stats.commitIndex, LogIndex{3});
  }
}


TEST_F(ReplicatedLogTest, test) {
  auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
  auto const ourParticipantId = ParticipantId{1};
  auto persistedLog = std::make_shared<MockLog>(LogId{1});
  auto log = ReplicatedLog{ourParticipantId, state, persistedLog};

  log.becomeLeader(LogTerm{1}, {}, 1);

  {
    auto stats = log.getLocalStatistics();
    EXPECT_EQ(LogIndex{0}, stats.commitIndex);
    EXPECT_EQ(LogIndex{0}, stats.spearHead);
  }

  auto const payload = LogPayload{"myLogEntry 1"};
  auto index = log.insert(payload);
  EXPECT_EQ(LogIndex{1}, index);

  auto f = log.waitFor(index);

  {
    auto stats = log.getLocalStatistics();
    EXPECT_EQ(LogIndex{0}, stats.commitIndex);
    EXPECT_EQ(LogIndex{1}, stats.spearHead);
  }

  log.runAsyncStep();

  EXPECT_TRUE(f.isReady());

  {
    auto stats = log.getLocalStatistics();
    EXPECT_EQ(LogIndex{1}, stats.commitIndex);
    EXPECT_EQ(LogIndex{1}, stats.spearHead);
  }

  auto it = persistedLog->read(LogIndex{1});
  auto const elt = it->next();
  ASSERT_TRUE(elt.has_value());
  auto const logEntry = *elt;
  EXPECT_EQ(LogIndex{1}, logEntry.logIndex());
  EXPECT_EQ(LogTerm{1}, logEntry.logTerm());
  EXPECT_EQ(payload, logEntry.logPayload());
}

TEST_F(ReplicatedLogTest, appendEntries) {
  auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
  auto const ourParticipantId = ParticipantId{1};
  auto const leaderId = ParticipantId{2};
  auto persistedLog = std::make_shared<MockLog>(LogId{1});
  auto log = DelayedFollowerLog{ourParticipantId, state, persistedLog};

  log.becomeFollower(LogTerm{1}, leaderId);

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = LogTerm{1};
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{0};
    request.prevLogIndex = LogIndex{0};
    request.leaderCommit = LogIndex{0};
    request.entries = {};

    auto future = log.appendEntries(request);
    ASSERT_FALSE(future.isReady());
    log.runAsyncAppendEntries();
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    EXPECT_TRUE(res->success);
    EXPECT_EQ(LogTerm{1}, res->logTerm);
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = LogTerm{1};
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{0};
    request.prevLogIndex = LogIndex{0};
    request.leaderCommit = LogIndex{0};
    request.entries = {LogEntry{LogTerm{1}, LogIndex{1}, LogPayload{"one"}}};
    {
      auto future = log.appendEntries(request);
      ASSERT_FALSE(future.isReady());
      log.runAsyncAppendEntries();
      ASSERT_TRUE(future.isReady());
      auto res = future.getTry();
      ASSERT_TRUE(res.hasValue());
      EXPECT_TRUE(res->success);
      EXPECT_EQ(LogTerm{1}, res->logTerm);
    }
    auto const maybeEntry = log.getEntryByIndex(LogIndex{1});
    ASSERT_TRUE(maybeEntry.has_value());
    auto const& entry = maybeEntry.value();
    ASSERT_EQ(LogIndex{1}, entry.logIndex());
    ASSERT_EQ(LogTerm{1}, entry.logTerm());
    ASSERT_EQ(LogPayload{"one"}, entry.logPayload());
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = LogTerm{1};
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{1};
    request.prevLogIndex = LogIndex{2};
    request.leaderCommit = LogIndex{0};
    request.entries = {};

    auto future = log.appendEntries(request);
    ASSERT_FALSE(future.isReady());
    log.runAsyncAppendEntries();
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    EXPECT_FALSE(res->success);
    EXPECT_EQ(LogTerm{1}, res->logTerm);
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = LogTerm{1};
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{0};
    request.prevLogIndex = LogIndex{1};
    request.leaderCommit = LogIndex{0};
    request.entries = {};

    auto future = log.appendEntries(request);
    ASSERT_FALSE(future.isReady());
    log.runAsyncAppendEntries();
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    EXPECT_FALSE(res->success);
    EXPECT_EQ(LogTerm{1}, res->logTerm);
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = LogTerm{1};
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{1};
    request.prevLogIndex = LogIndex{1};
    request.leaderCommit = LogIndex{0};
    request.entries = {LogEntry{LogTerm{1}, LogIndex{2}, LogPayload{"two"}},
                       LogEntry{LogTerm{1}, LogIndex{3}, LogPayload{"three"}}};
    {
      auto future = log.appendEntries(request);
      ASSERT_FALSE(future.isReady());
      log.runAsyncAppendEntries();
      ASSERT_TRUE(future.isReady());
      auto res = future.getTry();
      ASSERT_TRUE(res.hasValue());
      EXPECT_TRUE(res->success);
      EXPECT_EQ(LogTerm{1}, res->logTerm);
    }
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{2});
      ASSERT_TRUE(maybeEntry.has_value());
      auto const& entry = maybeEntry.value();
      ASSERT_EQ(LogIndex{2}, entry.logIndex());
      ASSERT_EQ(LogTerm{1}, entry.logTerm());
      ASSERT_EQ(LogPayload{"two"}, entry.logPayload());
    }
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{3});
      ASSERT_TRUE(maybeEntry.has_value());
      auto const& entry = maybeEntry.value();
      ASSERT_EQ(LogIndex{3}, entry.logIndex());
      ASSERT_EQ(LogTerm{1}, entry.logTerm());
      ASSERT_EQ(LogPayload{"three"}, entry.logPayload());
    }
  }

  {
    log.becomeFollower(LogTerm{2}, leaderId);
    auto request = AppendEntriesRequest{};
    request.leaderTerm = LogTerm{2};
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{1};
    request.prevLogIndex = LogIndex{1};
    request.leaderCommit = LogIndex{0};
    request.entries = {LogEntry{LogTerm{2}, LogIndex{2}, LogPayload{"two.2"}}};

    {
      auto future = log.appendEntries(request);
      ASSERT_FALSE(future.isReady());
      log.runAsyncAppendEntries();
      ASSERT_TRUE(future.isReady());
      auto res = future.getTry();
      ASSERT_TRUE(res.hasValue());
      EXPECT_TRUE(res->success);
      EXPECT_EQ(LogTerm{2}, res->logTerm);
    }
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{1});
      ASSERT_TRUE(maybeEntry.has_value());
      auto const& entry = maybeEntry.value();
      ASSERT_EQ(LogIndex{1}, entry.logIndex());
      ASSERT_EQ(LogTerm{1}, entry.logTerm());
      ASSERT_EQ(LogPayload{"one"}, entry.logPayload());
    }
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{2});
      ASSERT_TRUE(maybeEntry.has_value());
      auto const& entry = maybeEntry.value();
      ASSERT_EQ(LogIndex{2}, entry.logIndex());
      ASSERT_EQ(LogTerm{2}, entry.logTerm());
      ASSERT_EQ(LogPayload{"two.2"}, entry.logPayload());
    }
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{3});
      ASSERT_FALSE(maybeEntry.has_value());
    }
  }
}

TEST_F(ReplicatedLogTest, replicationTest) {
  auto const leaderId = ParticipantId{1};
  auto const leaderState = std::make_shared<InMemoryState>();
  auto const leaderPersistentLog = std::make_shared<MockLog>(LogId{1});
  auto leaderLog =
      std::make_shared<ReplicatedLog>(leaderId, leaderState, leaderPersistentLog);

  auto const followerId = ParticipantId{3};
  auto const followerState = std::make_shared<InMemoryState>();
  auto const followerPersistentLog = std::make_shared<MockLog>(LogId{5});
  auto followerLog = std::make_shared<DelayedFollowerLog>(followerId, followerState,
                                                          followerPersistentLog);

  {
    followerLog->becomeFollower(LogTerm{1}, leaderId);
    leaderLog->becomeLeader(LogTerm{1}, {followerLog}, 2);

    {
      auto const payload = LogPayload{"myLogEntry 1"};
      auto index = leaderLog->insert(payload);
      ASSERT_EQ(LogIndex{1}, index);
    }

    auto fut = leaderLog->waitFor(LogIndex{1});

    ASSERT_FALSE(fut.isReady());
    ASSERT_FALSE(followerLog->hasPendingAppendEntries());
    leaderLog->runAsyncStep();
    // future should not be ready because write concern is two
    ASSERT_FALSE(fut.isReady());
    ASSERT_TRUE(followerLog->hasPendingAppendEntries());

    followerLog->runAsyncAppendEntries();
    ASSERT_TRUE(fut.isReady());

    auto& info = fut.get();
    ASSERT_EQ(info->quorum.size(), 2);
    ASSERT_EQ(info->term, LogTerm{1});

    {
      auto stats = followerLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{0});
      ASSERT_EQ(stats.spearHead, LogIndex{1});
    }

    ASSERT_TRUE(followerLog->hasPendingAppendEntries());
  }

  {
    leaderLog->becomeLeader(LogTerm{2}, {followerLog}, 1);
    {
      auto const payload = LogPayload{"myLogEntry 2"};
      auto index = leaderLog->insert(payload);
      ASSERT_EQ(LogIndex{2}, index);
    }
    auto fut = leaderLog->waitFor(LogIndex{2});
    leaderLog->runAsyncStep();
    ASSERT_TRUE(followerLog->hasPendingAppendEntries());
    ASSERT_TRUE(fut.isReady());
    {
      auto& info = fut.get();
      ASSERT_EQ(info->quorum.size(), 1);
      ASSERT_EQ(info->term, LogTerm{2});
      ASSERT_EQ(info->quorum[0], leaderId);
    }

    {
      auto stats = followerLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{0});
      ASSERT_EQ(stats.spearHead, LogIndex{1});
    }
    followerLog->runAsyncAppendEntries();
    {
      auto stats = followerLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{1});
      ASSERT_EQ(stats.spearHead, LogIndex{1});
    }
    // should still be true because of leader retry
    ASSERT_TRUE(followerLog->hasPendingAppendEntries());
    followerLog->becomeFollower(LogTerm{2}, leaderId);
    followerLog->runAsyncAppendEntries();
    {
      auto stats = followerLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{2});
      ASSERT_EQ(stats.spearHead, LogIndex{2});
    }
  }
}

TEST_F(ReplicatedLogTest, replicationTest2) {
  auto const leaderId = ParticipantId{1};
  auto const leaderState = std::make_shared<InMemoryState>();
  auto const leaderPersistentLog = std::make_shared<MockLog>(LogId{1});
  auto leaderLog =
      std::make_shared<ReplicatedLog>(leaderId, leaderState, leaderPersistentLog);

  auto const followerId = ParticipantId{3};
  auto const followerState = std::make_shared<InMemoryState>();
  auto const followerPersistentLog = std::make_shared<MockLog>(LogId{5});
  auto followerLog = std::make_shared<DelayedFollowerLog>(followerId, followerState,
                                                          followerPersistentLog);

  {
    followerLog->becomeFollower(LogTerm{1}, leaderId);
    leaderLog->becomeLeader(LogTerm{1}, {followerLog}, 2);

    {
      leaderLog->insert(LogPayload{"myLogEntry 1"});
      leaderLog->insert(LogPayload{"myLogEntry 2"});
      leaderLog->insert(LogPayload{"myLogEntry 3"});
      auto index = leaderLog->insert(LogPayload{"myLogEntry 4"});
      ASSERT_EQ(LogIndex{4}, index);
    }

    {
      auto stats = leaderLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{0});
      ASSERT_EQ(stats.spearHead, LogIndex{4});
    }

    auto fut = leaderLog->waitFor(LogIndex{4});

    ASSERT_FALSE(fut.isReady());
    ASSERT_FALSE(followerLog->hasPendingAppendEntries());
    leaderLog->runAsyncStep();
    // future should not be ready because write concern is two
    ASSERT_FALSE(fut.isReady());
    ASSERT_TRUE(followerLog->hasPendingAppendEntries());
    followerLog->runAsyncAppendEntries();
    ASSERT_TRUE(fut.isReady());
    auto& info = fut.get();
    ASSERT_EQ(info->quorum.size(), 2);
    ASSERT_EQ(info->term, LogTerm{1});

    {
      auto stats = leaderLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{4});
      ASSERT_EQ(stats.spearHead, LogIndex{4});
    }

    {
      auto stats = followerLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{0});
      ASSERT_EQ(stats.spearHead, LogIndex{4});
    }
    ASSERT_TRUE(followerLog->hasPendingAppendEntries());
    followerLog->runAsyncAppendEntries();
    {
      auto stats = followerLog->getLocalStatistics();
      ASSERT_EQ(stats.commitIndex, LogIndex{4});
      ASSERT_EQ(stats.spearHead, LogIndex{4});
    }
  }

  ASSERT_FALSE(followerLog->hasPendingAppendEntries());
}

TEST_F(ReplicatedLogTest, parallelAccessTest) {
  using namespace std::chrono_literals;

  auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
  auto const ourParticipantId = ParticipantId{1};
  auto persistedLog = std::make_shared<MockLog>(LogId{1});
  struct alignas(128) {
    ReplicatedLog log;
    std::atomic<bool> go = false;
    std::atomic<bool> stopClientThreads = false;
    std::atomic<bool> stopReplicationThread = false;
    std::atomic<std::size_t> threadsReady = 0;
    std::atomic<std::size_t> threadsSatisfied = 0;
  } data{ReplicatedLog{ourParticipantId, state, persistedLog}};
  data.log.becomeLeader(LogTerm{1}, {}, 1);

  constexpr static auto genPayload = [](uint16_t thread, uint32_t i) {
    auto str = std::string(16, ' ');

    // 5 digits for thread
    static_assert(std::numeric_limits<uint16_t>::max() <= 99'999);
    // 10 digits for i
    static_assert(std::numeric_limits<uint32_t>::max() <= 99'999'999'999);

    auto p = str.begin() + 5;
    *p = ':';
    do {
      --p;
      *p = static_cast<char>('0' + (thread % 10));
      thread /= 10;
    } while (thread > 0);
    p = str.end();
    do {
      --p;
      *p = static_cast<char>('0' + (i % 10));
      i /= 10;
    } while (i > 0);

    return str;
  };
  EXPECT_EQ("    0:         0", genPayload(0, 0));
  EXPECT_EQ("   11:        42", genPayload(11, 42));
  EXPECT_EQ("65535:4294967295", genPayload(65535, 4294967295));

  constexpr static auto maxIter = std::numeric_limits<uint32_t>::max();

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  auto alternatinglyInsertAndRead = [&](auto const threadIdx) {
    return [threadIdx, &data] {
      data.threadsReady.fetch_add(1);
      while (!data.go.load()) {
      }

      auto& log = data.log;
      for (auto i = std::uint32_t{0}; i < maxIter && !data.stopClientThreads.load(); ++i) {
        auto const payload = LogPayload{genPayload(threadIdx, i)};
        auto const idx = log.insert(payload);
        std::this_thread::sleep_for(1ns);
        auto fut = log.waitFor(idx);
        fut.get();
        // TODO Rather use a function that only allows to read replicated entries,
        //      and wait for the entry to be replicated beforehand.
        auto res = log.getEntryByIndex(idx);
        ASSERT_TRUE(res.has_value());
        auto const& entry = res.value();
        EXPECT_EQ(payload, entry.logPayload());
        EXPECT_EQ(idx, entry.logIndex());
        if (i == 1000) {
          // we should have done at least a few iterations before finishing
          data.threadsSatisfied.fetch_add(1, std::memory_order_relaxed);
        }
      }
    };
  };

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  auto insertManyThenRead = [&](auto const threadIdx) {
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    return [threadIdx, &data] {
      data.threadsReady.fetch_add(1);
      while (!data.go.load()) {
      }

      auto& log = data.log;

      constexpr auto batch = 100;
      auto idxs = std::vector<LogIndex>{};
      idxs.resize(batch);

      for (auto i = std::uint32_t{0}; i < maxIter && !data.stopClientThreads.load(); i += batch) {
        for (auto k = 0; k < batch && i + k < maxIter; ++k) {
          auto const payload = LogPayload{genPayload(threadIdx, i + k)};
          idxs[k] = log.insert(payload);
        }
        std::this_thread::sleep_for(1ns);
        auto fut = log.waitFor(idxs.back());
        fut.get();
        for (auto k = 0; k < batch && i + k < maxIter; ++k) {
          auto const payload = LogPayload{genPayload(threadIdx, i + k)};
          auto res = log.getEntryByIndex(idxs[k]);
          auto const& entry = res.value();
          EXPECT_EQ(payload, entry.logPayload());
          EXPECT_EQ(idxs[k], entry.logIndex());
        }
        if (i == 10 * batch) {
          // we should have done at least a few iterations before finishing
          data.threadsSatisfied.fetch_add(1, std::memory_order_relaxed);
        }
      }
    };
  };

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  auto runReplicationWithIntermittentPauses = [&data] {
    for (auto i = 0; ; ++i) {
      data.log.runAsyncStep();
      if (i % 16) {
        std::this_thread::sleep_for(10ns);
        if (data.stopReplicationThread.load()) {
          return;
        }
      }
    }
  };

  // start replication
  auto replicationThread = std::thread{runReplicationWithIntermittentPauses};

  std::vector<std::thread> clientThreads;
  auto threadCounter = uint16_t{0};
  clientThreads.emplace_back(alternatinglyInsertAndRead(threadCounter++));
  clientThreads.emplace_back(insertManyThenRead(threadCounter++));


  ASSERT_EQ(clientThreads.size(), threadCounter);
  while (data.threadsReady.load() < clientThreads.size()) {
  }
  data.go.store(true);
  while (data.threadsSatisfied.load() < clientThreads.size()) {
    std::this_thread::sleep_for(100us);
  }
  data.stopClientThreads.store(true);

  for (auto& thread : clientThreads) {
    thread.join();
  }

  // stop replication only after all client threads joined, so we don't block
  // them in some intermediate state
  data.stopReplicationThread.store(true);
  replicationThread.join();

  auto stats = data.log.getLocalStatistics();
  EXPECT_LE(LogIndex{8000}, stats.commitIndex);
  EXPECT_LE(stats.commitIndex, stats.spearHead);
}

TEST(LogIndexTest, compareOperators) {
  auto one = LogIndex{1};
  auto two = LogIndex{2};

  EXPECT_TRUE(one == one);
  EXPECT_FALSE(one != one);
  EXPECT_FALSE(one < one);
  EXPECT_FALSE(one > one);
  EXPECT_TRUE(one <= one);
  EXPECT_TRUE(one >= one);

  EXPECT_FALSE(one == two);
  EXPECT_TRUE(one != two);
  EXPECT_TRUE(one < two);
  EXPECT_FALSE(one > two);
  EXPECT_TRUE(one <= two);
  EXPECT_FALSE(one >= two);

  EXPECT_FALSE(two == one);
  EXPECT_TRUE(two != one);
  EXPECT_FALSE(two < one);
  EXPECT_TRUE(two > one);
  EXPECT_FALSE(two <= one);
  EXPECT_TRUE(two >= one);
}
