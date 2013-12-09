////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HeartbeatThread.h"
#include "Basics/ConditionLocker.h"
#include "BasicsC/logging.h"
#include "Sharding/ServerState.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                   HeartbeatThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread (std::string const& myId,
                                  uint64_t interval,
                                  uint64_t maxFailsBeforeWarning) 
  : Thread("heartbeat"),
    _agency(),
    _condition(),
    _myId(myId),
    _interval(interval),
    _maxFailsBeforeWarning(maxFailsBeforeWarning),
    _numFails(0),
    _stop(0) {

  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run () {
  LOG_TRACE("starting heartbeat thread");

  while (! _stop) {
    LOG_TRACE("sending heartbeat");

    sendState();
    
    CONDITION_LOCKER(guard, _condition);
    guard.wait(_interval);
  }
  
  LOG_TRACE("stopping heartbeat thread");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::init () {
  // send the server state a first time and use this as an indicator about
  // the agency's health
  if (! sendState()) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendState () {
  const std::string value = ServerState::stateToString(ServerState::instance()->getState()) + ":" + AgencyComm::generateStamp();

  // return value is intentionally not handled
  // if sending the current state fails, we'll just try again in the next iteration
  bool result = _agency.setValue("state/servers/state/" + _myId, value);

  if (result) {
    _numFails = 0;
  }
  else {
    if (++_numFails % _maxFailsBeforeWarning == 0) {
      const std::string endpoints = AgencyComm::getEndpointsString();

      LOG_WARNING("heartbeat could not be sent to agency endpoints (%s)", endpoints.c_str());
    }
  }

  return result;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

