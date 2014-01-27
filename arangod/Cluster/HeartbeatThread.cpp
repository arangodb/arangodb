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
#include "Basics/JsonHelper.h"
#include "BasicsC/logging.h"
#include "Cluster/DBServerJob.h"
#include "Cluster/ServerState.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/Job.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/server.h"

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

HeartbeatThread::HeartbeatThread (TRI_server_t* server,
                                  triagens::rest::ApplicationDispatcher* dispatcher,
                                  ApplicationV8* applicationV8,
                                  uint64_t interval,
                                  uint64_t maxFailsBeforeWarning)
  : Thread("heartbeat"),
    _server(server),
    _dispatcher(dispatcher),
    _applicationV8(applicationV8),
    _agency(),
    _condition(),
    _myId(ServerState::instance()->getId()),
    _interval(interval),
    _maxFailsBeforeWarning(maxFailsBeforeWarning),
    _numFails(0),
    _stop(0) {

  assert(_dispatcher != 0);
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
/// "Sync/ServerStates/" + my-id.
/// after transferring the current state to the agency, the heartbeat thread
/// will wait for changes on the "Sync/Commands/" + my-id key. If no changes occur,
/// then the request it aborted and the heartbeat thread will go on with 
/// reporting its state to the agency again. If it notices a change when 
/// watching the command key, it will wake up and apply the change locally.
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run () {
  LOG_TRACE("starting heartbeat thread");

  // convert timeout to seconds  
  const double interval = (double) _interval / 1000.0 / 1000.0;
  
  // last value of plan that we fetched
  uint64_t lastPlanVersion = 0;

  // value of Sync/Commands/my-id at startup 
  uint64_t lastCommandIndex = getLastCommandIndex(); 
  bool valueFound = false;

  const bool isCoordinator = ServerState::instance()->isCoordinator();

  while (! _stop) {
    LOG_TRACE("sending heartbeat to agency");

    const double start = TRI_microtime();

    // send our state to the agency. 
    // we don't care if this fails
    sendState();

    if (_stop) {
      break;
    }

    if (! isCoordinator) {
      // get the current version of the Plan
      AgencyCommResult result = _agency.getValues("Plan/Version", false);

      if (result.successful()) {
        result.parse("", false);

        std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

        if (it != result._values.end()) {
          // there is a plan version
          uint64_t planVersion = triagens::basics::JsonHelper::stringUInt64((*it).second._json);

          if (planVersion > lastPlanVersion) {
            handlePlanChange(planVersion, lastPlanVersion);
          }
        }
      }
    }

    {
      // send an initial GET request to Sync/Commands/my-id
      AgencyCommResult result = _agency.getValues("Sync/Commands/" + _myId, false);

      if (result.successful()) {
        handleStateChange(result, lastCommandIndex);
        valueFound = true;
      }
    }
    
    if (_stop) {
      break;
    }
  

    {
      // watch Sync/Commands/my-id for changes

      // TODO: check if this is CPU-intensive and whether we need to sleep
      AgencyCommResult result = _agency.watchValue("Sync/Commands/" + _myId, 
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
        // check for a specific AGENCY error code 
        if (result.httpCode() == triagens::rest::HttpResponse::BAD && result.errorCode() == 401) {
          // the requested history has been cleared
          // pick up new index from the agency
          const uint64_t agencyIndex = result.index();
          if (agencyIndex > 0) {
            lastCommandIndex = agencyIndex;
          }
        }
        if (valueFound) {
          // value did not change, but we already blocked waiting for a change...
          // nothing to do here
        }
        else {
          const double remain = interval - (TRI_microtime() - start);

          if (remain > 0.0) {
            usleep((unsigned long) (remain * 1000.0 * 1000.0)); 
          }
        }
      }
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
/// @brief fetch the index id of the value of Sync/Commands/my-id from the 
/// agency this index value is determined initially and it is passed to the 
/// watch command (we're waiting for an entry with a higher id)
////////////////////////////////////////////////////////////////////////////////

uint64_t HeartbeatThread::getLastCommandIndex () {
  // get the initial command state  
  AgencyCommResult result = _agency.getValues("Sync/Commands/" + _myId, false);

  if (result.successful()) {
    result.parse("Sync/Commands/", false);
   
    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.find(_myId);

    if (it != result._values.end()) {
      // found something
      LOG_TRACE("last command index was: '%llu'", (unsigned long long) (*it).second._index);
      return (*it).second._index;
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
/// @brief handles a plan version change
/// this is triggered if the heartbeat thread finds a new plan version number
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::handlePlanChange (uint64_t currentPlanVersion,
                                        uint64_t& remotePlanVersion) {
  LOG_TRACE("found a plan update");

  // schedule a job for the change
  triagens::rest::Job* job = new DBServerJob(_server, _applicationV8);

  assert(job != 0);

  if (_dispatcher->dispatcher()->addJob(job)) {
    remotePlanVersion = currentPlanVersion;

    LOG_TRACE("scheduled plan update handler");
  }
  else {
    LOG_ERROR("could not schedule plan update handler");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a state change
/// this is triggered if the watch command reports a change
/// when this is called, it will update the index value of the last command
/// (we'll pass the updated index value to the next watches so we don't get
/// notified about this particular change again).
////////////////////////////////////////////////////////////////////////////////
      
bool HeartbeatThread::handleStateChange (AgencyCommResult& result,
                                         uint64_t& lastCommandIndex) {
  result.parse("Sync/Commands/", false);

  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.find(_myId);

  if (it != result._values.end()) {
    lastCommandIndex = (*it).second._index;

    const std::string command = triagens::basics::JsonHelper::getStringValue((*it).second._json, "");
    ServerState::StateEnum newState = ServerState::stringToState(command);
      
    if (newState != ServerState::STATE_UNDEFINED) {
      // state change.
      ServerState::instance()->setState(newState);
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends the current server's state to the agency
////////////////////////////////////////////////////////////////////////////////

bool HeartbeatThread::sendState () {
  const AgencyCommResult result = _agency.sendServerState();

  if (result.successful()) {
    _numFails = 0;
    return true;

  }
    
  if (++_numFails % _maxFailsBeforeWarning == 0) {
    const std::string endpoints = AgencyComm::getEndpointsString();

    LOG_WARNING("heartbeat could not be sent to agency endpoints (%s): http code: %d, body: %s", 
                endpoints.c_str(),
                result.httpCode(),
                result.body().c_str());
    _numFails = 0;
  }

  return false;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

