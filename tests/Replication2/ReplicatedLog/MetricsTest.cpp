////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"
#include "Metrics/Counter.h"

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct ReplicatedLogMetricsTest : ReplicatedLogTest {
  auto makeFollower(ParticipantId id, LogTerm term, ParticipantId leaderId)
      -> std::shared_ptr<ReplicatedLog> {
    auto core = makeLogCore(LogId{3});
    auto log = std::make_shared<ReplicatedLog>(std::move(core), _logMetricsMock,
                                               _optionsMock, defaultLogger());
    log->becomeFollower(std::move(id), term, std::move(leaderId));
    return log;
  }

  MessageId nextMessageId{0};
};

TEST_F(ReplicatedLogMetricsTest, follower_append_count_entries) {
  auto log = makeFollower("follower", LogTerm{5}, "leader");
  auto follower = log->getFollower();

  // insert one entry but do not increment the commit index
  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogEntry = TermIndexPair{LogTerm{0}, LogIndex{0}};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {InMemoryLogEntry(
        PersistingLogEntry(LogTerm{5}, LogIndex{1},
                           LogPayload::createFromString("some payload")))};

    auto const acceptedCountBefore =
        _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
    auto const committedCountBefore =
        _logMetricsMock->replicatedLogNumberCommittedEntries->load();
    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    ASSERT_EQ(f.get().errorCode, TRI_ERROR_NO_ERROR);

    auto const acceptedCountAfter =
        _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
    auto const committedCountAfter =
        _logMetricsMock->replicatedLogNumberCommittedEntries->load();
    EXPECT_EQ(acceptedCountAfter, acceptedCountBefore + 1);
    EXPECT_EQ(committedCountBefore, committedCountAfter);
  }
  // insert another entry and increment the commit index
  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogEntry = TermIndexPair{LogTerm{5}, LogIndex{1}};
    request.leaderCommit = LogIndex{1};
    request.messageId = ++nextMessageId;
    request.entries = {InMemoryLogEntry(
        PersistingLogEntry(LogTerm{5}, LogIndex{2},
                           LogPayload::createFromString("some payload")))};

    auto const acceptedCountBefore =
        _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
    auto const committedCountBefore =
        _logMetricsMock->replicatedLogNumberCommittedEntries->load();
    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    ASSERT_EQ(f.get().errorCode, TRI_ERROR_NO_ERROR);

    auto const acceptedCountAfter =
        _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
    auto const committedCountAfter =
        _logMetricsMock->replicatedLogNumberCommittedEntries->load();
    EXPECT_EQ(acceptedCountAfter, acceptedCountBefore + 1);
    EXPECT_EQ(committedCountAfter, committedCountBefore + 1);
  }
}

TEST_F(ReplicatedLogMetricsTest, follower_append_dont_count_entries_error) {
  auto log = makeFollower("follower", LogTerm{5}, "leader");
  auto follower = log->getFollower();

  // insert one entry with an invalid leader, it will be rejected
  // we expect all metrics to be unchanged.
  {
    AppendEntriesRequest request;
    request.leaderId = "WRONG_LEADER";
    request.leaderTerm = LogTerm{5};
    request.prevLogEntry = TermIndexPair{LogTerm{0}, LogIndex{0}};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {InMemoryLogEntry(
        PersistingLogEntry(LogTerm{1}, LogIndex{1},
                           LogPayload::createFromString("some payload")))};

    auto const acceptedCountBefore =
        _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
    auto const committedCountBefore =
        _logMetricsMock->replicatedLogNumberCommittedEntries->load();
    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    ASSERT_NE(f.get().errorCode, TRI_ERROR_NO_ERROR);

    auto const acceptedCountAfter =
        _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
    auto const committedCountAfter =
        _logMetricsMock->replicatedLogNumberCommittedEntries->load();
    EXPECT_EQ(acceptedCountAfter, acceptedCountBefore);
    EXPECT_EQ(committedCountBefore, committedCountAfter);
  }
}

