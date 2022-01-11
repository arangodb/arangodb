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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/SupervisionTypes.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct Action {
  enum class ActionType {
    EmptyAction,
    AddLogToPlanAction,
    CreateInitialTermAction,
    UpdateTermAction,
    SuccessfulLeaderElectionAction,
    FailedLeaderElectionAction,
    ImpossibleCampaignAction,
    UpdateParticipantFlagsAction,
    AddParticipantToPlanAction,
    RemoveParticipantFromPlanAction,
    UpdateLogConfigAction
  };
  virtual auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope = 0;

  virtual ActionType type() const = 0;
  virtual void toVelocyPack(VPackBuilder& builder) const = 0;
  virtual ~Action() = default;
};

auto to_string(Action::ActionType action) -> std::string_view;
auto operator<<(std::ostream& os, Action::ActionType const& action)
    -> std::ostream&;
auto operator<<(std::ostream& os, Action const& action) -> std::ostream&;

// Empty Action
// TODO: we currently use a mix of nullptr and EmptyAction; should only use one
// of them.
struct EmptyAction : Action {
  EmptyAction(){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override { return Action::ActionType::EmptyAction; };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(EmptyAction action) -> std::string;
auto operator<<(std::ostream& os, EmptyAction const& action) -> std::ostream&;

// AddLogToPlanAction
struct AddLogToPlanAction : Action {
  AddLogToPlanAction(LogPlanSpecification const& spec) : _spec(spec){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::AddLogToPlanAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogPlanSpecification const _spec;
};
auto to_string(AddLogToPlanAction const& action) -> std::string;
auto operator<<(std::ostream& os, AddLogToPlanAction const& action)
    -> std::ostream&;

struct CreateInitialTermAction : Action {
  CreateInitialTermAction(){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override {
    return Action::ActionType::CreateInitialTermAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;
};

struct UpdateTermAction : Action {
  UpdateTermAction(LogPlanTermSpecification const& newTerm)
      : _newTerm(newTerm){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override {
    return Action::ActionType::UpdateTermAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogPlanTermSpecification _newTerm;
};

auto to_string(UpdateTermAction action) -> std::string;
auto operator<<(std::ostream& os, UpdateTermAction const& action)
    -> std::ostream&;

struct LeaderElectionCampaign;

struct SuccessfulLeaderElectionAction : Action {
  SuccessfulLeaderElectionAction(){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override {
    return Action::ActionType::SuccessfulLeaderElectionAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LeaderElectionCampaign _campaign;
  ParticipantId _newLeader;
  LogPlanTermSpecification _newTerm;
};
auto to_string(SuccessfulLeaderElectionAction action) -> std::string;
auto operator<<(std::ostream& os, SuccessfulLeaderElectionAction const& action)
    -> std::ostream&;

struct FailedLeaderElectionAction : Action {
  FailedLeaderElectionAction(){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override {
    return Action::ActionType::FailedLeaderElectionAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LeaderElectionCampaign _campaign;
};
auto to_string(FailedLeaderElectionAction const& action) -> std::string;
auto operator<<(std::ostream& os, FailedLeaderElectionAction const& action)
    -> std::ostream&;

struct ImpossibleCampaignAction : Action {
  ImpossibleCampaignAction(){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override {
    return Action::ActionType::ImpossibleCampaignAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(ImpossibleCampaignAction const& action) -> std::string;
auto operator<<(std::ostream& os, ImpossibleCampaignAction const& action)
    -> std::ostream&;

struct UpdateParticipantFlagsAction : Action {
  UpdateParticipantFlagsAction(ParticipantId const& participant,
                               ParticipantFlags const& flags)
      : _participant(participant), _flags(flags){};
  ActionType type() const override {
    return Action::ActionType::UpdateParticipantFlagsAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  ParticipantId _participant;
  ParticipantFlags _flags;
};

struct AddParticipantToPlanAction : Action {
  AddParticipantToPlanAction(ParticipantId const& participant,
                             ParticipantFlags const& flags)
      : _participant(participant), _flags(flags){};
  ActionType type() const override {
    return Action::ActionType::AddParticipantToPlanAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  ParticipantId _participant;
  ParticipantFlags _flags;
};

struct RemoveParticipantFromPlanAction : Action {
  RemoveParticipantFromPlanAction(ParticipantId const& participant)
      : _participant(participant){};
  ActionType type() const override {
    return Action::ActionType::RemoveParticipantFromPlanAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  ParticipantId _participant;
};

struct UpdateLogConfigAction : Action {
  UpdateLogConfigAction(LogConfig const& config) : _config(config){};
  ActionType type() const override {
    return Action::ActionType::UpdateLogConfigAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogConfig _config;
};

}  // namespace arangodb::replication2::replicated_log
