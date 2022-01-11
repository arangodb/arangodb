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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/SupervisionTypes.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct Action {
  enum class ActionType {
    EmptyAction,
    AddLogToPlanAction,
    UpdateTermAction,
    SuccessfulLeaderElectionAction,
    FailedLeaderElectionAction,
    ImpossibleCampaignAction,
    UpdateParticipantFlagsAction,
    AddParticipantToPlanAction,
    RemoveParticipantFromPlanAction,
    UpdateLogConfigAction
  };
  virtual void execute() = 0;
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
  void execute() override{};
  ActionType type() const override { return Action::ActionType::EmptyAction; };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(EmptyAction action) -> std::string;
auto operator<<(std::ostream& os, EmptyAction const& action) -> std::ostream&;

// AddLogToPlanAction
struct AddLogToPlanAction : Action {
  AddLogToPlanAction(){};
  void execute() override{};
  ActionType type() const override {
    return Action::ActionType::AddLogToPlanAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(AddLogToPlanAction const& action) -> std::string;
auto operator<<(std::ostream& os, AddLogToPlanAction const& action)
    -> std::ostream&;

struct UpdateTermAction : Action {
  UpdateTermAction(LogPlanTermSpecification const& newTerm)
      : _newTerm(newTerm){};
  void execute() override{};
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
  void execute() override{};
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
  void execute() override{};
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
  void execute() override{};
  ActionType type() const override {
    return Action::ActionType::ImpossibleCampaignAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;
};
auto to_string(ImpossibleCampaignAction const& action) -> std::string;
auto operator<<(std::ostream& os, ImpossibleCampaignAction const& action)
    -> std::ostream&;

struct UpdateParticipantFlagsAction : Action {
  UpdateParticipantFlagsAction(){};
  ActionType type() const override {
    return Action::ActionType::UpdateParticipantFlagsAction;
  };

  void execute() override{};
  void toVelocyPack(VPackBuilder& builder) const override;
};

struct AddParticipantToPlanAction : Action {
  AddParticipantToPlanAction(){};
  ActionType type() const override {
    return Action::ActionType::AddParticipantToPlanAction;
  };

  void execute() override{};
  void toVelocyPack(VPackBuilder& builder) const override;
};

struct RemoveParticipantFromPlanAction : Action {
  RemoveParticipantFromPlanAction(){};
  ActionType type() const override {
    return Action::ActionType::RemoveParticipantFromPlanAction;
  };

  void execute() override{};
  void toVelocyPack(VPackBuilder& builder) const override;
};

struct UpdateLogConfigAction : Action {
  UpdateLogConfigAction(){};
  ActionType type() const override {
    return Action::ActionType::UpdateLogConfigAction;
  };

  void execute() override{};
  void toVelocyPack(VPackBuilder& builder) const override;
};

}  // namespace arangodb::replication2::replicated_log
