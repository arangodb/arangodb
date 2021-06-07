////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <optional>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/overload.h"
#include "Cluster/MaintenanceFeature.h"
#include "Network/NetworkFeature.h"
#include "Replication2/ReplicatedLog/NetworkAttachedFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "RestServer/DatabaseFeature.h"
#include "UpdateReplicatedLogAction.h"
#include "Utils/DatabaseGuard.h"

#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

using namespace arangodb::basics;
using namespace arangodb::replication2;

bool arangodb::maintenance::UpdateReplicatedLogAction::first() {
  auto spec = std::invoke([&]() -> std::optional<agency::LogPlanSpecification> {
    auto buffer = StringUtils::decodeBase64(_description.get(FOLLOWERS_TO_DROP));
    auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(buffer.c_str()));
    if (!slice.isNone()) {
      return agency::LogPlanSpecification(agency::from_velocypack, slice);
    }

    return std::nullopt;
  });

  auto logId = LogId{StringUtils::uint64(_description.get(COLLECTION))};
  auto serverId = ServerState::instance()->getId();
  auto rebootId = ServerState::instance()->getRebootId();

  auto result = basics::catchToResult([&]() -> Result {
    auto const& database = _description.get(DATABASE);
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    if (!spec.has_value()) {
        return vocbase.dropReplicatedLog(logId);
    }

    TRI_ASSERT(logId == spec->id);
    TRI_ASSERT(spec->currentTerm.has_value());
    auto& leader = spec->currentTerm->leader;

    // TODO add ensureReplicatedLog
    auto& log = vocbase.ensureReplicatedLog(logId);

    auto term = std::invoke([&] {
      auto status = log.getParticipant()->getStatus();
      return std::visit(
          overload{[&](replicated_log::UnconfiguredStatus) -> std::optional<LogTerm> {
                     return std::nullopt;
                   },
                   [&](replicated_log::LeaderStatus const& s) -> std::optional<LogTerm> {
                     return s.term;
                   },
                   [&](replicated_log::FollowerStatus const& s) -> std::optional<LogTerm> {
                     return s.term;
                   }},
          status);
    });

    if (term == spec->currentTerm->term) {
      return Result();
    }

    if (leader.has_value() && leader->serverId == serverId && leader->rebootId == rebootId) {
      replicated_log::LogLeader::TermData termData;
      termData.term = spec->currentTerm->term;
      termData.id = serverId;
      // TODO maybe we should use the same type in the term data
      termData.writeConcern = spec->currentTerm->config.writeConcern;
      termData.waitForSync = spec->currentTerm->config.waitForSync;

      NetworkFeature& nf = _feature.server().getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();

      auto follower =
          std::vector<std::shared_ptr<replication2::replicated_log::AbstractFollower>>{};
      for (auto const& [participant, data] : spec->currentTerm->participants) {
        if (participant != serverId) {
          follower.emplace_back(std::make_shared<replication2::replicated_log::NetworkAttachedFollower>(
              pool, participant, database, logId));
        }
      }

      log.becomeLeader(termData, std::move(follower));
    } else {
      auto leaderString = std::string{};
      if (spec->currentTerm->leader) {
        leaderString = spec->currentTerm->leader->serverId;
      }

      std::ignore = log.becomeFollower(serverId, spec->currentTerm->term, leaderString);
    }

    return Result();
  });

  if (result.fail()) {
    abort();  // to do
  }

  return false;
}

arangodb::maintenance::UpdateReplicatedLogAction::UpdateReplicatedLogAction(
    arangodb::MaintenanceFeature& mf, arangodb::maintenance::ActionDescription const& desc)
    : ActionBase(mf, desc) {}
