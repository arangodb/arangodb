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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <optional>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Inspection/VPack.h"
#include "Network/NetworkFeature.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "RestServer/DatabaseFeature.h"
#include "UpdateReplicatedLogAction.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::replication2;

bool arangodb::maintenance::UpdateReplicatedLogAction::first() {
  auto spec = std::invoke([&]() -> std::optional<agency::LogPlanSpecification> {
    auto buffer =
        StringUtils::decodeBase64(_description.get(REPLICATED_LOG_SPEC));
    auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(buffer.c_str()));
    if (!slice.isNone()) {
      return velocypack::deserialize<agency::LogPlanSpecification>(slice);
    }

    return std::nullopt;
  });

  auto logId = LogId{StringUtils::uint64(_description.get(REPLICATED_LOG_ID))};

  auto const& database = _description.get(DATABASE);
  auto& df = _feature.server().getFeature<DatabaseFeature>();
  DatabaseGuard guard(df, database);

  auto result = std::invoke([&] {
    if (spec.has_value()) {
      ADB_PROD_ASSERT(spec->currentTerm.has_value())
          << database << "/" << logId << " missing term";
      if (auto state = guard->getReplicatedStateById(logId); state.fail()) {
        auto& impl = spec->properties.implementation;
        VPackSlice parameter = impl.parameters ? impl.parameters->slice()
                                               : VPackSlice::noneSlice();
        if (auto res = guard->createReplicatedState(logId, impl.type, parameter)
                           .result();
            res.fail()) {
          return res;
        }
      }

      return guard->updateReplicatedState(logId, *spec->currentTerm,
                                          spec->participantsConfig);
    } else {
      return guard->dropReplicatedState(logId);
    }
  });

  if (result.fail()) {
    LOG_TOPIC("ba775", ERR, Logger::REPLICATION2)
        << "failed to modify replicated log " << _description.get(DATABASE)
        << '/' << logId << "; " << result.errorMessage();
  }

  _feature.addDirty(database);
  return false;
}

arangodb::maintenance::UpdateReplicatedLogAction::UpdateReplicatedLogAction(
    arangodb::MaintenanceFeature& mf,
    arangodb::maintenance::ActionDescription const& desc)
    : ActionBase(mf, desc) {
  _labels.emplace(FAST_TRACK);
}
