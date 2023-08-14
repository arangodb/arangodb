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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/Helper/ReplicatedLogTestSetup.h"
#include "Replication2/Mocks/DelayedLogFollower.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

#include <Containers/Enumerate.h>
#include <gtest/gtest.h>
#include <optional>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct AppendEntriesBatchTest
    : ReplicatedLogTest,
      ::testing::WithParamInterface<
          std::tuple<ReplicatedLogGlobalSettings, std::vector<LogPayload>>> {
  AppendEntriesBatchTest()
      : _settings(std::make_shared<ReplicatedLogGlobalSettings>(
            std::get<0>(GetParam()))),
        _payloads(std::get<1>(GetParam())) {}

  std::shared_ptr<ReplicatedLogGlobalSettings> _settings{};
  std::vector<LogPayload> const _payloads{};
};

TEST_P(AppendEntriesBatchTest, test_with_sized_batches) {
  auto expectedNumRequests = size_t{0};
  // AppendEntries CommitIndex
  expectedNumRequests++;
  // AppendEntries LCI
  expectedNumRequests++;

  auto leaderLogContainer =
      makeLogWithFakes({.initialLogRange = _payloads, .options = _settings});

  {
    /*
     * Compute the number of requests we expect to be generating;
     * this is slightly dissatisfying as we're re-implementing the
     * algorithm used for batching, but there is no closed formula
     * for this
     */
    auto numRequests = size_t{0};
    auto currentSize = size_t{0};
    for (auto const& [idx, logEntry] : leaderLogContainer.storageContext->log) {
      currentSize += logEntry.approxByteSize();
      if (currentSize >= _settings->_thresholdNetworkBatchSize) {
        numRequests += 1;
        currentSize = 0;
      }
    }
    {
      // Add first entry in term
      currentSize +=
          LogEntry{TermIndexPair{LogTerm{5}, LogIndex{1}}, LogMetaPayload{}}
              .approxByteSize();
      if (currentSize >= _settings->_thresholdNetworkBatchSize) {
        numRequests += 1;
        currentSize = 0;
      }
    }
    // Some pending entries still need to be submitted
    if (currentSize > 0) {
      numRequests += 1;
    }

    expectedNumRequests += numRequests;
  }

  auto followerLogContainer = makeLogWithFakes({});

  auto config = makeConfig(leaderLogContainer, {followerLogContainer},
                           {.term = 5_T, .writeConcern = 2});
  config.installConfig(false);
  EXPECT_CALL(*followerLogContainer.stateHandleMock, updateCommitIndex)
      .Times(testing::AtLeast(1));

  auto leader = leaderLogContainer.log;
  auto follower = followerLogContainer.log;

  {
    if (_payloads.size() > 0) {
      auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
      EXPECT_EQ(stats.local.spearHead,
                TermIndexPair(LogTerm{5}, LogIndex{_payloads.size() + 1}));
      EXPECT_EQ(stats.local.commitIndex, LogIndex{0});

      EXPECT_EQ(stats.follower.at(followerLogContainer.serverInstance.serverId)
                    .nextPrevLogIndex,
                LogIndex{_payloads.size()});
    } else {
      auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
      EXPECT_EQ(stats.local.spearHead,
                TermIndexPair(LogTerm{5}, LogIndex{_payloads.size() + 1}));
      EXPECT_EQ(stats.local.commitIndex, LogIndex{0});

      EXPECT_EQ(stats.follower.at(followerLogContainer.serverInstance.serverId)
                    .nextPrevLogIndex,
                LogIndex{_payloads.size()});
    }
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{0});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{0});
  }

  leaderLogContainer.runAll();
  ASSERT_TRUE(
      followerLogContainer.delayedLogFollower->hasPendingAppendEntries());

  {
    // TODO Instead of this loop, maybe have the DelayedLogFollower track/count
    //      all requests, and just "runAll" on leader+follower.
    std::size_t num_requests = 0;
    while (followerLogContainer.delayedLogFollower->hasPendingAppendEntries()) {
      followerLogContainer.runAll();
      num_requests += 1;
      leaderLogContainer.runAll();
    }

    EXPECT_EQ(num_requests, expectedNumRequests);
  }

  ASSERT_NE(leaderLogContainer.stateHandleMock->logLeaderMethods, nullptr);

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{_payloads.size() + 1});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{_payloads.size() + 1});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{_payloads.size() + 1});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{_payloads.size() + 1});
  }
}

auto testReplicatedLogOptions = testing::Values(
    ReplicatedLogGlobalSettings{5, 5}, ReplicatedLogGlobalSettings{1024, 1024},
    ReplicatedLogGlobalSettings{1024 * 1024, 1024 * 1024});

auto testPayloads = testing::Values(
    std::vector<LogPayload>{LogPayload::createFromString("a")},
    std::vector<LogPayload>{LogPayload::createFromString("a"),
                            LogPayload::createFromString("b")},
    std::vector<LogPayload>{
        LogPayload::createFromString(std::string(1024, 'a'))},
    std::vector<LogPayload>{LogPayload::createFromString("Hello, world"),
                            LogPayload::createFromString("Bye, world")},
    std::vector<LogPayload>(1024, LogPayload::createFromString("")),
    std::vector<LogPayload>{
        LogPayload::createFromString(std::string(1024, 'a')),
        LogPayload::createFromString("Hello, world"),
        LogPayload::createFromString("Bye, world")},
    std::vector<LogPayload>(
        1024, LogPayload::createFromString(std::string(1024, 'a'))));

// Use a sequence of payload of pre-defined sizes and send them;
// count the number of batches that are created.
INSTANTIATE_TEST_CASE_P(AppendEntriesBatchTestInstance, AppendEntriesBatchTest,
                        testing::Combine(testReplicatedLogOptions,
                                         testPayloads));
