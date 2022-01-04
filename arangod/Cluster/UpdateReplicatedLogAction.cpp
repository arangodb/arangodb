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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Network/NetworkFeature.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/NetworkAttachedFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "RestServer/DatabaseFeature.h"
#include "UpdateReplicatedLogAction.h"
#include "Utils/DatabaseGuard.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::replication2;

namespace {
struct LogActionContextMaintenance : algorithms::LogActionContext {
  LogActionContextMaintenance(TRI_vocbase_t& vocbase, network::ConnectionPool* pool)
      : vocbase(vocbase), pool(pool) {}

  auto dropReplicatedLog(LogId id) -> arangodb::Result override {
    return vocbase.dropReplicatedLog(id);
  }
  auto ensureReplicatedLog(LogId id)
      -> std::shared_ptr<replicated_log::ReplicatedLog> override {
    return vocbase.ensureReplicatedLog(id, std::nullopt);
  }
  auto buildAbstractFollowerImpl(LogId id, ParticipantId participantId)
      -> std::shared_ptr<replicated_log::AbstractFollower> override {
    return std::make_shared<replicated_log::NetworkAttachedFollower>(
        pool, std::move(participantId), vocbase.name(), id);
  }

  TRI_vocbase_t& vocbase;
  network::ConnectionPool* pool;
};
}

bool arangodb::maintenance::UpdateReplicatedLogAction::first() {

  auto spec = std::invoke([&]() -> std::optional<agency::LogPlanSpecification> {
    auto buffer = StringUtils::decodeBase64(_description.get(REPLICATED_LOG_SPEC));
    auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(buffer.c_str()));
    if (!slice.isNone()) {
      return agency::LogPlanSpecification(agency::from_velocypack, slice);
    }

    return std::nullopt;
  });

  auto logId = LogId{StringUtils::uint64(_description.get(REPLICATED_LOG_ID))};
  auto serverId = ServerState::instance()->getId();
  auto rebootId = ServerState::instance()->getRebootId();

  network::ConnectionPool* pool = _feature.server().getFeature<NetworkFeature>().pool();

  auto const& database = _description.get(DATABASE);
  auto& df = _feature.server().getFeature<DatabaseFeature>();
  DatabaseGuard guard(df, database);
  auto ctx = LogActionContextMaintenance{guard.database(), pool};
  auto result = replication2::algorithms::updateReplicatedLog(
      ctx, serverId, rebootId, logId,
      spec.has_value() ? &spec.value() : nullptr);
  std::move(result).thenFinal([desc = _description, logId, &feature = _feature](
                                  futures::Try<Result>&& tryResult) noexcept {
    try {
      auto const& result = tryResult.get();
      if (result.fail()) {
        LOG_TOPIC("ba775", ERR, Logger::REPLICATION2)
            << "failed to modify replicated log " << desc.get(DATABASE) << '/'
            << logId << "; " << result.errorMessage();
      }
      feature.addDirty(desc.get(DATABASE));
    } catch (std::exception const& e) {
      LOG_TOPIC("f824f", ERR, Logger::REPLICATION2)
          << "exception during update of replicated log " << desc.get(DATABASE)
          << '/' << logId << "; " << e.what();
    }
  });

  return false;
}

arangodb::maintenance::UpdateReplicatedLogAction::UpdateReplicatedLogAction(
    arangodb::MaintenanceFeature& mf, arangodb::maintenance::ActionDescription const& desc)
    : ActionBase(mf, desc) {}
