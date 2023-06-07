////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Replication2/ReplicatedLog/SupervisionContext.h"

namespace arangodb::agency {
struct envelope;
}

namespace arangodb::replication2::replicated_log {

using LogCurrentLocalStates =
    std::unordered_map<ParticipantId, LogCurrentLocalState>;

auto computeEffectiveWriteConcern(LogTargetConfig const& config,
                                  ParticipantsFlagsMap const& participants,
                                  ParticipantsHealth const& health) -> size_t;

auto computeEffectiveWriteConcern(LogTargetConfig const& config,
                                  LogCurrent const& current,
                                  LogPlanSpecification const& plan,
                                  ParticipantsHealth const& health) -> size_t;

auto isLeaderFailed(ServerInstanceReference const& leader,
                    ParticipantsHealth const& health) -> bool;

auto computeReason(std::optional<LogCurrentLocalState> const& maybeStatus,
                   bool healthy, bool excluded, LogTerm term)
    -> LogCurrentSupervisionElection::ErrorCode;

struct CleanOracle {
  TEST_VIRTUAL auto serverIsClean(ServerInstanceReference,
                                  bool const assumedWaitForSync) -> bool;
  TEST_VIRTUAL ~CleanOracle() = default;
};

auto runElectionCampaign(LogCurrentLocalStates const& participantId,
                         ParticipantsConfig const& participantsConfig,
                         ParticipantsHealth const& health, LogTerm term,
                         bool assumedWaitForSync, CleanOracle)
    -> LogCurrentSupervisionElection;

auto getParticipantsAcceptableAsLeaders(
    ParticipantId const& currentLeader, LogTerm term,
    ParticipantsFlagsMap const& participants,
    std::unordered_map<ParticipantId, LogCurrentLocalState> const& localStates)
    -> std::vector<ParticipantId>;

// Actions capture entries in log, so they have to stay
// valid until the returned action has been executed (or discarded)
auto checkReplicatedLog(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void;

auto executeCheckReplicatedLog(DatabaseID const& database,
                               std::string const& logIdString, Log log,
                               ParticipantsHealth const& health,
                               arangodb::agency::envelope envelope) noexcept
    -> arangodb::agency::envelope;

auto buildAgencyTransaction(DatabaseID const& dbName, LogId const& logId,
                            SupervisionContext& sctx, ActionContext& actx,
                            size_t maxActionsTraceLength,
                            arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::replicated_log
