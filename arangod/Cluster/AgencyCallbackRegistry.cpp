////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCallbackRegistry.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::basics;

DECLARE_COUNTER(arangodb_agency_callback_registered_total,
                "Total number of agency callbacks registered");
DECLARE_GAUGE(arangodb_agency_callback_number, uint64_t,
              "Current number of agency callbacks registered");

AgencyCallbackRegistry::AgencyCallbackRegistry(
    ApplicationServer& server, ClusterFeature& clusterFeature,
    EngineSelectorFeature& engineSelectorFeature,
    DatabaseFeature& databaseFeature, metrics::MetricsFeature& metrics,
    std::string const& callbackBasePath)
    : _server(server),
      _clusterFeature(clusterFeature),
      _agencyComm(server, clusterFeature, engineSelectorFeature,
                  databaseFeature),
      _callbackBasePath(callbackBasePath),
      _totalCallbacksRegistered(
          metrics.add(arangodb_agency_callback_registered_total{})),
      _callbacksCount(metrics.add(arangodb_agency_callback_number{})) {}

AgencyCallbackRegistry::~AgencyCallbackRegistry() = default;

Result AgencyCallbackRegistry::registerCallback(
    std::shared_ptr<AgencyCallback> cb) {
  uint64_t id;
  while (true) {
    id = RandomGenerator::interval(std::numeric_limits<uint64_t>::max());

    WRITE_LOCKER(locker, _lock);
    if (_callbacks.try_emplace(id, cb).second) {
      break;
    }
  }

  Result res;
  try {
    res = _clusterFeature.agencyCache().registerCallback(cb->key, id);
    if (res.ok()) {
      _callbacksCount += 1;
      ++_totalCallbacksRegistered;

      if (cb->needsInitialValue()) {
        cb->refetchAndUpdate(true, false);
      }
      return res;
    }
  } catch (std::exception const& e) {
    res.reset(TRI_ERROR_FAILED, e.what());
  } catch (...) {
    res.reset(TRI_ERROR_FAILED, "unknown exception");
  }

  TRI_ASSERT(res.fail());
  res.reset(res.errorNumber(),
            StringUtils::concatT("registering local callback failed: ",
                                 res.errorMessage()));
  LOG_TOPIC("b88f4", WARN, Logger::CLUSTER) << res.errorMessage();

  {
    WRITE_LOCKER(locker, _lock);
    _callbacks.erase(id);
  }
  return res;
}

std::shared_ptr<AgencyCallback> AgencyCallbackRegistry::getCallback(
    uint64_t id) {
  READ_LOCKER(locker, _lock);
  auto it = _callbacks.find(id);

  if (it == _callbacks.end()) {
    return nullptr;
  }
  return (*it).second;
}

bool AgencyCallbackRegistry::unregisterCallback(
    std::shared_ptr<AgencyCallback> cb) {
  bool found = false;
  uint64_t id = 0;
  {
    // find the key of the callback while only holding a read lock
    READ_LOCKER(locker, _lock);

    for (auto const& it : _callbacks) {
      if (it.second.get() == cb.get()) {
        id = it.first;
        found = true;
        break;
      }
    }
  }

  // if we found the callback, we need to unregister it and remove it from
  // the map. that requires a write lock
  if (found) {
    {
      WRITE_LOCKER(locker, _lock);
      if (_callbacks.erase(id) == 0) {
        // callback not in map anymore. this can only happen if this method
        // is called concurrently for the same callback and one thread has
        // already deleted it from the map. in this case we act like as if
        // the callback was not found, and leave the callback handling to
        // the other thread.
        found = false;
      }
    }
    // we need to release the write lock for the map already, because we are
    // now calling into other methods which may also acquire locks. and we
    // don't want to be vulnerable to priority inversion.

    if (found) {
      _clusterFeature.agencyCache().unregisterCallback(cb->key, id);

      _callbacksCount -= 1;
    }
  }
  return found;
}

std::string AgencyCallbackRegistry::getEndpointUrl(uint64_t id) const {
  return absl::StrCat(Endpoint::uriForm(ServerState::instance()->getEndpoint()),
                      _callbackBasePath, "/", id);
}
