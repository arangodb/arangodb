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
/// @author Alex Petenchea
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ParticipantsCacheFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "FeaturePhases/FinalFeaturePhase.h"
#include "FeaturePhases/AgencyFeaturePhase.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "Cluster/ClusterFeature.h"

namespace arangodb::replication2 {

const std::string_view ParticipantsCacheFeature::kParticipantsHealthPath =
    "arango/Supervision/Health";

ParticipantsCacheFeature::ParticipantsCacheFeature(Server& server)
    : ArangodFeature{server, *this} {
  agencyCallback = std::make_shared<AgencyCallback>(
      server, std::string(kParticipantsHealthPath),
      [self = weak_from_this()](VPackSlice const& result) {
        auto watcher = self.lock();
        if (watcher) {
          if (result.isNone()) {
            LOG_DEVEL << "result is None";
          } else {
            LOG_DEVEL << result.toString();
          }
        } else {
          LOG_DEVEL << "watcher destroyed";
        }
        return true;
      },
      true, false);

  setOptional(true);
  startsAfter<ClusterFeature>();
}

void ParticipantsCacheFeature::stop() {
  try {
    auto agencyCallbackRegistry =
        server().getEnabledFeature<ClusterFeature>().agencyCallbackRegistry();
    agencyCallbackRegistry->unregisterCallback(agencyCallback);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42af2", WARN, Logger::REPLICATION2)
        << "caught unexpected exception while unregistering agency callback in "
           "ParticipantsCacheFeature: "
        << ex.what();
  }
}

void ParticipantsCacheFeature::start() {
  auto agencyCallbackRegistry =
      server().getEnabledFeature<ClusterFeature>().agencyCallbackRegistry();
  Result res = agencyCallbackRegistry->registerCallback(agencyCallback, true);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void ParticipantsCacheFeature::prepare() {
  if (ServerState::instance()->isAgent()) {
    disable();
  }
}

// TODO ask agency for initial state in case the server is restarted
auto ParticipantsCacheFeature::isServerFailed(
    std::string_view serverId) const noexcept -> bool {
  if (auto status = isFailed.find(std::string(serverId));
      status != std::end(isFailed)) {
    return status->second;
  }
  return false;
}

}  // namespace arangodb::replication2
