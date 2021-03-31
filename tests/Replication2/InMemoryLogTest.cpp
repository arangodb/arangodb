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

#include <Replication2/InMemoryLog.h>
#include <Basics/Exceptions.h>

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
    auto stats = log.getStatistics();
    ASSERT_EQ(LogIndex{0}, stats.commitIndex);
    ASSERT_EQ(LogIndex{0}, stats.spearHead);
  }

  auto const payload = LogPayload{"myLogEntry 1"};
  auto index = log.insert(payload);
  ASSERT_EQ(LogIndex{1}, index);

  auto f = log.waitFor(index);

  {
    auto stats = log.getStatistics();
    ASSERT_EQ(LogIndex{0}, stats.commitIndex);
    ASSERT_EQ(LogIndex{1}, stats.spearHead);
  }

  log.runAsyncStep();

  ASSERT_TRUE(f.isReady());

  {
    auto stats = log.getStatistics();
    ASSERT_EQ(LogIndex{1}, stats.commitIndex);
    ASSERT_EQ(LogIndex{1}, stats.spearHead);
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
  auto const term = LogTerm{1};
  auto persistedLog = std::make_shared<MockLog>(LogId{1});
  auto log = InMemoryLog{ourParticipantId, state, persistedLog};

  log.becomeFollower(term, leaderId);

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = term;
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{0};
    request.prevLogIndex = LogIndex{0};
    request.leaderCommit = LogIndex{0};
    request.entries = {};

    auto future = log.appendEntries(request);
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    EXPECT_TRUE(res->success);
    EXPECT_EQ(term, res->logTerm);
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = term;
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{0};
    request.prevLogIndex = LogIndex{0};
    request.leaderCommit = LogIndex{0};
    request.entries = {LogEntry{term, LogIndex{1}, LogPayload{"one"}}};
    {
      auto future = log.appendEntries(request);
      ASSERT_TRUE(future.isReady());
      auto res = future.getTry();
      ASSERT_TRUE(res.hasValue());
      EXPECT_TRUE(res->success);
      EXPECT_EQ(term, res->logTerm);
    }
    auto future = log.waitFor(LogIndex{1});
    log.runAsyncStep();
    ASSERT_TRUE(future.isReady());

    auto const maybeEntry = log.getEntryByIndex(LogIndex{1});
    ASSERT_TRUE(maybeEntry.has_value());
    auto const& entry = maybeEntry.value();
    ASSERT_EQ(LogIndex{1}, entry.logIndex());
    ASSERT_EQ(term, entry.logTerm());
    ASSERT_EQ(LogPayload{"one"}, entry.logPayload());
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = term;
    request.leaderId = leaderId;
    request.prevLogTerm = term;
    request.prevLogIndex = LogIndex{2};
    request.leaderCommit = LogIndex{0};
    request.entries = {};

    auto future = log.appendEntries(request);
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    EXPECT_FALSE(res->success);
    EXPECT_EQ(term, res->logTerm);
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = term;
    request.leaderId = leaderId;
    request.prevLogTerm = LogTerm{0};
    request.prevLogIndex = LogIndex{1};
    request.leaderCommit = LogIndex{0};
    request.entries = {};

    auto future = log.appendEntries(request);
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    EXPECT_FALSE(res->success);
    EXPECT_EQ(term, res->logTerm);
  }

  {
    auto request = AppendEntriesRequest{};
    request.leaderTerm = term;
    request.leaderId = leaderId;
    request.prevLogTerm = term;
    request.prevLogIndex = LogIndex{1};
    request.leaderCommit = LogIndex{0};
    request.entries = {LogEntry{term, LogIndex{2}, LogPayload{"two"}}, LogEntry{term, LogIndex{3}, LogPayload{"three"}}};
    {
      auto future = log.appendEntries(request);
      ASSERT_TRUE(future.isReady());
      auto res = future.getTry();
      ASSERT_TRUE(res.hasValue());
      EXPECT_TRUE(res->success);
      EXPECT_EQ(term, res->logTerm);
    }
    auto future = log.waitFor(LogIndex{3});
    log.runAsyncStep();
    ASSERT_TRUE(future.isReady());
    auto res = future.getTry();
    ASSERT_TRUE(res.hasValue());
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{2});
      ASSERT_TRUE(maybeEntry.has_value());
      auto const& entry = maybeEntry.value();
      ASSERT_EQ(LogIndex{2}, entry.logIndex());
      ASSERT_EQ(term, entry.logTerm());
      ASSERT_EQ(LogPayload{"two"}, entry.logPayload());
    }
    {
      auto const maybeEntry = log.getEntryByIndex(LogIndex{3});
      ASSERT_TRUE(maybeEntry.has_value());
      auto const& entry = maybeEntry.value();
      ASSERT_EQ(LogIndex{3}, entry.logIndex());
      ASSERT_EQ(term, entry.logTerm());
      ASSERT_EQ(LogPayload{"three"}, entry.logPayload());
    }
  }


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
