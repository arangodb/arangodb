////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCallbackRegistry.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb;
using namespace arangodb::basics;

DECLARE_COUNTER(arangodb_agency_callback_registered_total, "Total number of agency callbacks registered");
DECLARE_GAUGE(arangodb_agency_callback_number, uint64_t, "Current number of agency callbacks registered");

AgencyCallbackRegistry::AgencyCallbackRegistry(application_features::ApplicationServer& server,
                                               std::string const& callbackBasePath)
  : _agency(server), 
    _callbackBasePath(callbackBasePath),
    _totalCallbacksRegistered(
      server.getFeature<arangodb::MetricsFeature>().add(arangodb_agency_callback_registered_total{})),
    _callbacksCount(
      server.getFeature<arangodb::MetricsFeature>().add(arangodb_agency_callback_number{})) {}

AgencyCallbackRegistry::~AgencyCallbackRegistry() = default;

Result AgencyCallbackRegistry::registerCallback(std::shared_ptr<AgencyCallback> cb, bool local) {
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
    if (local) {
      auto& cache = _agency.server().getFeature<ClusterFeature>().agencyCache();
      res = cache.registerCallback(cb->key, id);
    } else {
      res = _agency.registerCallback(cb->key, getEndpointUrl(id)).asResult();
      cb->local(false);
    }
    if (res.ok()) {
      _callbacksCount += 1;
      ++_totalCallbacksRegistered;
      return res;
    }
  } catch (std::exception const& e) {
    res.reset(TRI_ERROR_FAILED, e.what());
  } catch (...) {
    res.reset(TRI_ERROR_FAILED, "unknown exception");
  }
  
  TRI_ASSERT(res.fail());
  res.reset(res.errorNumber(),
            StringUtils::concatT("registering ", (local ? "local " : ""),
                                 "callback failed: ", res.errorMessage()));
  LOG_TOPIC("b88f4", WARN, Logger::CLUSTER) << res.errorMessage();
  
  {
    WRITE_LOCKER(locker, _lock);
    _callbacks.erase(id);
  }
  return res;
}

std::shared_ptr<AgencyCallback> AgencyCallbackRegistry::getCallback(uint64_t id) {
  READ_LOCKER(locker, _lock);
  auto it = _callbacks.find(id);

  if (it == _callbacks.end()) {
    return nullptr;
  }
  return (*it).second;
}

bool AgencyCallbackRegistry::unregisterCallback(std::shared_ptr<AgencyCallback> cb) {
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
      if (cb->local()) {
        auto& cache = _agency.server().getFeature<ClusterFeature>().agencyCache();
        cache.unregisterCallback(cb->key, id);
      } else {
        _agency.unregisterCallback(cb->key, getEndpointUrl(id));
      }
      _callbacksCount -= 1;
    }
  }
  return found;
}

std::string AgencyCallbackRegistry::getEndpointUrl(uint64_t id) const {
  std::stringstream url;
  url << Endpoint::uriForm(ServerState::instance()->getEndpoint())
      << _callbackBasePath << "/" << id;

  return url.str();
}
