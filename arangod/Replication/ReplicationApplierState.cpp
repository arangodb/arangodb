////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationApplierState.h"
#include "Replication/common-defines.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

ReplicationApplierState::ReplicationApplierState() 
    : _lastProcessedContinuousTick(0),
      _lastAppliedContinuousTick(0),
      _lastAvailableContinuousTick(0),
      _safeResumeTick(0),
      _phase(ActivityPhase::INACTIVE),
      _preventStart(false),
      _stopInitialSynchronization(false),
      _progressMsg(),
      _serverId(0),
      _failedConnects(0),
      _totalRequests(0),
      _totalFailedConnects(0),
      _totalEvents(0),
      _totalResyncs(0),
      _skippedOperations(0) {
  _progressTime[0] = '\0';
}

ReplicationApplierState::~ReplicationApplierState() {}

ReplicationApplierState& ReplicationApplierState::operator=(ReplicationApplierState const& other) {
  reset(true);

  _phase = other._phase;
  _lastAppliedContinuousTick = other._lastAppliedContinuousTick;
  _lastProcessedContinuousTick = other._lastProcessedContinuousTick;
  _lastAvailableContinuousTick = other._lastAvailableContinuousTick;
  _safeResumeTick = other._safeResumeTick;
  _serverId = other._serverId;
  _progressMsg = other._progressMsg;
  memcpy(&_progressTime[0], &other._progressTime[0], sizeof(_progressTime));

  _lastError.code = other._lastError.code;
  _lastError.message = other._lastError.message;
  memcpy(&_lastError.time[0], &other._lastError.time[0], sizeof(_lastError.time));
  
  _failedConnects = other._failedConnects;
  _totalRequests = other._totalRequests;
  _totalFailedConnects = other._totalFailedConnects;
  _totalEvents = other._totalEvents;
  _totalResyncs = other._totalResyncs;
  _skippedOperations = other._skippedOperations;

  return *this;
}

void ReplicationApplierState::reset(bool resetState) {
  _lastProcessedContinuousTick = 0;
  _lastAppliedContinuousTick = 0;
  _lastAvailableContinuousTick = 0;
  _safeResumeTick = 0;
  _preventStart = false;
  _stopInitialSynchronization = false;
  _progressMsg.clear();
  _progressTime[0] = '\0';
  _serverId = 0;
  _lastError.reset();
      
  _failedConnects = 0;
  _totalRequests = 0;
  _totalFailedConnects = 0;
  _totalEvents = 0;
  _totalResyncs = 0;
  _skippedOperations = 0;
  
  if (resetState) {
    _phase = ActivityPhase::INACTIVE;
  }
}

static std::string ActivityToString(ReplicationApplierState::ActivityPhase ph) {
  switch (ph) {
    case ReplicationApplierState::ActivityPhase::INACTIVE:
      return "inactive";
    case ReplicationApplierState::ActivityPhase::INITIAL:
      return "initial";
    case ReplicationApplierState::ActivityPhase::TAILING:
      return "running";
    case ReplicationApplierState::ActivityPhase::SHUTDOWN:
      return "shutdown";
  }
  return "unknown";
}

void ReplicationApplierState::toVelocyPack(VPackBuilder& result, bool full) const {
  result.openObject();

  if (full) {
    result.add("running", VPackValue(isTailing())); // isRunning
    result.add("phase", VPackValue(ActivityToString(_phase)));

    // lastAppliedContinuousTick
    if (_lastAppliedContinuousTick > 0) {
      result.add("lastAppliedContinuousTick", VPackValue(std::to_string(_lastAppliedContinuousTick)));
    } else {
      result.add("lastAppliedContinuousTick", VPackValue(VPackValueType::Null));
    }

    // lastProcessedContinuousTick
    if (_lastProcessedContinuousTick > 0) {
      result.add("lastProcessedContinuousTick", VPackValue(std::to_string(_lastProcessedContinuousTick)));
    } else {
      result.add("lastProcessedContinuousTick", VPackValue(VPackValueType::Null));
    }

    // lastAvailableContinuousTick
    if (_lastAvailableContinuousTick > 0) {
      result.add("lastAvailableContinuousTick", VPackValue(std::to_string(_lastAvailableContinuousTick)));
    } else {
      result.add("lastAvailableContinuousTick", VPackValue(VPackValueType::Null));
    }

    // safeResumeTick
    if (_safeResumeTick > 0) {
      result.add("safeResumeTick", VPackValue(std::to_string(_safeResumeTick)));
    } else {
      result.add("safeResumeTick", VPackValue(VPackValueType::Null));
    }

    if (isTailing()) {
      TRI_voc_tick_t ticksBehind = _lastAvailableContinuousTick - std::max(_lastAppliedContinuousTick, _lastProcessedContinuousTick);
      result.add("ticksBehind", VPackValue(ticksBehind));
      result.add("inSync", VPackValue(ticksBehind == 0));
    }

    // progress
    result.add("progress", VPackValue(VPackValueType::Object));
    result.add("time", VPackValue(_progressTime));
    if (!_progressMsg.empty()) {
      result.add("message", VPackValue(_progressMsg));
    }
    result.add("failedConnects", VPackValue(_failedConnects));
    result.close(); // progress

    result.add("totalRequests", VPackValue(_totalRequests));
    result.add("totalFailedConnects", VPackValue(_totalFailedConnects));
    result.add("totalEvents", VPackValue(_totalEvents));
    result.add("totalResyncs", VPackValue(_totalResyncs));
    result.add("totalOperationsExcluded", VPackValue(_skippedOperations));

    // lastError
    result.add(VPackValue("lastError"));
    _lastError.toVelocyPack(result);

    char timeString[24];
    TRI_GetTimeStampReplication(timeString, sizeof(timeString) - 1);
    result.add("time", VPackValue(&timeString[0]));
  } else {
    result.add("serverId", VPackValue(std::to_string(_serverId))); 
    result.add("lastProcessedContinuousTick", VPackValue(std::to_string(_lastProcessedContinuousTick))); 
    result.add("lastAppliedContinuousTick", VPackValue(std::to_string(_lastAppliedContinuousTick))); 
    result.add("safeResumeTick", VPackValue(std::to_string(_safeResumeTick)));
  }

  result.close();
}

