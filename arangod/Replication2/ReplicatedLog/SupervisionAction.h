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

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct Action {
  enum class ActionType {
    EmptyAction,
    ErrorAction,
    AddLogToPlanAction,
    AddParticipantsToTargetAction,
    CreateInitialTermAction,
    UpdateTermAction,
    DictateLeaderAction,
    EvictLeaderAction,
    LeaderElectionAction,
    UpdateParticipantFlagsAction,
    AddParticipantToPlanAction,
    RemoveParticipantFromPlanAction,
    UpdateLogConfigAction,
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
  EmptyAction() : _message(""){};
  EmptyAction(std::string_view message) : _message(message){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override {
    return envelope;
  };
  ActionType type() const override { return Action::ActionType::EmptyAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  std::string _message;
};
auto to_string(EmptyAction action) -> std::string;
auto operator<<(std::ostream& os, EmptyAction const& action) -> std::ostream&;

struct ErrorAction : Action {
  ErrorAction(LogId const& id, LogCurrentSupervisionError const& error)
      : _id{id}, _error{error} {};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override { return Action::ActionType::ErrorAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId _id;
  LogCurrentSupervisionError _error;
};
auto to_string(ErrorAction action) -> std::string;
auto operator<<(std::ostream& os, ErrorAction const& action) -> std::ostream&;

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

// AddParticipantsToTarget
struct AddParticipantsToTargetAction : Action {
  AddParticipantsToTargetAction(LogTarget const& spec) : _spec(spec){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::AddParticipantsToTargetAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogTarget const _spec;
};
auto to_string(AddParticipantsToTargetAction const& action) -> std::string;
auto operator<<(std::ostream& os, AddParticipantsToTargetAction const& action)
    -> std::ostream&;

struct CreateInitialTermAction : Action {
  CreateInitialTermAction(LogId id, LogPlanTermSpecification const& term)
      : _id(id), _term(term){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::CreateInitialTermAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  LogPlanTermSpecification const _term;
};

struct DictateLeaderAction : Action {
  DictateLeaderAction(LogId const& id, LogPlanTermSpecification const& newTerm)
      : _id(id), _term{newTerm} {};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::DictateLeaderAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  LogPlanTermSpecification _term;
};

struct EvictLeaderAction : Action {
  EvictLeaderAction(LogId const& id, ParticipantId const& leader,
                    ParticipantFlags const& flags,
                    LogPlanTermSpecification const& newTerm,
                    std::size_t generation)
      : _id(id),
        _leader{leader},
        _flags{flags},
        _newTerm{newTerm},
        _generation{generation} {};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::EvictLeaderAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  ParticipantId _leader;
  ParticipantFlags _flags;
  LogPlanTermSpecification _newTerm;
  std::size_t _generation;
};

struct UpdateTermAction : Action {
  UpdateTermAction(LogId const& id, LogPlanTermSpecification const& newTerm)
      : _id{id}, _newTerm(newTerm){};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::UpdateTermAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  LogPlanTermSpecification _newTerm;
};

auto to_string(UpdateTermAction action) -> std::string;
auto operator<<(std::ostream& os, UpdateTermAction const& action)
    -> std::ostream&;

struct LeaderElectionAction : Action {
  LeaderElectionAction(LogId id, LogCurrentSupervisionElection const& election)
      : _id(id), _election{election}, _newTerm{std::nullopt} {};
  LeaderElectionAction(LogId id, LogCurrentSupervisionElection const& election,
                       LogPlanTermSpecification newTerm)
      : _id(id), _election{election}, _newTerm{newTerm} {};
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  ActionType type() const override {
    return Action::ActionType::LeaderElectionAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  LogCurrentSupervisionElection _election;
  std::optional<LogPlanTermSpecification> _newTerm;
};
auto to_string(LeaderElectionAction action) -> std::string;
auto operator<<(std::ostream& os, LeaderElectionAction const& action)
    -> std::ostream&;

struct UpdateParticipantFlagsAction : Action {
  UpdateParticipantFlagsAction(LogId id, ParticipantId const& participant,
                               ParticipantFlags const& flags,
                               std::size_t generation)
      : _id(id),
        _participant(participant),
        _flags(flags),
        _generation{generation} {};
  ActionType type() const override {
    return Action::ActionType::UpdateParticipantFlagsAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  ParticipantId _participant;
  ParticipantFlags _flags;
  std::size_t _generation;
};

struct AddParticipantToPlanAction : Action {
  AddParticipantToPlanAction(LogId id, ParticipantId const& participant,
                             ParticipantFlags const& flags,
                             std::size_t generation)
      : _id{id},
        _participant(participant),
        _flags(flags),
        _generation{generation} {};
  ActionType type() const override {
    return Action::ActionType::AddParticipantToPlanAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  ParticipantId _participant;
  ParticipantFlags _flags;
  std::size_t _generation;
};

struct RemoveParticipantFromPlanAction : Action {
  RemoveParticipantFromPlanAction(LogId const& id,
                                  ParticipantId const& participant,
                                  std::size_t generation)
      : _id{id}, _participant(participant), _generation{generation} {};
  ActionType type() const override {
    return Action::ActionType::RemoveParticipantFromPlanAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  ParticipantId _participant;
  std::size_t _generation;
};

struct UpdateLogConfigAction : Action {
  UpdateLogConfigAction(LogId const& id, LogConfig const& config)
      : _id(id), _config(config){};
  ActionType type() const override {
    return Action::ActionType::UpdateLogConfigAction;
  };

  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;
  void toVelocyPack(VPackBuilder& builder) const override;

  LogId const _id;
  LogConfig _config;
};

}  // namespace arangodb::replication2::replicated_log
