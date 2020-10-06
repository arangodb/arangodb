////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "AgencyCallback.h"

#include <chrono>
#include <thread>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::application_features;

AgencyCallback::AgencyCallback(application_features::ApplicationServer& server, std::string const& key,
                               std::function<bool(VPackSlice const&)> const& cb,
                               bool needsValue, bool needsInitialValue)
    : key(key),
      _server(server),
      _cb(cb),
      _needsValue(needsValue),
      _wasSignaled(false),
      _local(true) {
  if (_needsValue && needsInitialValue) {
    refetchAndUpdate(true, false);
  }
}

void AgencyCallback::local(bool b) {
  _local = b;
  if (!b) {
    _agency = std::make_unique<AgencyComm>(_server);
  }
}

bool AgencyCallback::local() const {
  return _local;
}

void AgencyCallback::refetchAndUpdate(bool needToAcquireMutex, bool forceCheck) {
  if (!_needsValue) {
    // no need to pass any value to the callback
    if (needToAcquireMutex) {
      CONDITION_LOCKER(locker, _cv);
      executeEmpty();
    } else {
      executeEmpty();
    }
    return;
  }

  VPackSlice result;
  std::shared_ptr<VPackBuilder> builder;
  consensus::index_t idx = 0;
  AgencyCommResult tmp;
  
  LOG_TOPIC("a6344", TRACE, Logger::CLUSTER) <<
    "Refetching and update for " << AgencyCommHelper::path(key);
  
  if (_local) {
    auto& _cache = _server.getFeature<ClusterFeature>().agencyCache();
    std::tie(builder, idx) = _cache.read(std::vector<std::string>{AgencyCommHelper::path(key)});
    result = builder->slice();
    if (!result.isArray()) {
      if (!_server.isStopping()) {
        // only log errors if we are not already shutting down...
        // in case of shutdown this error is somewhat expected
        LOG_TOPIC("ec320", ERR, arangodb::Logger::CLUSTER)
          << "Callback to get agency cache was not successful: " << result.toJson();
      }
      return;
    }
  } else {
    TRI_ASSERT(_agency.get() != nullptr);
    tmp = _agency->getValues(key);
    if (!tmp.successful()) {
      if (!_server.isStopping()) {
        // only log errors if we are not already shutting down...
        // in case of shutdown this error is somewhat expected
        LOG_TOPIC("fb402", ERR, arangodb::Logger::CLUSTER)
          << "Callback getValues to agency was not successful: " << tmp.errorCode()
          << " " << tmp.errorMessage();
      }
      return;
    }
    result = tmp.slice();
  }

  std::vector<std::string> kv =
      basics::StringUtils::split(AgencyCommHelper::path(key), '/');
  kv.erase(std::remove(kv.begin(), kv.end(), ""), kv.end());

  auto newData = std::make_shared<VPackBuilder>();
  newData->add(result[0].get(kv));

  if (needToAcquireMutex) {
    CONDITION_LOCKER(locker, _cv);
    checkValue(std::move(newData), forceCheck);
  } else {
    checkValue(std::move(newData), forceCheck);
  }
}

void AgencyCallback::checkValue(std::shared_ptr<VPackBuilder> newData, bool forceCheck) {
  // Only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  if (!_lastData || 
      forceCheck || 
      !arangodb::basics::VelocyPackHelper::equal(_lastData->slice(), newData->slice(), false)) {
    LOG_TOPIC("2bd14", DEBUG, Logger::CLUSTER)
        << "AgencyCallback: Got new value " << newData->slice().typeName()
        << " " << newData->toJson() << " forceCheck=" << forceCheck;
    if (execute(newData->slice())) {
      _lastData = newData;
    } else {
      LOG_TOPIC("337dc", DEBUG, Logger::CLUSTER)
          << "Callback was not successful for " << newData->toJson();
    }
  }
}

bool AgencyCallback::executeEmpty() {
  // only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  return execute(VPackSlice::noneSlice());
}

bool AgencyCallback::execute(velocypack::Slice newData) {
  // only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  LOG_TOPIC("add4e", DEBUG, Logger::CLUSTER) << "Executing" << (newData.isNone() ? " (empty)" : "");
  try {
    bool result = _cb(newData);
    if (result) {
      _wasSignaled = true;
      _cv.signal();
    }
    return result;
  } catch (std::exception const& ex) {
    LOG_TOPIC("1de99", WARN, Logger::CLUSTER) << "AgencyCallback execution failed: " << ex.what();
    throw;
  }
}

bool AgencyCallback::executeByCallbackOrTimeout(double maxTimeout) {
  // One needs to acquire the mutex of the condition variable
  // before entering this function!
  if (!_server.isStopping()) {
    if (_wasSignaled) {
      // ok, we have been signaled already, so there is no need to wait at all
      // directly refetch the values
      _wasSignaled = false;
      LOG_TOPIC("67690", DEBUG, Logger::CLUSTER)
          << "We were signaled already";
      return false;
    }

    // we haven't yet been signaled. so let's wait for a signal or
    // the timeout to occur
    if (!_cv.wait(static_cast<uint64_t>(maxTimeout * 1000000.0))) {
      LOG_TOPIC("1514e", DEBUG, Logger::CLUSTER)
          << "Waiting done and nothing happended. Refetching to be sure";
      // mop: watches have not triggered during our sleep...recheck to be sure
      refetchAndUpdate(false, true);  // Force a check
      return true;
    }
  }
  return false;
}
