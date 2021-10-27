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
using namespace arangodb::replication2::test;

struct ChangeStreamTests : ReplicatedLogTest {};

TEST_F(ChangeStreamTests, ask_for_existing_entries) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock,
                                     "leader", std::move(coreA), LogTerm{3}, {}, 1);
  leader->triggerAsyncReplication();
  {
    auto fut = leader->waitForIterator(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    {
      auto entry = std::optional<LogEntryView>{};
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

TEST_F(ChangeStreamTests, ask_for_non_existing_entries) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {}, 1);
  // Note that the leader inserts an empty log entry in LogLeader::construct
  auto fut = leader->waitForIterator(LogIndex{3});

  ASSERT_FALSE(fut.isReady());

  leader->triggerAsyncReplication();

  ASSERT_TRUE(fut.isReady());
  std::move(fut).then([](auto&&){});

  fut = leader->waitForIterator(LogIndex{5});

  ASSERT_FALSE(fut.isReady());

  auto const idx4 = leader->insert(LogPayload::createFromString("fourth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in LogLeader::construct
  EXPECT_EQ(idx4, LogIndex{5});
  auto const idx5 = leader->insert(LogPayload::createFromString("fifth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in LogLeader::construct
  EXPECT_EQ(idx5, LogIndex{6});
  leader->triggerAsyncReplication();

  ASSERT_TRUE(fut.isReady());
  {
    auto entry = std::optional<LogEntryView>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), idx4);

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), idx5);

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(ChangeStreamTests, ask_for_internal_entries_should_block_until_the_next_user_entry) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
          LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
          LogPayload::createFromString("third entry"))};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
      std::move(coreA), LogTerm{3}, {}, 1);
  // The forth entry is inserted in LogLeader::construct - wait for it
  auto fut = leader->waitForIterator(LogIndex{4});

  ASSERT_FALSE(fut.isReady());

  leader->triggerAsyncReplication();

  // because index 4 is internal, the future is still not ready
  ASSERT_FALSE(fut.isReady());

  // Now insert another entry
  auto const idx5 = leader->insert(LogPayload::createFromString("fourth entry"), false,
      LogLeader::doNotTriggerAsyncReplication);
  leader->triggerAsyncReplication();
  EXPECT_EQ(idx5, LogIndex{5});

  // Now the future is ready
  ASSERT_TRUE(fut.isReady());
  {
    // We shall only see index 5
    auto entry = std::optional<LogEntryView>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), idx5);

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(ChangeStreamTests, ask_for_non_existing_entries_with_follower) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

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
  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {follower}, 2);

  leader->triggerAsyncReplication();
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto fut = leader->waitForIterator(LogIndex{4});
  ASSERT_FALSE(fut.isReady());

  leader->insert(LogPayload::createFromString("fourth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->insert(LogPayload::createFromString("fifth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->triggerAsyncReplication();

  ASSERT_FALSE(fut.isReady());
  ASSERT_TRUE(follower->hasPendingAppendEntries());
  follower->runAsyncAppendEntries();
  ASSERT_TRUE(fut.isReady());

  {
    auto entry = std::optional<LogEntryView>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{5});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{6});

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(ChangeStreamTests, ask_for_non_replicated_entries_with_follower) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

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
  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {follower}, 2);

  leader->triggerAsyncReplication();
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  leader->insert(LogPayload::createFromString("fourth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->insert(LogPayload::createFromString("fifth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->triggerAsyncReplication();

  auto fut = leader->waitForIterator(LogIndex{3});
  ASSERT_TRUE(fut.isReady());

  {
    auto entry = std::optional<LogEntryView>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{3});

    entry = iter->next();
    EXPECT_FALSE(entry.has_value());
  }

  ASSERT_TRUE(follower->hasPendingAppendEntries());
  follower->runAsyncAppendEntries();
}

TEST_F(ChangeStreamTests, ask_for_existing_entries_follower) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto followerLog = makeReplicatedLog(LogId{2});
  auto follower = followerLog->becomeFollower("follower", LogTerm{3}, "leader");

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {follower}, 1);
  leader->triggerAsyncReplication();

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  {
    auto fut = follower->waitForIterator(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    {
      auto entry = std::optional<LogEntryView>{};
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

TEST_F(ChangeStreamTests, ask_for_non_existing_entries_follower) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto followerLog = makeReplicatedLog(LogId{2});
  auto follower = followerLog->becomeFollower("follower", LogTerm{3}, "leader");

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {follower}, 2);
  leader->triggerAsyncReplication();

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto fut = follower->waitForIterator(LogIndex{4});
  ASSERT_FALSE(fut.isReady());

  leader->insert(LogPayload::createFromString("fourth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->insert(LogPayload::createFromString("fifth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->triggerAsyncReplication();

  // replicate entries, not commit index
  ASSERT_TRUE(follower->hasPendingAppendEntries());
  follower->runAsyncAppendEntries();
  ASSERT_FALSE(fut.isReady());

  // replicated commit index
  ASSERT_TRUE(follower->hasPendingAppendEntries());
  follower->runAsyncAppendEntries();
  ASSERT_TRUE(fut.isReady());
  {
    auto entry = std::optional<LogEntryView>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{5});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{6});

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}

TEST_F(ChangeStreamTests, ask_for_non_committed_entries_follower) {
  auto const entries = {
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("first entry")),
      replication2::PersistingLogEntry(LogTerm{1}, LogIndex{2},
                             LogPayload::createFromString("second entry")),
      replication2::PersistingLogEntry(LogTerm{2}, LogIndex{3},
                             LogPayload::createFromString("third entry"))};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto followerLog = makeReplicatedLog(LogId{2});
  auto follower = followerLog->becomeFollower("follower", LogTerm{3}, "leader");

  auto leader = LogLeader::construct(defaultLogger(), _logMetricsMock, _optionsMock, "leader",
                                     std::move(coreA), LogTerm{3}, {follower}, 2);
  leader->triggerAsyncReplication();

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  leader->insert(LogPayload::createFromString("fourth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->insert(LogPayload::createFromString("fifth entry"), false,
                 LogLeader::doNotTriggerAsyncReplication);
  leader->triggerAsyncReplication();

  // replicate entries, not commit index
  ASSERT_TRUE(follower->hasPendingAppendEntries());
  follower->runAsyncAppendEntries();

  auto fut = follower->waitForIterator(LogIndex{3});
  ASSERT_TRUE(fut.isReady());

  {
    auto entry = std::optional<LogEntryView>{};
    auto iter = std::move(fut).get();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{3});

    entry = iter->next();
    ASSERT_FALSE(entry.has_value());
  }
}
