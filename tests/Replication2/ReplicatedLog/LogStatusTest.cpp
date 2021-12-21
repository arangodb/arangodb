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
/// @author Alex Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "TestHelper.h"

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

TEST_F(ReplicatedLogTest, quick_status_compare) {
  auto coreA = makeLogCore(LogId{1});
  auto coreB = makeLogCore(LogId{2});

  auto leaderId = ParticipantId{"leader"};
  auto followerId = ParticipantId{"follower"};

  auto follower = std::make_shared<DelayedFollowerLog>(
      defaultLogger(), _logMetricsMock, followerId, std::move(coreB),
      LogTerm{1}, leaderId);
  auto leader = LogLeader::construct(
      LoggerContext(Logger::REPLICATION2), _logMetricsMock, _optionsMock,
      leaderId, std::move(coreA), LogTerm{1},
      std::vector<std::shared_ptr<AbstractFollower>>{follower}, 2);

  leader->triggerAsyncReplication();
  follower->runAsyncAppendEntries();

  {
    auto quickStatus = leader->getQuickStatus();
    EXPECT_EQ(quickStatus.role, ParticipantRole::kLeader);
    EXPECT_EQ(quickStatus.getCurrentTerm(), LogTerm{1});
    auto status = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(quickStatus.getLocalStatistics(), status.local);
    EXPECT_EQ(*quickStatus.activeParticipantConfig,
              status.activeParticipantConfig);
    EXPECT_EQ(*quickStatus.committedParticipantConfig,
              status.committedParticipantConfig);
    EXPECT_TRUE(quickStatus.leadershipEstablished);
  }

  {
    auto quickStatus = follower->getQuickStatus();
    EXPECT_EQ(quickStatus.role, ParticipantRole::kFollower);
    EXPECT_EQ(quickStatus.getCurrentTerm(), LogTerm{1});
    auto status = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(quickStatus.getLocalStatistics(), status.local);

    EXPECT_EQ(quickStatus.activeParticipantConfig, nullptr);
    EXPECT_EQ(quickStatus.committedParticipantConfig, nullptr);
    EXPECT_FALSE(quickStatus.leadershipEstablished);
  }
}
