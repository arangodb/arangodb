////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ReadLocker.h"
#include "Basics/random.h"
#include "Basics/WriteLocker.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include <ctime>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

AgencyCallbackRegistry::AgencyCallbackRegistry(std::string const& callbackBasePath)
  : _agency(),
  _callbackBasePath(callbackBasePath) {
}

AgencyCallbackRegistry::~AgencyCallbackRegistry() {
}

bool AgencyCallbackRegistry::registerCallback(std::shared_ptr<AgencyCallback> cb) {
  uint32_t rand;
  { 
    WRITE_LOCKER(locker, _lock);
    while (true) {
      rand = TRI_UInt32Random();
      if (_endpoints.emplace(rand, cb).second) {
        break;
      }
    }
  }

  bool ok = false;
  try {
    ok = _agency.registerCallback(cb->key, getEndpointUrl(rand));
    if (!ok) {
      LOG(ERR) << "Registering callback failed";
    }
  } catch (std::exception const& e) {
    LOG(ERR) << "Couldn't register callback " << e.what();
  } catch (...) {
    LOG(ERR) << "Couldn't register callback. Unknown exception";
  }
  if (!ok) {
    WRITE_LOCKER(locker, _lock);
    _endpoints.erase(rand);
  }
  return ok;
}

std::shared_ptr<AgencyCallback> AgencyCallbackRegistry::getCallback(uint32_t id) {
  READ_LOCKER(locker, _lock);
  auto it = _endpoints.find(id);

  if (it == _endpoints.end()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_UNKNOWN_CALLBACK_ENDPOINT);
  }
  return (*it).second;
}

bool AgencyCallbackRegistry::unregisterCallback(std::shared_ptr<AgencyCallback> cb) {
  WRITE_LOCKER(locker, _lock);

  for (auto const& it: _endpoints) {
    if (it.second.get() == cb.get()) {
      _agency.unregisterCallback(cb->key, getEndpointUrl(it.first));
      _endpoints.erase(it.first);
      return true;
    }
  }
  return false;
}

std::string AgencyCallbackRegistry::getEndpointUrl(uint32_t endpoint) {
  std::stringstream url;
  url << Endpoint::uriForm(ServerState::instance()->getAddress()) << _callbackBasePath << "/" << endpoint;
  
  return url.str();
}

void AgencyCallbackRegistry::awaitNextChange(std::string const& key, double timeout) {
  auto maxWait = std::chrono::milliseconds(static_cast<int>(timeout * 1000));

  std::condition_variable cv;
  
  std::function<bool(VPackSlice const& result)> notify = [&](VPackSlice const& result) {
    LOG(DEBUG) << "Notifying change!";
    cv.notify_one();
    return true;
  };
  auto agencyCallback = std::make_shared<AgencyCallback>(_agency, key, notify, false);
  
  std::mutex mtx;
  std::unique_lock<std::mutex> lck(mtx);
  // mop: hmmm if callback registering failed this will just wait for the timeout, which
  // should be ok I think?
  registerCallback(agencyCallback);
  LOG(DEBUG) << "Awaiting change!";
  if (cv.wait_for(lck, maxWait) == std::cv_status::timeout) {
    LOG(DEBUG) << "Reached timeout!";
  }
  unregisterCallback(agencyCallback);
}
