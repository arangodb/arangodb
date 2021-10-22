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

#include "Replication2/Mocks/FakeReplicatedLog.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "TestHelper.h"

#include <Containers/Enumerate.h>
#include <gtest/gtest.h>
#include <optional>

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogLeader.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct AppendEntriesBatchTest
    : ReplicatedLogTest,
      ::testing::WithParamInterface<std::tuple<ReplicatedLogGlobalSettings, std::vector<LogPayload>>> {
  AppendEntriesBatchTest() : _payloads(std::get<1>(GetParam())) {
    *_optionsMock = std::get<0>(GetParam());
  }

  std::vector<LogPayload> const _payloads{};
};

TEST_P(AppendEntriesBatchTest, test_with_sized_batches) {
  auto expectedNumRequests = size_t{0};
  // AppendEntries CommitIndex
  expectedNumRequests++;
  // AppendEntries LCI
  expectedNumRequests++;

  // If the payload has more than one entry, decrement lastAckedIndex to 0
  if (_payloads.size() > 1) {
    expectedNumRequests += 1;
  }

  auto leaderLog = std::invoke([&] {
    auto persistedLog = makePersistedLog(LogId{1});

    for (auto [idx, payload] : enumerate(_payloads)) {
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
      if (currentSize >= _optionsMock->_maxNetworkBatchSize) {
        numRequests += 1;
        currentSize = 0;
      }
    }
    // Some pending entries still need to be submitted
    if (currentSize > 0) {
      numRequests += 1;
    }

    expectedNumRequests += numRequests;

    return std::make_shared<TestReplicatedLog>(std::make_unique<LogCore>(persistedLog),
                                               _logMetricsMock, _optionsMock,
                                               LoggerContext(Logger::REPLICATION2));
  });

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  {
    if (_payloads.size() > 0) {
      auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
      EXPECT_EQ(stats.local.spearHead,
                TermIndexPair(LogTerm{4}, LogIndex{_payloads.size()}));
      EXPECT_EQ(stats.local.commitIndex, LogIndex{0});

      EXPECT_EQ(stats.follower.at("follower").spearHead.index,
                LogIndex{_payloads.size() - 1});
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
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{_payloads.size()});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{_payloads.size()});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{_payloads.size()});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{_payloads.size()});
  }
}

auto testReplicatedLogOptions =
    testing::Values(ReplicatedLogGlobalSettings{5, 5}, ReplicatedLogGlobalSettings{1024, 1024},
                    ReplicatedLogGlobalSettings{1024 * 1024, 1024 * 1024});

auto testPayloads = testing::Values(
    std::vector<LogPayload>{LogPayload::createFromString("a")},
    std::vector<LogPayload>{LogPayload::createFromString("a"),
                            LogPayload::createFromString("b")},
    std::vector<LogPayload>{LogPayload::createFromString(std::string(1024, 'a'))},
    std::vector<LogPayload>{LogPayload::createFromString("Hello, world"),
                            LogPayload::createFromString("Bye, world")},
    std::vector<LogPayload>(1024, LogPayload::createFromString("")),
    std::vector<LogPayload>{LogPayload::createFromString(std::string(1024, 'a')),
                            LogPayload::createFromString("Hello, world"),
                            LogPayload::createFromString("Bye, world")},
    std::vector<LogPayload>(1024, LogPayload::createFromString(std::string(1024, 'a'))));

// Use a sequence of payload of pre-defined sizes and send them;
// count the number of batches that are created.
INSTANTIATE_TEST_CASE_P(AppendEntriesBatchTestInstance, AppendEntriesBatchTest,
                        testing::Combine(testReplicatedLogOptions, testPayloads));
