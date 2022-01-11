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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"
#include "Cluster/ClusterTypes.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/SupervisionTypes.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

using LogCurrentLocalStates =
    std::unordered_map<ParticipantId, LogCurrentLocalState>;

// Check whether a log has been added to target
auto checkLogAdded(const Log& log) -> std::unique_ptr<Action>;

//
auto checkLeaderPresent(LogPlanSpecification const& plan,
                        LogCurrent const& current,
                        ParticipantsHealth const& health)
    -> std::unique_ptr<Action>;

auto checkLeaderHealth(LogPlanSpecification const& plan,
                       ParticipantsHealth const& health)
    -> std::unique_ptr<Action>;

auto runElectionCampaign(LogCurrentLocalStates const& states,
                         ParticipantsHealth const& health, LogTerm term)
    -> LeaderElectionCampaign;

auto tryLeadershipElection(LogPlanSpecification const& plan,
                           LogCurrent const& current,
                           ParticipantsHealth const& health)
    -> std::unique_ptr<Action>;

auto checkLogTargetParticipantFlags(LogTarget const& target,
                                    LogPlanSpecification const& plan)
    -> std::unique_ptr<Action>;

auto checkLogTargetParticipantAdded(LogTarget const& target,
                                    LogPlanSpecification const& plan)
    -> std::unique_ptr<Action>;

auto checkLogTargetParticipantRemoved(LogTarget const& target,
                                      LogPlanSpecification const& plan)
    -> std::unique_ptr<Action>;

auto checkLogTargetConfig(LogTarget const& target,
                          LogPlanSpecification const& plan)
    -> std::unique_ptr<Action>;

// Actions capture entries in log, so they have to stay
// valid until the returned action has been executed (or discarded)
auto checkReplicatedLog(Log const& log, ParticipantsHealth const& health)
    -> std::unique_ptr<Action>;

}  // namespace arangodb::replication2::replicated_log
