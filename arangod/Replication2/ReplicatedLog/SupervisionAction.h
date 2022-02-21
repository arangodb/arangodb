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
#include "velocypack/velocypack-aliases.h"
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

  LogPlanTermSpecification const _term;
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

  ParticipantId _leader;
  ParticipantFlags _flags;
  LogPlanTermSpecification _newTerm;
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
                 UpdateTermAction, DictateLeaderAction, EvictLeaderAction,
                 LeaderElectionAction, UpdateParticipantFlagsAction,
                 AddParticipantToPlanAction, RemoveParticipantFromPlanAction,
                 UpdateLogConfigAction>;

// using Trace = std::vector<std::string>;

/*
struct CheckReplicatedLog {
  Target const &target;
  Plan const &plan;
  Current const &current;

  std::optional<Action> action{std::nullopt};
  Trace trace;

  auto check(std::function f) && -> CheckReplicatedLog {
    if (action) {
      return check;
    } else {
      auto [action, trace] = f(target, plan, current);
      return CheckReplicatedLog{action, trace++ check.trace};
    }
  };
};
 */

} // namespace arangodb::replication2::replicated_log
