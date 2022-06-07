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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <optional>

#include "Logger/LogMacros.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "Cluster/MaintenanceFeature.h"
#include "UpdateReplicatedStateAction.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "Replication2/ReplicatedState/UpdateReplicatedState.h"

#include "Futures/Try.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

struct StateActionContextImpl : algorithms::StateActionContext {
  explicit StateActionContextImpl(TRI_vocbase_t& vocbase) : vocbase(vocbase) {}

  auto getReplicatedStateById(LogId id) noexcept
      -> std::shared_ptr<replicated_state::ReplicatedStateBase> override {
    try {
      return vocbase.getReplicatedStateById(id);
    } catch (...) {
      return nullptr;
    }
  }

  auto createReplicatedState(LogId id, std::string_view type,
                             velocypack::Slice data)
      -> ResultT<
          std::shared_ptr<replicated_state::ReplicatedStateBase>> override {
    return vocbase.createReplicatedState(id, type, data);
  }

  auto dropReplicatedState(LogId id) -> Result override {
    return vocbase.dropReplicatedState(id);
  }

  TRI_vocbase_t& vocbase;
};

bool arangodb::maintenance::UpdateReplicatedStateAction::first() {
  auto const extractOptionalType =
      [&]<typename T>(std::string_view key) -> std::optional<T> {
    auto buffer = StringUtils::decodeBase64(_description.get(std::string{key}));
    auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(buffer.c_str()));
    if (!slice.isNone()) {
      return velocypack::deserialize<T>(slice);
    }

    return std::nullopt;
  };

  auto spec = extractOptionalType.operator()<replicated_state::agency::Plan>(
      REPLICATED_LOG_SPEC);
  auto current =
      extractOptionalType.operator()<replicated_state::agency::Current>(
          REPLICATED_STATE_CURRENT);

  auto logId = LogId{StringUtils::uint64(_description.get(REPLICATED_LOG_ID))};
  auto serverId = ServerState::instance()->getId();

  auto const& database = _description.get(DATABASE);
  auto& df = _feature.server().getFeature<DatabaseFeature>();
  DatabaseGuard guard(df, database);

  auto ctx = StateActionContextImpl{guard.database()};
  try {
    auto result = replication2::algorithms::updateReplicatedState(
        ctx, serverId, logId, spec.has_value() ? &spec.value() : nullptr,
        current.has_value() ? &current.value() : nullptr);
    if (result.fail()) {
      LOG_TOPIC("ba776", ERR, Logger::REPLICATION2)
          << "failed to modify replicated state " << database << '/' << logId
          << "; " << result.errorMessage();
    }
    _feature.addDirty(database);
  } catch (std::exception const& e) {
    LOG_TOPIC("f824e", ERR, Logger::REPLICATION2)
        << "exception during update of replicated state " << database << '/'
        << logId << "; " << e.what();
  }

  return false;
}

arangodb::maintenance::UpdateReplicatedStateAction::UpdateReplicatedStateAction(
    arangodb::MaintenanceFeature& mf,
    arangodb::maintenance::ActionDescription const& desc)
    : ActionBase(mf, desc) {
  _labels.emplace(FAST_TRACK);
}
