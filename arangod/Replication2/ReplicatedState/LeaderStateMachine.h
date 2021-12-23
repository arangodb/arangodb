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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"
#include "Cluster/ClusterTypes.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

#include "Replication2/ReplicatedLog/LogCommon.h"

#include "Replication2/ReplicatedState/AgencySpecificationLog.h"
#include "Replication2/ReplicatedState/AgencySpecificationState.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

// We have
//  - ReplicatedLog
//  - ReplicatedState
//
//  ReplicatedState is built on top of ReplicatedLogs
//
// We have (in the agency)
//  - Current
//  - Plan
//  - Target
//  for each of ReplicatedLog and ReplicatedState
//
//  write down structs that reflect the necessary information
//  in the agency for everything involved here
//
//  Then write (a) function(s)
// auto replicatedStateAction(Log, State) -> Action;

namespace arangodb::replication2::replicated_state {

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state::agency;

struct ParticipantHealth {
  RebootId rebootId;
  bool isHealthy;
};

struct ParticipantsHealth {
  auto isHealthy(ParticipantId participant) const -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.isHealthy;
    }
    return false;
  };
  auto validRebootId(ParticipantId participant, RebootId rebootId) const
      -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.rebootId == rebootId;
    }
    return false;
  };

  std::unordered_map<ParticipantId, ParticipantHealth> _health;
};

struct LeaderElectionCampaign {
  enum class Reason { ServerIll, TermNotConfirmed, OK };

  std::unordered_map<ParticipantId, Reason> reasons;
  size_t numberOKParticipants{0};
  replication2::TermIndexPair bestTermIndex;
  std::vector<ParticipantId> electibleLeaderSet;

  void toVelocyPack(VPackBuilder& builder) const;
};
auto to_string(LeaderElectionCampaign::Reason reason) -> std::string_view;
auto operator<<(std::ostream& os, LeaderElectionCampaign::Reason reason) -> std::ostream&;

auto to_string(LeaderElectionCampaign const& campaign) -> std::string;
auto operator<<(std::ostream& os, LeaderElectionCampaign const& action) -> std::ostream&;

struct Action {
  enum class ActionType { UpdateTermAction, SuccessfulLeaderElectionAction, FailedLeaderElectionAction, ImpossibleCampaignAction };
  virtual void execute() = 0;
  virtual ActionType type() const = 0;
  virtual void toVelocyPack(VPackBuilder& builder) const = 0;
  virtual ~Action() = default;
};

auto to_string(Action::ActionType action) -> std::string_view;
auto operator<<(std::ostream& os, Action::ActionType const& action) -> std::ostream&;
auto operator<<(std::ostream& os, Action const& action) -> std::ostream&;

struct UpdateTermAction : Action {
  UpdateTermAction(agency::Log::Plan::TermSpecification const& newTerm)
      : _newTerm(newTerm){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::UpdateTermAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  agency::Log::Plan::TermSpecification _newTerm;
};

auto to_string(UpdateTermAction action) -> std::string;
auto operator<<(std::ostream& os, UpdateTermAction const& action) -> std::ostream&;

struct SuccessfulLeaderElectionAction : Action {
  SuccessfulLeaderElectionAction(){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::SuccessfulLeaderElectionAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  LeaderElectionCampaign _campaign;
  ParticipantId _newLeader;
  agency::Log::Plan::TermSpecification _newTerm;
};
auto to_string(SuccessfulLeaderElectionAction action) -> std::string;
auto operator<<(std::ostream& os, SuccessfulLeaderElectionAction const& action) -> std::ostream&;

struct FailedLeaderElectionAction : Action {
  FailedLeaderElectionAction(){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::FailedLeaderElectionAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  LeaderElectionCampaign _campaign;
};
auto to_string(FailedLeaderElectionAction const& action) -> std::string;
auto operator<<(std::ostream& os, FailedLeaderElectionAction const& action) -> std::ostream&;


struct ImpossibleCampaignAction : Action {
  ImpossibleCampaignAction(){};
  void execute() override{};
  ActionType type() const override { return Action::ActionType::ImpossibleCampaignAction; };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(ImpossibleCampaignAction const& action) -> std::string;
auto operator<<(std::ostream& os, ImpossibleCampaignAction const& action) -> std::ostream&;



auto computeReason(agency::Log::Current::LocalState const& status, bool healthy,
                   LogTerm term) -> LeaderElectionCampaign::Reason;

auto runElectionCampaign(agency::Log::Current::LocalStates const& states,
                         ParticipantsHealth const& health, LogTerm term)
    -> LeaderElectionCampaign;

auto replicatedLogAction(Log const&, ParticipantsHealth const&)
    -> std::unique_ptr<Action>;

}  // namespace arangodb::replication2::replicated_state
