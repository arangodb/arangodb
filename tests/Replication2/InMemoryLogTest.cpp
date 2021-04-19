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

#include <Basics/Exceptions.h>
#include <Replication2/InMemoryLog.h>

#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::replication2;

TEST(InMemoryLogTest, test) {
  auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
  auto const ourParticipantId = ParticipantId{1};
  auto persistedLog = std::make_shared<MockLog>(LogId{1});
  auto log = InMemoryLog{ourParticipantId, state, persistedLog};

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

TEST(InMemoryLogTest, appendEntries) {
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

TEST(InMemoryLogTest, replicationTest) {
  auto const leaderId = ParticipantId{1};
  auto const leaderState = std::make_shared<InMemoryState>();
  auto const leaderPersistentLog = std::make_shared<MockLog>(LogId{1});
  auto leaderLog = std::make_shared<InMemoryLog>(leaderId, leaderState, leaderPersistentLog);

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
      ASSERT_EQ(stats.commitIndex, LogIndex{0});
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

TEST(InMemoryLogTest, replicationTest2) {
  auto const leaderId = ParticipantId{1};
  auto const leaderState = std::make_shared<InMemoryState>();
  auto const leaderPersistentLog = std::make_shared<MockLog>(LogId{1});
  auto leaderLog = std::make_shared<InMemoryLog>(leaderId, leaderState, leaderPersistentLog);

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
