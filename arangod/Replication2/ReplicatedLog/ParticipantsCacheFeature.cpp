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

#include "Replication2/ReplicatedLog/FailureOracle.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/LogMacros.h"

#include <memory>
#include <unordered_map>

namespace arangodb::replication2 {

class ParticipantsCache final
    : public FailureOracle,
      public std::enable_shared_from_this<ParticipantsCache> {
 public:
  ParticipantsCache() = default;
  ~ParticipantsCache() override = default;

  auto isServerFailed(std::string_view const serverId) const noexcept
      -> bool override {
    if (auto status = isFailed.find(std::string(serverId));
        status != std::end(isFailed)) [[likely]] {
      return status->second;
    }
    return true;
  }

  void start(AgencyCallbackRegistry* agencyCallbackRegistry);
  void stop(AgencyCallbackRegistry* agencyCallbackRegistry);

 private:
  std::unordered_map<std::string, bool> isFailed;
  std::shared_ptr<AgencyCallback> agencyCallback;

  friend ParticipantsCacheFeature;
};

void ParticipantsCache::start(AgencyCallbackRegistry* agencyCallbackRegistry) {
  Result res = agencyCallbackRegistry->registerCallback(agencyCallback, true);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void ParticipantsCache::stop(AgencyCallbackRegistry* agencyCallbackRegistry) {
  try {
    agencyCallbackRegistry->unregisterCallback(agencyCallback);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42bf2", WARN, Logger::REPLICATION2)
        << "Caught unexpected exception while unregistering agency callback "
           "for ParticipantsCache: "
        << ex.what();
  }
}

ParticipantsCacheFeature::ParticipantsCacheFeature(Server& server)
    : ArangodFeature{server, *this}, cache(createHealthCache(server)) {
  setOptional(true);
  startsAfter<ClusterFeature>();
}

void ParticipantsCacheFeature::prepare() {
  if (ServerState::instance()->isAgent()) {
    disable();
  } else {
    enable();
  }
}

void ParticipantsCacheFeature::start() {
  LOG_DEVEL << "ParticipantsCacheFeature started";
  auto agencyCallbackRegistry =
      server().getEnabledFeature<ClusterFeature>().agencyCallbackRegistry();
  if (agencyCallbackRegistry == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Expected non-null AgencyCallbackRegistry "
                                   "when starting ParticipantsCacheFeature.");
  }
  cache->start(agencyCallbackRegistry);
  LOG_TOPIC("42af3", DEBUG, Logger::REPLICATION2)
      << "ParticipantsCacheFeature is ready";
}

void ParticipantsCacheFeature::stop() {
  LOG_DEVEL << "ParticipantsCacheFeature stopped";
  try {
    auto agencyCallbackRegistry =
        server().getEnabledFeature<ClusterFeature>().agencyCallbackRegistry();
    cache->stop(agencyCallbackRegistry);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42af2", WARN, Logger::REPLICATION2)
        << "caught unexpected exception while unregistering agency callback in "
           "ParticipantsCacheFeature: "
        << ex.what();
  }
}

auto ParticipantsCacheFeature::getFailureOracle()
    -> std::shared_ptr<FailureOracle> {
  return std::dynamic_pointer_cast<FailureOracle>(cache);
}

const std::string_view ParticipantsCacheFeature::kParticipantsHealthPath =
    "Supervision/Health";

auto ParticipantsCacheFeature::createHealthCache(Server& server)
    -> std::shared_ptr<ParticipantsCache> {
  // static_assert(Server::contains<ClusterFeature>());

  auto cache = std::make_shared<ParticipantsCache>();
  cache->agencyCallback = std::make_shared<AgencyCallback>(
      server, std::string(kParticipantsHealthPath),
      [cache = std::weak_ptr(cache)](VPackSlice const& result) {
        LOG_DEVEL << "ParticipantsCacheFeature agencyCallback called";
        auto watcher = cache.lock();
        if (watcher) {
          if (result.isNone()) {
            LOG_DEVEL << "result is None";
          } else {
            LOG_DEVEL << result.toJson();
          }
        } else {
          LOG_DEVEL << "watcher destroyed";
        }
        return true;
      },
      true, false);
  return cache;
}

}  // namespace arangodb::replication2
