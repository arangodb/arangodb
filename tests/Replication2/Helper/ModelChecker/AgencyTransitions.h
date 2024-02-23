////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#pragma once
#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

namespace arangodb::test {

struct KillServerAction {
  void apply(AgencyState& agency);

  explicit KillServerAction(replication2::ParticipantId id);

  auto toString() const -> std::string;
  replication2::ParticipantId id;
};

struct SupervisionLogAction {
  void apply(AgencyState& agency);

  explicit SupervisionLogAction(replication2::replicated_log::Action action);
  auto toString() const -> std::string;

  replication2::replicated_log::Action _action;
};

struct DBServerSnapshotCompleteAction {
  explicit DBServerSnapshotCompleteAction(replication2::ParticipantId name);

  void apply(AgencyState& agency);
  auto toString() const -> std::string;

  replication2::ParticipantId name;
};

struct DBServerReportTermAction {
  explicit DBServerReportTermAction(replication2::ParticipantId name,
                                    replication2::LogTerm term);

  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;

  replication2::ParticipantId name;
  replication2::LogTerm term;
};

struct DBServerCommitConfigAction {
  explicit DBServerCommitConfigAction(replication2::ParticipantId name,
                                      std::size_t generation,
                                      replication2::LogTerm term);

  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;

  replication2::ParticipantId name;
  std::size_t generation;
  replication2::LogTerm term;
};

struct ReplaceServerTargetState {
  ReplaceServerTargetState(replication2::ParticipantId oldServer,
                           replication2::ParticipantId newServer);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  replication2::ParticipantId oldServer;
  replication2::ParticipantId newServer;
};

struct ReplaceServerTargetLog {
  ReplaceServerTargetLog(replication2::ParticipantId oldServer,
                         replication2::ParticipantId newServer);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  replication2::ParticipantId oldServer;
  replication2::ParticipantId newServer;
};

struct SetLeaderInTargetAction {
  SetLeaderInTargetAction(replication2::ParticipantId newLeader);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  replication2::ParticipantId newLeader;
};

struct SetWriteConcernAction {
  SetWriteConcernAction(size_t newWriteConcern);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  size_t newWriteConcern;
};

struct SetSoftWriteConcernAction {
  SetSoftWriteConcernAction(size_t newSoftWriteConcern);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  size_t newSoftWriteConcern;
};

struct SetBothWriteConcernAction {
  SetBothWriteConcernAction(size_t newWriteConcern, size_t newSoftWriteConcern);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  size_t newWriteConcern;
  size_t newSoftWriteConcern;
};

struct SetWaitForSyncAction {
  SetWaitForSyncAction(bool newWaitForSync);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  bool newWaitForSync;
};

struct AddLogParticipantAction {
  AddLogParticipantAction(replication2::ParticipantId server);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  replication2::ParticipantId server;
};

struct RemoveLogParticipantAction {
  RemoveLogParticipantAction(replication2::ParticipantId server);
  void apply(AgencyState& agency) const;
  auto toString() const -> std::string;
  replication2::ParticipantId server;
};

using AgencyTransition =
    std::variant<SupervisionLogAction, DBServerSnapshotCompleteAction,
                 DBServerReportTermAction, DBServerCommitConfigAction,
                 KillServerAction, ReplaceServerTargetState,
                 AddLogParticipantAction, SetLeaderInTargetAction,
                 RemoveLogParticipantAction, SetWriteConcernAction,
                 SetSoftWriteConcernAction, SetBothWriteConcernAction,
                 ReplaceServerTargetLog, SetWaitForSyncAction>;

auto operator<<(std::ostream& os, AgencyTransition const& a) -> std::ostream&;
}  // namespace arangodb::test
