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
#include <gtest/gtest.h>

#include "TestHelper.h"

#include "Basics/voc-errors.h"

#include <Replication2/ReplicatedLog/Algorithms.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

namespace {

struct FakeAbstractFollower : replicated_log::AbstractFollower {
  FakeAbstractFollower(LogId logId, ParticipantId id)
      : logId(logId), id(std::move(id)) {}

  auto getParticipantId() const noexcept -> ParticipantId const& override {
    return id;
  }

  auto appendEntries(AppendEntriesRequest req)
      -> futures::Future<AppendEntriesResult> override {
    // Silently ignore
    return {AppendEntriesResult::withOk(req.leaderTerm, req.messageId)};
  }

  LogId logId;
  ParticipantId id;
};

struct ReplicationMaintenanceActionTest : ReplicatedLogTest, algorithms::LogActionContext {

  auto dropReplicatedLog(LogId id) -> Result final {
    if (auto it = logs.find(id); it != logs.end()) {
      logs.erase(it);
      return {};
    } else {
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }
  }

  auto ensureReplicatedLog(LogId id) -> std::shared_ptr<replicated_log::ReplicatedLog> final {
    if (auto it = logs.find(id); it != logs.end()) {
      return it->second;
    }

    return logs.emplace(id, makeReplicatedLog(id)).first->second;
  }

  auto buildAbstractFollowerImpl(LogId id, ParticipantId participantId)
      -> std::shared_ptr<replication2::replicated_log::AbstractFollower> final {
    return std::make_shared<FakeAbstractFollower>(id, std::move(participantId));
  }

  std::unordered_map<LogId, std::shared_ptr<TestReplicatedLog>> logs;
};

}

TEST_F(ReplicationMaintenanceActionTest, drop_replicated_log) {
  auto const logId = LogId{12};
  algorithms::updateReplicatedLog(*this, ParticipantId{"A"}, RebootId{17}, logId, nullptr);

  ASSERT_EQ(logs.size(), 0);
}

TEST_F(ReplicationMaintenanceActionTest, create_replicated_log) {
  auto const logId = LogId{12};
  auto const serverId = ParticipantId{"A"};

  agency::LogPlanSpecification spec;
  spec.id = logId;
  spec.currentTerm = agency::LogPlanTermSpecification{};
  spec.currentTerm->term = LogTerm{8};
  spec.currentTerm->participants[serverId] = {};

  algorithms::updateReplicatedLog(*this, serverId, RebootId{17}, logId, &spec);

  ASSERT_EQ(logs.size(), 1);
  auto& log = logs.at(logId);
  EXPECT_EQ(log->getParticipant()->getTerm().value(), LogTerm{8});
}

TEST_F(ReplicationMaintenanceActionTest, create_replicated_log_leader) {
  auto const logId = LogId{12};
  auto const serverId = ParticipantId{"A"};

  agency::LogPlanSpecification spec;
  spec.id = logId;
  spec.currentTerm = agency::LogPlanTermSpecification{};
  spec.currentTerm->term = LogTerm{8};
  spec.currentTerm->participants[serverId] = {};
  spec.currentTerm->leader = agency::LogPlanTermSpecification::Leader{serverId, RebootId{17}};

  algorithms::updateReplicatedLog(*this, serverId, RebootId{17}, logId, &spec);

  ASSERT_EQ(logs.size(), 1);
  auto& log = logs.at(logId);
  EXPECT_EQ(log->getParticipant()->getTerm().value(), LogTerm{8});
  auto status = std::get<LeaderStatus>(log->getParticipant()->getStatus().getVariant());
  EXPECT_EQ(status.follower.size(), 1);
  EXPECT_NE(status.follower.find(serverId), status.follower.end());
}

TEST_F(ReplicationMaintenanceActionTest, create_replicated_log_leader_wrong_reboot_id) {
  auto const logId = LogId{12};
  auto const serverId = ParticipantId{"A"};

  agency::LogPlanSpecification spec;
  spec.id = logId;
  spec.currentTerm = agency::LogPlanTermSpecification{};
  spec.currentTerm->term = LogTerm{8};
  spec.currentTerm->participants[serverId] = {};
  spec.currentTerm->leader = agency::LogPlanTermSpecification::Leader{serverId, RebootId{18}};

  algorithms::updateReplicatedLog(*this, serverId, RebootId{17}, logId, &spec);

  ASSERT_EQ(logs.size(), 1);
  auto& log = logs.at(logId);
  EXPECT_EQ(log->getParticipant()->getTerm().value(), LogTerm{8});
  EXPECT_TRUE(std::holds_alternative<FollowerStatus>(log->getParticipant()->getStatus().getVariant()));
}

TEST_F(ReplicationMaintenanceActionTest, create_replicated_log_leader_with_follower) {
  auto const logId = LogId{12};
  auto const serverId = ParticipantId{"A"};
  auto const followerId = ParticipantId{"B"};

  agency::LogPlanSpecification spec;
  spec.id = logId;
  spec.currentTerm = agency::LogPlanTermSpecification{};
  spec.currentTerm->term = LogTerm{8};
  spec.currentTerm->participants[serverId] = {};
  spec.currentTerm->participants[followerId] = {};
  spec.currentTerm->leader = agency::LogPlanTermSpecification::Leader{serverId, RebootId{17}};

  algorithms::updateReplicatedLog(*this, serverId, RebootId{17}, logId, &spec);

  ASSERT_EQ(logs.size(), 1);
  auto& log = logs.at(logId);
  EXPECT_EQ(log->getParticipant()->getTerm().value(), LogTerm{8});
  auto status = std::get<LeaderStatus>(log->getParticipant()->getStatus().getVariant());
  EXPECT_EQ(status.follower.size(), 2);
  EXPECT_NE(status.follower.find(serverId), status.follower.end());
  EXPECT_NE(status.follower.find(followerId), status.follower.end());
}

