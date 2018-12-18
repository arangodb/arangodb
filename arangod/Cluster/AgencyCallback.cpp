////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::application_features;

AgencyCallback::AgencyCallback(AgencyComm& agency, std::string const& key,
                               std::function<bool(VPackSlice const&)> const& cb,
                               bool needsValue, bool needsInitialValue)
    : key(key), _agency(agency), _cb(cb), _needsValue(needsValue) {
  if (_needsValue && needsInitialValue) {
    refetchAndUpdate(true, false);
  }
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

  AgencyCommResult result = _agency.getValues(key);

  if (!result.successful()) {
    if (!application_features::ApplicationServer::isStopping()) {
      // only log errors if we are not already shutting down...
      // in case of shutdown this error is somewhat expected
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
          << "Callback getValues to agency was not successful: "
          << result.errorCode() << " " << result.errorMessage();
    }
    return;
  }

  std::vector<std::string> kv =
      basics::StringUtils::split(AgencyCommManager::path(key), '/');
  kv.erase(std::remove(kv.begin(), kv.end(), ""), kv.end());

  auto newData = std::make_shared<VPackBuilder>();
  newData->add(result.slice()[0].get(kv));

  if (needToAcquireMutex) {
    CONDITION_LOCKER(locker, _cv);
    checkValue(std::move(newData), forceCheck);
  } else {
    checkValue(std::move(newData), forceCheck);
  }
}

void AgencyCallback::checkValue(std::shared_ptr<VPackBuilder> newData,
                                bool forceCheck) {
  // Only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  if (!_lastData || !_lastData->slice().equals(newData->slice()) || forceCheck) {
    LOG_TOPIC(DEBUG, Logger::CLUSTER) << "AgencyCallback: Got new value "
                                      << newData->slice().typeName() << " "
                                      << newData->toJson()
                                      << " forceCheck=" << forceCheck;
    if (execute(newData)) {
      _lastData = newData;
    } else {
      LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Callback was not successful for "
                                        << newData->toJson();
    }
  }
}

bool AgencyCallback::executeEmpty() {
  // only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Executing (empty)";
  bool result = _cb(VPackSlice::noneSlice());
  if (result) {
    _cv.signal();
  }
  return result;
}

bool AgencyCallback::execute(std::shared_ptr<VPackBuilder> newData) {
  // only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Executing";
  bool result = _cb(newData->slice());
  if (result) {
    _cv.signal();
  }
  return result;
}

void AgencyCallback::executeByCallbackOrTimeout(double maxTimeout) {
  // One needs to acquire the mutex of the condition variable
  // before entering this function!
  if (!_cv.wait(static_cast<uint64_t>(maxTimeout * 1000000.0))
    && application_features::ApplicationServer::isRetryOK()) {
    LOG_TOPIC(DEBUG, Logger::CLUSTER)
        << "Waiting done and nothing happended. Refetching to be sure";
    // mop: watches have not triggered during our sleep...recheck to be sure
    refetchAndUpdate(false, true);  // Force a check
  }
}
