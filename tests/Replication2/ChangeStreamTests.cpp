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

#include "TestHelper.h"

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogLeader.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct ChangeStreamTests : ReplicatedLogTest {};

TEST_F(ChangeStreamTests, ask_for_exisiting_entries) {
  auto const entries = {
      replication2::LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"first entry"}),
      replication2::LogEntry(LogTerm{1}, LogIndex{2}, LogPayload{"second entry"}),
      replication2::LogEntry(LogTerm{2}, LogIndex{3}, LogPayload{"third entry"})};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {}, 1);
  leader->runAsyncStep();
  {
    auto fut = leader->waitForIterator(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    {
      auto entry = std::optional<LogEntry>{};
      auto iter = std::move(fut).get();

      entry = iter->next();
      ASSERT_TRUE(entry.has_value());
      EXPECT_EQ(entry->logIndex(), LogIndex{2});

      entry = iter->next();
      ASSERT_TRUE(entry.has_value());
      EXPECT_EQ(entry->logIndex(), LogIndex{3});

      entry = iter->next();
      ASSERT_FALSE(entry.has_value());
    }
  }
}

TEST_F(ChangeStreamTests, ask_for_non_exisiting_entries) {
  auto const entries = {
      replication2::LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"first entry"}),
      replication2::LogEntry(LogTerm{1}, LogIndex{2}, LogPayload{"second entry"}),
      replication2::LogEntry(LogTerm{2}, LogIndex{3}, LogPayload{"third entry"})};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {}, 1);
  leader->runAsyncStep();

  auto fut = leader->waitForIterator(LogIndex{4});
  ASSERT_FALSE(fut.isReady());

  leader->insert(LogPayload{"fourth entry"});
  leader->insert(LogPayload{"fifth entry"});
  leader->runAsyncStep();

  ASSERT_TRUE(fut.isReady());
  {
    auto entry = std::optional<LogEntry>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{4});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{5});

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(ChangeStreamTests, ask_for_non_exisiting_entries_with_follower) {
  auto const entries = {
      replication2::LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"first entry"}),
      replication2::LogEntry(LogTerm{1}, LogIndex{2}, LogPayload{"second entry"}),
      replication2::LogEntry(LogTerm{2}, LogIndex{3}, LogPayload{"third entry"})};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto coreB = makeLogCore(LogId{2});
  auto follower = std::make_shared<DelayedFollowerLog>(defaultLogger(), _logMetricsMock,
                                                       "follower", std::move(coreB),
                                                       LogTerm{3}, "leader");
  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {follower}, 2);

  leader->runAsyncStep();
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto fut = leader->waitForIterator(LogIndex{4});
  ASSERT_FALSE(fut.isReady());

  leader->insert(LogPayload{"fourth entry"});
  leader->insert(LogPayload{"fifth entry"});
  leader->runAsyncStep();

  ASSERT_FALSE(fut.isReady());
  ASSERT_TRUE(follower->hasPendingAppendEntries());
  follower->runAsyncAppendEntries();
  ASSERT_TRUE(fut.isReady());

  {
    auto entry = std::optional<LogEntry>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{4});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{5});

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}
