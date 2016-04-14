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

#include "AgencyCallback.h"
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>
#include <chrono>
#include <thread>
#include "Basics/MutexLocker.h"

using namespace arangodb;

AgencyCallback::AgencyCallback(AgencyComm& agency, 
                               std::string const& key, 
                               std::function<bool(VPackSlice const&)> const& cb,
                               bool needsValue) 
  : key(key),
    _agency(agency),
    _cb(cb),
    _needsValue(needsValue) {

  if (_needsValue) {
    refetchAndUpdate();
  }
}

void AgencyCallback::refetchAndUpdate() {
  if (!_needsValue) {
    // no need to pass any value to the callback
    executeEmpty();
    return;
  }

  AgencyCommResult result = _agency.getValues(key, true);

  if (!result.successful()) {
    return;
  }
  
  if (!result.parse("", false)) {
    LOG(ERR) << "Cannot parse body " << result.body();
    return;
  }

  std::map<std::string, AgencyCommResultEntry>::const_iterator it =
      result._values.begin();
  
  if (it == result._values.end()) {
    std::shared_ptr<VPackBuilder> newData = std::make_shared<VPackBuilder>();
    checkValue(newData);
  } else {
    checkValue(it->second._vpack);
  }
}

void AgencyCallback::checkValue(std::shared_ptr<VPackBuilder> newData) {
  if (!_lastData || !_lastData->slice().equals(newData->slice())) {
    LOG(DEBUG) << "Got new value" << newData->toJson();
    if (execute(newData)) {
      _lastData = newData;
    } else {
      LOG(DEBUG) << "Callback was not successful for " << newData->toJson();
    }
  }
}

bool AgencyCallback::executeEmpty() {
  LOG(DEBUG) << "Executing (empty)";
  MUTEX_LOCKER(locker, _lock);
  return _cb(VPackSlice::noneSlice());
}

bool AgencyCallback::execute(std::shared_ptr<VPackBuilder> newData) {
  LOG(DEBUG) << "Executing";
  MUTEX_LOCKER(locker, _lock);
  return _cb(newData->slice());
}

void AgencyCallback::waitWithFailover(double timeout) {
  // mop: todo thread safe? check with max
  std::shared_ptr<VPackBuilder> beginData = _lastData;
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(timeout * 1000)));
  
  if (!_lastData || _lastData->slice().equals(beginData->slice())) {
    LOG(DEBUG) << "Waiting done and nothing happended. Refetching to be sure";
    // mop: watches have not triggered during our sleep...recheck to be sure
    refetchAndUpdate();
  }
}
