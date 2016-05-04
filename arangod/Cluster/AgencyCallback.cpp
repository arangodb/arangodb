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

#include "Basics/MutexLocker.h"
#include "Basics/ConditionLocker.h"

#include <chrono>
#include <thread>

#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "AgencyCallback.h"

using namespace arangodb;

AgencyCallback::AgencyCallback(AgencyComm& agency, 
                               std::string const& key, 
                               std::function<bool(VPackSlice const&)> const& cb,
                               bool needsValue,
                               bool needsInitialValue) 
  : key(key),
    _useCv(false),
    _agency(agency),
    _cb(cb),
    _needsValue(needsValue) {
  if (_needsValue && needsInitialValue) {
    refetchAndUpdate();
  }
}

void AgencyCallback::refetchAndUpdate() {
  if (!_needsValue) {
    // no need to pass any value to the callback
    executeEmpty();
    return;
  }

  AgencyCommResult result = _agency.getValues2(key);

  if (!result.successful()) {
    return;
  }
  
  std::vector<std::string> kv = basics::StringUtils::split(AgencyComm::prefix() + key,'/');
  kv.erase(std::remove(kv.begin(), kv.end(), ""), kv.end());
  
  std::shared_ptr<VPackBuilder> newData = std::make_shared<VPackBuilder>();
  newData->add(result.slice()[0].get(kv));
  
  checkValue(newData);
  
}

void AgencyCallback::checkValue(std::shared_ptr<VPackBuilder> newData) {
  if (!_lastData || !_lastData->slice().equals(newData->slice())) {
    LOG(DEBUG) << "Got new value " << newData->slice().typeName() << " " << newData->toJson();
    if (execute(newData)) {
      _lastData = newData;
    } else {
      LOG(DEBUG) << "Callback was not successful for " << newData->toJson();
    }
  }
}

bool AgencyCallback::executeEmpty() {
  LOG(DEBUG) << "Executing (empty)";
  bool result;
  {
    MUTEX_LOCKER(locker, _lock);
    result = _cb(VPackSlice::noneSlice());
  }

  if (_useCv) {
    CONDITION_LOCKER(locker, _cv);
    _cv.signal();
  }
  return result;
}

bool AgencyCallback::execute(std::shared_ptr<VPackBuilder> newData) {
  LOG(DEBUG) << "Executing";
  bool result;
  {
    MUTEX_LOCKER(locker, _lock);
    result = _cb(newData->slice());
  }

  if (_useCv) {
    CONDITION_LOCKER(locker, _cv);
    _cv.signal();
  }
  return result;
}

void AgencyCallback::waitWithFailover(double timeout) {
  VPackSlice compareSlice;
  if (_lastData) {
    compareSlice = _lastData->slice();
  } else {
    compareSlice = VPackSlice::noneSlice();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(timeout * 1000)));
  
  if (!_lastData || _lastData->slice().equals(compareSlice)) {
    LOG(DEBUG) << "Waiting done and nothing happended. Refetching to be sure";
    // mop: watches have not triggered during our sleep...recheck to be sure
    refetchAndUpdate();
  }
}

void AgencyCallback::executeByCallbackOrTimeout(double maxTimeout) {
  auto compareBuilder = std::make_shared<VPackBuilder>();
  if (_lastData) {
    compareBuilder = _lastData;
  }
  
  _useCv = true;
  CONDITION_LOCKER(locker, _cv);
  locker.wait(static_cast<uint64_t>(maxTimeout * 1000000.0));
  _useCv = false;
  
  if (!_lastData || _lastData->slice().equals(compareBuilder->slice())) {
    LOG(DEBUG) << "Waiting done and nothing happended. Refetching to be sure";
    // mop: watches have not triggered during our sleep...recheck to be sure
    refetchAndUpdate();
  }
}
