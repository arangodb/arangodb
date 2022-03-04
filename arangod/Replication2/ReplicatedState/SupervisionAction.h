////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::replicated_state {

struct EmptyAction {};

struct AddParticipantAction {
  ParticipantId participant;
  StateGeneration generation;
};

struct AddStateToPlanAction {
  replication2::agency::LogTarget logTarget;
  agency::Plan statePlan;
};

struct ModifyParticipantFlagsAction {
  ParticipantId participant;
  ParticipantFlags flags;
};

using Action = std::variant<EmptyAction, AddParticipantAction,
                            AddStateToPlanAction, ModifyParticipantFlagsAction>;

struct Executor {
  LogId id;
  DatabaseID const& database;
  arangodb::agency::envelope envelope;

  void operator()(EmptyAction const&);
  void operator()(AddParticipantAction const&);
  void operator()(AddStateToPlanAction const&);
  void operator()(ModifyParticipantFlagsAction const&);
};

auto execute(LogId id, DatabaseID const& database, Action const& action,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::replicated_state
