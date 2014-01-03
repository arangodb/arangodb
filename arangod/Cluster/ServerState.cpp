////////////////////////////////////////////////////////////////////////////////
/// @brief single-server state
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

#include "ServerState.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/logging.h"
#include "Cluster/AgencyComm.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief server state instance
////////////////////////////////////////////////////////////////////////////////

static ServerState* Instance = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                       ServerState
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ServerState::ServerState () 
  : _id(),
    _address(),
    _lock(),
    _role(ROLE_UNDEFINED),
    _state(STATE_UNDEFINED),
    _uniqid() {

  _uniqid._currentValue = _uniqid._upperValue = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ServerState::~ServerState () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the (sole) instance
////////////////////////////////////////////////////////////////////////////////

ServerState* ServerState::instance () {
  if (Instance == 0) {
    Instance = new ServerState();
  }
  return Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a role
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::roleToString (RoleEnum role) {
  switch (role) {
    case ROLE_UNDEFINED: 
      return "UNDEFINED";
    case ROLE_PRIMARY:
      return "PRIMARY";
    case ROLE_SECONDARY:
      return "SECONDARY";
    case ROLE_COORDINATOR:
      return "COORDINATOR";
  }

  assert(false);
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string representation to a state
////////////////////////////////////////////////////////////////////////////////

ServerState::StateEnum ServerState::stringToState (std::string const& value) {
  if (value == "SHUTDOWN") {
    return STATE_SHUTDOWN;
  }
  // TODO: do we need to understand other states, too?

  return STATE_UNDEFINED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a state
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::stateToString (StateEnum state) {
  switch (state) {
    case STATE_UNDEFINED: 
      return "UNDEFINED";
    case STATE_STARTUP:
      return "STARTUP";
    case STATE_SERVINGASYNC:
      return "SERVINGASYNC";
    case STATE_SERVINGSYNC:
      return "SERVINGSYNC";
    case STATE_STOPPING:
      return "STOPPING";
    case STATE_STOPPED:
      return "STOPPED";
    case STATE_SYNCING:
      return "SYNCING"; 
    case STATE_INSYNC:
      return "INSYNC"; 
    case STATE_LOSTPRIMARY:
      return "LOSTPRIMARY"; 
    case STATE_SERVING:
      return "SERVING"; 
    case STATE_SHUTDOWN:
      return "SHUTDOWN"; 
  }

  assert(false);
  return "";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a unique id
////////////////////////////////////////////////////////////////////////////////

uint64_t ServerState::uniqid () {
  WRITE_LOCKER(_lock);

  if (_uniqid._currentValue >= _uniqid._upperValue) {
    AgencyComm comm;
    AgencyCommResult result = comm.uniqid("Sync/LatestID", ValuesPerBatch, 0.0);

    if (! result.successful() || result._index == 0) {
      return 0;
    }

    _uniqid._currentValue = result._index;
    _uniqid._upperValue   = _uniqid._currentValue + ValuesPerBatch;
  }

  return _uniqid._currentValue++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current state
////////////////////////////////////////////////////////////////////////////////

ServerState::StateEnum ServerState::getState () {
  READ_LOCKER(_lock);
  return _state;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current state
////////////////////////////////////////////////////////////////////////////////
        
void ServerState::setState (StateEnum state) { 
  bool result = false;

  WRITE_LOCKER(_lock);

  if (state == _state) {
    return;
  }

  if (_role == ROLE_PRIMARY) {
    result = checkPrimaryState(state);
  }
  else if (_role == ROLE_SECONDARY) {
    result = checkSecondaryState(state);
  }
  else if (_role == ROLE_COORDINATOR) {
    result = checkCoordinatorState(state);
  }
  
  if (result) {
    LOG_INFO("changing state of %s server from %s to %s", 
             ServerState::roleToString(_role).c_str(),
             ServerState::stateToString(_state).c_str(),
             ServerState::stateToString(state).c_str());

    _state = state;
  }
  else {
    LOG_ERROR("invalid state transition for %s server from %s to %s", 
              ServerState::roleToString(_role).c_str(),
              ServerState::stateToString(_state).c_str(),
              ServerState::stateToString(state).c_str());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a primary server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkPrimaryState (StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  }
  else if (state == STATE_SERVINGASYNC) {
    return (_state == STATE_STARTUP ||
            _state == STATE_STOPPED);
  }
  else if (state == STATE_SERVINGSYNC) {
    return (_state == STATE_STARTUP ||
            _state == STATE_SERVINGASYNC ||
            _state == STATE_STOPPED);
  }
  else if (state == STATE_STOPPING) {
    return (_state == STATE_SERVINGSYNC ||
            _state == STATE_SERVINGASYNC);
  }
  else if (state == STATE_STOPPED) {
    return (_state == STATE_STOPPING);
  }
  else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP ||
            _state == STATE_STOPPED ||
            _state == STATE_SERVINGSYNC ||
            _state == STATE_SERVINGASYNC);
  }

  // anything else is invalid
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a secondary server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkSecondaryState (StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  }
  else if (state == STATE_SYNCING) {
    return (_state == STATE_STARTUP ||
            _state == STATE_LOSTPRIMARY);
  }
  else if (state == STATE_INSYNC) {
    return (_state == STATE_SYNCING);
  }
  else if (state == STATE_LOSTPRIMARY) {
    return (_state == STATE_SYNCING ||
            _state == STATE_INSYNC);
  }
  else if (state == STATE_SERVING) {
    return (_state == STATE_STARTUP);
  }
  else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP ||
            _state == STATE_SYNCING ||
            _state == STATE_INSYNC ||
            _state == STATE_LOSTPRIMARY);
  }

  // anything else is invalid
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a state transition for a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool ServerState::checkCoordinatorState (StateEnum state) {
  if (state == STATE_STARTUP) {
    // startup state can only be set once
    return (_state == STATE_UNDEFINED);
  }
  else if (state == STATE_SERVING) {
    return (_state == STATE_STARTUP);
  }
  else if (state == STATE_SHUTDOWN) {
    return (_state == STATE_STARTUP ||
            _state == STATE_SERVING);
  }

  // anything else is invalid
  return false;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

