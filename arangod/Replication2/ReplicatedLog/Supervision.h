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

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

using LogCurrentLocalStates =
    std::unordered_map<ParticipantId, LogCurrentLocalState>;

auto isLeaderFailed(LogPlanTermSpecification::Leader const& leader,
                    ParticipantsHealth const& health) -> bool;

auto getAddedParticipant(ParticipantsFlagsMap const& target,
                         ParticipantsFlagsMap const& plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>>;

auto getRemovedParticipant(ParticipantsFlagsMap const& target,
                           ParticipantsFlagsMap const& plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>>;

auto getParticipantWithUpdatedFlags(
    ParticipantsFlagsMap const& targetParticipants,
    ParticipantsFlagsMap const& planParticipants,
    std::optional<ParticipantId> const& targetLeader,
    ParticipantId const& currentTermLeader)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>>;

auto computeReason(LogCurrentLocalState const& status, bool healthy,
                   bool excluded, LogTerm term)
    -> LogCurrentSupervisionElection::ErrorCode;

auto runElectionCampaign(LogCurrentLocalStates const& states,
                         ParticipantsConfig const& participantsConfig,
                         ParticipantsHealth const& health, LogTerm term)
    -> LogCurrentSupervisionElection;

auto doLeadershipElection(LogPlanSpecification const& plan,
                          LogCurrent const& current,
                          ParticipantsHealth const& health) -> Action;

auto getParticipantsAcceptableAsLeaders(
    ParticipantId const& currentLeader,
    ParticipantsFlagsMap const& participants) -> std::vector<ParticipantId>;

auto dictateLeader(LogTarget const& target, LogPlanSpecification const& plan,
                   LogCurrent const& current, ParticipantsHealth const& health)
    -> Action;

// Actions capture entries in log, so they have to stay
// valid until the returned action has been executed (or discarded)
auto checkReplicatedLog(LogTarget const& target,
                        std::optional<LogPlanSpecification> const& plan,
                        std::optional<LogCurrent> const& current,
                        ParticipantsHealth const& health) -> Action;

}  // namespace arangodb::replication2::replicated_log
