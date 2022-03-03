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

#include <memory>
#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct EmptyAction {
  static constexpr std::string_view name = "EmptyAction";

  EmptyAction() : _message(""){};
  EmptyAction(std::string_view message) : _message(message){};
  std::string _message;
};

struct ErrorAction {
  static constexpr std::string_view name = "ErrorAction";

  ErrorAction(LogCurrentSupervisionError const& error) : _error{error} {};

  LogCurrentSupervisionError _error;
};

struct AddLogToPlanAction {
  static constexpr std::string_view name = "AddLogToPlanAction";

  AddLogToPlanAction(LogPlanSpecification const& spec) : _spec(spec){};
  LogPlanSpecification const _spec;
};

struct AddParticipantsToTargetAction {
  static constexpr std::string_view name = "AddParticipantsToTargetAction";

  AddParticipantsToTargetAction(LogTarget const& spec) : _spec(spec){};
  LogTarget const _spec;
};

struct CreateInitialTermAction {
  static constexpr std::string_view name = "CreateIntialTermAction";

  CreateInitialTermAction(LogPlanTermSpecification const& term) : _term(term){};

  LogPlanTermSpecification const _term;
};

struct DictateLeaderAction {
  static constexpr std::string_view name = "DictateLeaderAction";

  DictateLeaderAction(LogPlanTermSpecification const& newTerm)
      : _term{newTerm} {};

  LogPlanTermSpecification _term;
};

struct EvictLeaderAction {
  static constexpr std::string_view name = "EvictLeaderAction";

  EvictLeaderAction(ParticipantId const& leader, ParticipantFlags const& flags,
                    LogPlanTermSpecification const& newTerm,
                    std::size_t generation)
      : _leader{leader},
        _flags{flags},
        _newTerm{newTerm},
        _generation{generation} {};

  ParticipantId _leader;
  ParticipantFlags _flags;
  LogPlanTermSpecification _newTerm;
  std::size_t _generation;
};

struct UpdateTermAction {
  static constexpr std::string_view name = "UpdateTermAction";

  UpdateTermAction(LogPlanTermSpecification const& newTerm)
      : _newTerm(newTerm){};

  LogPlanTermSpecification _newTerm;
};

struct LeaderElectionAction {
  static constexpr std::string_view name = "LeaderElectionAction";

  LeaderElectionAction(LogCurrentSupervisionElection const& election)
      : _election{election}, _newTerm{std::nullopt} {};
  LeaderElectionAction(LogCurrentSupervisionElection const& election,
                       LogPlanTermSpecification newTerm)
      : _election{election}, _newTerm{newTerm} {};

  LogCurrentSupervisionElection _election;
  std::optional<LogPlanTermSpecification> _newTerm;
};

struct UpdateParticipantFlagsAction {
  static constexpr std::string_view name = "UpdateParticipantFlagsAction";

  UpdateParticipantFlagsAction(ParticipantId const& participant,
                               ParticipantFlags const& flags,
                               std::size_t generation)
      : _participant(participant), _flags(flags), _generation{generation} {};

  ParticipantId _participant;
  ParticipantFlags _flags;
  std::size_t _generation;
};

struct AddParticipantToPlanAction {
  static constexpr std::string_view name = "AddParticipantToPlanAction";

  AddParticipantToPlanAction(ParticipantId const& participant,
                             ParticipantFlags const& flags,
                             std::size_t generation)
      : _participant(participant), _flags(flags), _generation{generation} {};

  ParticipantId _participant;
  ParticipantFlags _flags;
  std::size_t _generation;
};

struct RemoveParticipantFromPlanAction {
  static constexpr std::string_view name = "RemoveParticipantFromPlanAction";

  RemoveParticipantFromPlanAction(ParticipantId const& participant,
                                  std::size_t generation)
      : _participant(participant), _generation{generation} {};

  ParticipantId _participant;
  std::size_t _generation;
};

struct UpdateLogConfigAction {
  static constexpr std::string_view name = "UpdateLogConfigAction";

  UpdateLogConfigAction(LogConfig const& config) : _config(config){};

  LogConfig _config;
};

using Action =
    std::variant<EmptyAction, ErrorAction, AddLogToPlanAction,
                 AddParticipantsToTargetAction, CreateInitialTermAction,
                 DictateLeaderAction, EvictLeaderAction, UpdateTermAction,
                 LeaderElectionAction, UpdateParticipantFlagsAction,
                 AddParticipantToPlanAction, RemoveParticipantFromPlanAction,
                 UpdateLogConfigAction>;

using namespace arangodb::cluster::paths;

/*
 * Execute a SupervisionAction
 */
struct Executor {
  explicit Executor(DatabaseID const& dbName, LogId const& log,
                    arangodb::agency::envelope envelope)
      : dbName{dbName},
        log{log},
        envelope{std::move(envelope)},
        targetPath{
            root()->arango()->target()->replicatedLogs()->database(dbName)->log(
                log)},
        planPath{
            root()->arango()->plan()->replicatedLogs()->database(dbName)->log(
                log)},
        currentPath{root()
                        ->arango()
                        ->current()
                        ->replicatedLogs()
                        ->database(dbName)
                        ->log(log)}

        {};

  DatabaseID dbName;
  LogId log;
  arangodb::agency::envelope envelope;

  std::shared_ptr<Root::Arango::Target::ReplicatedLogs::Database::Log const>
      targetPath;
  std::shared_ptr<Root::Arango::Plan::ReplicatedLogs::Database::Log const>
      planPath;
  std::shared_ptr<Root::Arango::Current::ReplicatedLogs::Database::Log const>
      currentPath;

  std::shared_ptr<Root::Arango::Plan::Version const> planVersionPath;

  void operator()(EmptyAction const& action);
  void operator()(ErrorAction const& action);
  void operator()(AddLogToPlanAction const& action);
  void operator()(AddParticipantsToTargetAction const& action);
  void operator()(CreateInitialTermAction const& action);
  void operator()(DictateLeaderAction const& action);
  void operator()(EvictLeaderAction const& action);
  void operator()(UpdateTermAction const& action);
  void operator()(LeaderElectionAction const& action);
  void operator()(UpdateParticipantFlagsAction const& action);
  void operator()(AddParticipantToPlanAction const& action);
  void operator()(RemoveParticipantFromPlanAction const& action);
  void operator()(UpdateLogConfigAction const& action);
};

struct VelocyPacker {
  VelocyPacker(VPackBuilder& builder) : builder(builder), ob(&builder){};
  VelocyPacker(VelocyPacker&) = delete;
  VPackBuilder& builder;
  VPackObjectBuilder ob;

  void operator()(EmptyAction const& action);
  void operator()(ErrorAction const& action);
  void operator()(AddLogToPlanAction const& action);
  void operator()(AddParticipantsToTargetAction const& action);
  void operator()(CreateInitialTermAction const& action);
  void operator()(DictateLeaderAction const& action);
  void operator()(EvictLeaderAction const& action);
  void operator()(UpdateTermAction const& action);
  void operator()(LeaderElectionAction const& action);
  void operator()(UpdateParticipantFlagsAction const& action);
  void operator()(AddParticipantToPlanAction const& action);
  void operator()(RemoveParticipantFromPlanAction const& action);
  void operator()(UpdateLogConfigAction const& action);
};

auto execute(Action const& action, DatabaseID const& dbName, LogId const& log,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

auto to_string(Action const& action) -> std::string_view;
void toVelocyPack(Action const& action, VPackBuilder& builder);

}  // namespace arangodb::replication2::replicated_log
