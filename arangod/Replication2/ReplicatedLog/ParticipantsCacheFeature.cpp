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
#include "Cluster/ServerState.h"

namespace arangodb::replication2 {

ParticipantsCacheFeature::ParticipantsCacheFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ParticipantsCache") {
  setOptional(true);
  startsAfter<application_features::CommunicationFeaturePhase>();
  startsAfter<application_features::DatabaseFeaturePhase>();
}

void ParticipantsCacheFeature::prepare() {
  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isAgent()) {
    setEnabled(false);
    return;
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

CollectionWatcher::CollectionWatcher(
    AgencyCallbackRegistry* agencyCallbackRegistry,
    LogicalCollection const& collection)
    : _agencyCallbackRegistry(agencyCallbackRegistry), _present(true) {
  std::string databaseName = collection.vocbase().name();
  std::string collectionID = std::to_string(collection.id().id());
  std::string where = "Plan/Collections/" + databaseName + "/" + collectionID;

  _agencyCallback = std::make_shared<AgencyCallback>(
      collection.vocbase().server(), where,
      [self = weak_from_this()](VPackSlice const& result) {
        auto watcher = self.lock();
        if (result.isNone() && watcher) {
          watcher->_present.store(false);
        }
        return true;
      },
      true, false);
  Result res = _agencyCallbackRegistry->registerCallback(_agencyCallback);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

CollectionWatcher::~CollectionWatcher() {
  try {
    _agencyCallbackRegistry->unregisterCallback(_agencyCallback);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42af2", WARN, Logger::CLUSTER)
        << "caught unexpected exception in CollectionWatcher: " << ex.what();
  }
}

}  // namespace arangodb::replication2
