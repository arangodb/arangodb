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
#include "Cluster/ServerState.h"

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

HeartbeatThread::HeartbeatThread (uint64_t interval,
                                  uint64_t maxFailsBeforeWarning) 
  : Thread("heartbeat"),
    _agency(),
    _condition(),
    _myId(ServerState::instance()->getId()),
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
/// the heartbeat thread constantly reports the current server status to the
/// agency. it does so by sending the current state string to the key
/// "State/ServerStates/" + my-id.
/// after transferring the current state to the agency, the heartbeat thread
/// will wait for changes on the "Commands/" + my-id key. If no changes occur,
/// then the request it aborted and the heartbeat thread will go on with 
/// reporting its state to the agency again. If it notices a change when 
/// watching the command key, it will wake up and apply the change locally.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run () {
  LOG_TRACE("starting heartbeat thread");

  // convert timeout to seconds  
  const double interval = (double) _interval / 1000.0 / 1000.0;

  // value of /Commands/my-id at startup 
  uint64_t lastCommandIndex = getLastCommandIndex(); 

  while (! _stop) {
    LOG_TRACE("sending heartbeat to agency");

    // send our state to the agency. 
    // we don't care if this fails
    sendState();

    if (_stop) {
      break;
    }

    // watch Commands/my-id for changes

    // TODO: check if this is CPU-intensive and whether we need to sleep
    AgencyCommResult result = _agency.watchValue("Commands/" + _myId, 
                                                 lastCommandIndex + 1, 
                                                 interval,
                                                 false); 
      
    if (_stop) {
      break;
    }

    if (result.successful()) {
      // value has changed!
      handleStateChange(result, lastCommandIndex);

      // sleep a while 
      CONDITION_LOCKER(guard, _condition);
      guard.wait(_interval);
    }
    else {
      // value did not change, but we already blocked waiting for a change...
      // nothing to do here
      LOG_ERROR("HeartbeatThread suspended for 10 seconds");
      sleep(10);
    }
  }

  // another thread is waiting for this value to shut down properly
  _stop = 2;
  
  LOG_TRACE("stopped heartbeat thread");
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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the index id of the value of /Commands/my-id from the agency 
/// this index value is determined initially and it is passed to the watch
/// command (we're waiting for an entry with a higher id)
////////////////////////////////////////////////////////////////////////////////

uint64_t HeartbeatThread::getLastCommandIndex () {
  // get the initial command state  
  AgencyCommResult result = _agency.getValues("Commands/" + _myId, false);

  if (result.successful()) {
    std::map<std::string, std::string> out;

    if (result.flattenJson(out, "Commands/", true)) {
      // check if we can find ourselves in the list returned by the agency
      std::map<std::string, std::string>::const_iterator it = out.find(_myId);

      if (it != out.end()) {
        // found something
        LOG_TRACE("last command index was: '%s'", (*it).second.c_str());
        return triagens::basics::StringUtils::uint64((*it).second);
      }
    }
  }
  
  if (result._index > 0) {
    // use the value returned in header X-Etcd-Index
    return result._index;
  }

  // nothing found. this is not an error
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a state change
/// this is triggered if the watch command reports a change
/// when this is called, it will update the index value of the last command
/// (we'll pass the updated index value to the next watches so we don't get
/// notified about this particular change again).
////////////////////////////////////////////////////////////////////////////////
      
bool HeartbeatThread::handleStateChange (AgencyCommResult const& result,
                                         uint64_t& lastCommandIndex) {
  std::map<std::string, std::string> out;

  if (result.flattenJson(out, "Commands/", true)) {
    // get the new value of "modifiedIndex"
    std::map<std::string, std::string>::const_iterator it = out.find(_myId);

    if (it != out.end()) {
      lastCommandIndex = triagens::basics::StringUtils::uint64((*it).second);
    }
  }

  out.clear();
     
  if (result.flattenJson(out, "Commands/", false)) {
    // get the new value!
    std::map<std::string, std::string>::const_iterator it = out.find(_myId);

    if (it != out.end()) {
      const std::string command = (*it).second;

      ServerState::StateEnum newState = ServerState::stringToState(command);

      if (newState != ServerState::STATE_UNDEFINED) {
        // state change.
        ServerState::instance()->setState(newState);
        return true;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendState () {
  const bool result = _agency.sendServerState();

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

