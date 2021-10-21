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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "TestHelper.h"


#include <Containers/Enumerate.h>
#include <optional>

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogLeader.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct AppendEntriesBatchTest : ReplicatedLogTest {};

TEST_F(AppendEntriesBatchTest, test_with_two_batches) {
  // make batch size small enough to force more
  // than one batch to be sent
  _optionsMock->_maxNetworkBatchSize = 60000;

  auto leaderLog = std::invoke([&] {
    auto persistedLog = makePersistedLog(LogId{1});
    for (size_t i = 1; i <= 2000; i++) {
      persistedLog->setEntry(LogIndex{i}, LogTerm{4}, LogPayload::createFromString("log entry"));
    }
    return std::make_shared<TestReplicatedLog>(std::make_unique<LogCore>(persistedLog),
                                               _logMetricsMock, _optionsMock,
                                               LoggerContext(Logger::REPLICATION2));
  });

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead, TermIndexPair(LogTerm{4}, LogIndex{2000}));
    EXPECT_EQ(stats.local.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.follower.at("follower").spearHead.index, LogIndex{1999});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{0});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{0});
  }

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingAppendEntries());

  {
    std::size_t num_requests = 0;
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
      num_requests += 1;
    }

    // 0. Decrement lastAckedIndex to 0
    // 1. AppendEntries 1..1000
    // 2. AppendEntries 2..2000
    // 3. AppendEntries CommitIndex
    // 4. AppendEntries LCI
    EXPECT_EQ(num_requests, 3 + 1 + 1);
  }

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{2000});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{2000});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{2000});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{2000});
  }
}

// Use a sequence of payload of pre-defined sizes and send them;
// count the number of batches that are created.
TEST_F(AppendEntriesBatchTest, test_with_sized_batches) {
  // make batch size small enough to force more
  // than one batch to be sent
  _optionsMock->_maxNetworkBatchSize = 1024;

  auto test_payloads = {
      std::vector<LogPayload>{LogPayload::createFromString("a")},
      std::vector<LogPayload>{LogPayload::createFromString("a"), LogPayload::createFromString("b")},
      std::vector<LogPayload>{LogPayload::createFromString(std::string(1024, 'a'))},
      std::vector<LogPayload>{LogPayload::createFromString("Hello, world"),
                              LogPayload::createFromString("Bye, world")},
      std::vector<LogPayload>(1024, LogPayload::createFromString("")),
      std::vector<LogPayload>{LogPayload::createFromString(std::string(1024, 'a')),
                              LogPayload::createFromString("Hello, world"),
                              LogPayload::createFromString("Bye, world")},
      std::vector<LogPayload>(1024, LogPayload::createFromString(std::string(1024, 'a')))
    };

  for (auto payloads : test_payloads) {
    auto expectedNumRequests = size_t{0};
    // AppendEntries CommitIndex
    expectedNumRequests++;
    // AppendEntries LCI
    expectedNumRequests++;

    // If the payload has more than one entry, decrement lastAckedIndex to 0
    if(payloads.size() > 1) {
      expectedNumRequests += 1;
    }

    auto leaderLog = std::invoke([&] {
      auto persistedLog = makePersistedLog(LogId{1});

      for (auto [idx, payload] : enumerate(payloads)) {
        persistedLog->setEntry(LogIndex{idx + 1}, LogTerm{4}, payload);
      }

      /*
       * Compute the number of requests we expect to be generating;
       * this is slightly dissatisfying as we're re-implementing the
       * algorithm used for batching, but there is no closed formula
       * for this
       */
      auto it = persistedLog->read(LogIndex{1});
      auto numRequests = size_t{0};
      auto currentSize = size_t{0};
      while (auto log = it->next()) {
        currentSize += log->approxByteSize();
        if(currentSize >= _optionsMock->_maxNetworkBatchSize) {
          numRequests += 1;
          currentSize = 0;
        }
      }
      // Some pending entries still need to be submitted
      if(currentSize > 0) {
        numRequests += 1;
      }

      expectedNumRequests += numRequests;

      return std::make_shared<TestReplicatedLog>(std::make_unique<LogCore>(persistedLog),
                                                 _logMetricsMock, _optionsMock,
                                                 LoggerContext(Logger::REPLICATION2));
    });

    auto followerLog = makeReplicatedLog(LogId{1});
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{4}, "leader");
    auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);



    {
      if(payloads.size() > 0) {
        auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
        EXPECT_EQ(stats.local.spearHead,
                  TermIndexPair(LogTerm{4}, LogIndex{payloads.size()}));
        EXPECT_EQ(stats.local.commitIndex, LogIndex{0});

        EXPECT_EQ(stats.follower.at("follower").spearHead.index,
                  LogIndex{payloads.size() - 1});
      } else {
        // TODO:
      }
    }
    {
      auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
      EXPECT_EQ(stats.local.spearHead.index, LogIndex{0});
      EXPECT_EQ(stats.local.commitIndex, LogIndex{0});
    }

    leader->triggerAsyncReplication();
    ASSERT_TRUE(follower->hasPendingAppendEntries());

    {
      std::size_t num_requests = 0;
      while (follower->hasPendingAppendEntries()) {
        follower->runAsyncAppendEntries();
        num_requests += 1;
      }

      EXPECT_EQ(num_requests, expectedNumRequests);
    }

    {
      auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
      EXPECT_EQ(stats.local.spearHead.index, LogIndex{payloads.size()});
      EXPECT_EQ(stats.local.commitIndex, LogIndex{payloads.size()});
    }
    {
      auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
      EXPECT_EQ(stats.local.spearHead.index, LogIndex{payloads.size()});
      EXPECT_EQ(stats.local.commitIndex, LogIndex{payloads.size()});
    }
  }
}
