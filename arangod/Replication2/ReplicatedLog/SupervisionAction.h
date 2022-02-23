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
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct EmptyAction {
  static constexpr std::string_view name = "EmptyAction";
};

struct ErrorAction {
  static constexpr std::string_view name = "ErrorAction";
  std::string _message;
};

struct CurrentNotAvailableAction {
  static constexpr std::string_view name = "CurrentNotAvailableAction";
};

struct AddLogToPlanAction {
  static constexpr std::string_view name = "AddLogToPlanAction";

  LogTarget::Participants const _participants;
};

struct AddParticipantsToTargetAction {
  static constexpr std::string_view name = "AddParticipantstoTargetAction";
  LogTarget::Participants const _participants;
};

struct CreateInitialTermAction {
  static constexpr std::string_view name = "CreateInitialTermAction";

  LogConfig const config;
};

struct UpdateTermAction {
  static constexpr std::string_view name = "UpdateTermAction";

  LogPlanTermSpecification _newTerm;
};

struct DictateLeaderAction {
  static constexpr std::string_view name = "DictateLeaderAction";

  LogPlanTermSpecification _term;
};

struct EvictLeaderAction {
  static constexpr std::string_view name = "EvictLeaderAction";

  explicit EvictLeaderAction(ParticipantId const &leader,
                             ParticipantFlags flags,
                             LogPlanTermSpecification term, size_t generation)
      : _leader{leader}, _flags{flags}, _term{term}, _generation{generation} {
    _flags.excluded = true;
    _term.term = LogTerm{_term.term.value + 1};
    _term.leader.reset();
  }

  ParticipantId _leader;
  ParticipantFlags _flags;
  LogPlanTermSpecification _term;
  std::size_t _generation;
};

struct LeaderElectionAction {
  static constexpr std::string_view name = "LeaderElectionAction";

  LogCurrentSupervisionElection _election;
  std::optional<LogPlanTermSpecification> _newTerm;
};

struct UpdateParticipantFlagsAction {
  static constexpr std::string_view name = "UpdateParticipantFlagsAction";

  ParticipantId _participant;
  ParticipantFlags _flags;
  std::size_t _generation;
};

struct AddParticipantToPlanAction {
  static constexpr std::string_view name = "AddParticipantToPlanAction";

  ParticipantId _participant;
  ParticipantFlags _flags;
  std::size_t _generation;
};

struct RemoveParticipantFromPlanAction {
  static constexpr std::string_view name = "RemoveParticipantFromPlanAction";
  ParticipantId _participant;
  std::size_t _generation;
};

struct UpdateLogConfigAction {
  static constexpr std::string_view name = "UpdateLogConfigAction";
  LogConfig _config;
};

struct ConvergedToGenerationAction {
  static constexpr std::string_view name = "ConvergedToGenerationAction";
  std::size_t _generation;
};

using Action =
    std::variant<EmptyAction, ErrorAction, AddLogToPlanAction,
                 AddParticipantsToTargetAction, CreateInitialTermAction,
                 CurrentNotAvailableAction, UpdateTermAction,
                 DictateLeaderAction, EvictLeaderAction, LeaderElectionAction,
                 UpdateParticipantFlagsAction, AddParticipantToPlanAction,
                 RemoveParticipantFromPlanAction, UpdateLogConfigAction>;

using namespace arangodb::cluster::paths;

/*
 * Execute a SupervisionAction
 */
struct Executor {
  explicit Executor(DatabaseID const &dbName, LogId const &log,
                    arangodb::agency::envelope envelope)
      : dbName{dbName}, log{log}, envelope{std::move(envelope)},
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

  std::shared_ptr<Root::Arango::Plan::Version const> planVersion;

  void operator()(EmptyAction const &action);
  void operator()(ErrorAction const &action);
  void operator()(AddLogToPlanAction const &action);
  void operator()(AddParticipantsToTargetAction const &action);
  void operator()(CreateInitialTermAction const &action);
  void operator()(CurrentNotAvailableAction const &action);
  void operator()(UpdateTermAction const &action);
  void operator()(DictateLeaderAction const &action);
  void operator()(EvictLeaderAction const &action);
  void operator()(LeaderElectionAction const &action);
  void operator()(UpdateParticipantFlagsAction const &action);
  void operator()(AddParticipantToPlanAction const &action);
  void operator()(RemoveParticipantFromPlanAction const &action);
  void operator()(UpdateLogConfigAction const &action);
};

auto execute(Action const &action, DatabaseID const &dbName, LogId const &log,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

void toVelocyPack(Action const &action, VPackBuilder &builder);

} // namespace arangodb::replication2::replicated_log