TEST_F(ReplicatedLogMetricsTest, follower_count_compaction) {
  _optionsMock->_thresholdLogCompaction = 1;  // set compactions to 1
  auto log = makeFollower("follower", LogTerm{5}, "leader");
  auto follower = log->getFollower();

  // insert three entries and set the commit index to 3, as well as the
  // lowest index to keep. After a call to release, the follower should compact.
  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogEntry = TermIndexPair{LogTerm{0}, LogIndex{0}};
    request.leaderCommit = LogIndex{3};
    request.lowestIndexToKeep = LogIndex{3};
    request.messageId = ++nextMessageId;
    request.entries = {InMemoryLogEntry(PersistingLogEntry(
                           LogTerm{1}, LogIndex{1},
                           LogPayload::createFromString("some payload"))),
                       InMemoryLogEntry(PersistingLogEntry(
                           LogTerm{1}, LogIndex{2},
                           LogPayload::createFromString("some payload"))),
                       InMemoryLogEntry(PersistingLogEntry(
                           LogTerm{1}, LogIndex{3},
                           LogPayload::createFromString("some payload")))};

    auto const compactedCountBefore =
        _logMetricsMock->replicatedLogNumberCompactedEntries->load();
    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    ASSERT_EQ(f.get().errorCode, TRI_ERROR_NO_ERROR)
        << f.get().reason.getErrorMessage();
    auto res = follower->release(LogIndex{2});  // now release index 2
    ASSERT_TRUE(res.ok());
    auto const compactedCountAfter =
        _logMetricsMock->replicatedLogNumberCompactedEntries->load();
    EXPECT_EQ(compactedCountAfter, compactedCountBefore + 2);
  }
}

TEST_F(ReplicatedLogMetricsTest, leader_count_compaction) {
  _optionsMock->_thresholdLogCompaction = 1;  // set compactions to 1
  auto coreA = makeLogCore(LogId{1});

  auto leaderId = ParticipantId{"leader"};
  auto leader = createLeaderWithDefaultFlags(leaderId, LogTerm{1},
                                             std::move(coreA), {}, 1);

  auto const beforeCompaction =
      _logMetricsMock->replicatedLogNumberCompactedEntries->load();
  leader->insert(LogPayload::createFromString("first"));
  auto idx = leader->insert(LogPayload::createFromString("second"));
  leader->insert(LogPayload::createFromString("third"));
  auto last = leader->insert(LogPayload::createFromString("forth"));
  auto f = leader->waitFor(last);
  ASSERT_TRUE(f.isReady());

  auto res = leader->release(idx);  // now trigger compaction
  ASSERT_TRUE(res.ok());
  auto const compactedCountAfter =
      _logMetricsMock->replicatedLogNumberCompactedEntries->load();
  EXPECT_EQ(compactedCountAfter, beforeCompaction + 3);
}

TEST_F(ReplicatedLogMetricsTest, leader_accept_commit_counter) {
  _optionsMock->_thresholdLogCompaction = 1;  // set compactions to 1
  auto coreA = makeLogCore(LogId{1});

  auto leaderId = ParticipantId{"leader"};
  auto leader = createLeaderWithDefaultFlags(leaderId, LogTerm{1},
                                             std::move(coreA), {}, 1);

  auto const beforeAccept =
      _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
  auto const beforeCommit =
      _logMetricsMock->replicatedLogNumberCommittedEntries->load();
  leader->insert(LogPayload::createFromString("first"));
  leader->insert(LogPayload::createFromString("second"));
  leader->insert(LogPayload::createFromString("third"));
  auto last = leader->insert(LogPayload::createFromString("forth"));
  auto f = leader->waitFor(last);
  ASSERT_TRUE(f.isReady());

  auto const afterAccept =
      _logMetricsMock->replicatedLogNumberAcceptedEntries->load();
  auto const afterCommit =
      _logMetricsMock->replicatedLogNumberCommittedEntries->load();
  EXPECT_EQ(afterAccept, beforeAccept + 4);  // we inserted 4
  EXPECT_EQ(afterCommit, beforeCommit + 5);  // but committed 5
}
