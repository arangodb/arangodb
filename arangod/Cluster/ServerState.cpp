////////////////////////////////////////////////////////////////////////////////
/// @brief single-server state
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ServerState.h"
#include "Basics/JsonHelper.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/logging.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief single instance of ServerState - will live as long as the server is
/// running
////////////////////////////////////////////////////////////////////////////////

static ServerState Instance;

// -----------------------------------------------------------------------------
// --SECTION--                                                       ServerState
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ServerState::ServerState ()
  : _id(),
    _dataPath(),
    _logPath(),
    _agentPath(),
    _arangodPath(),
    _dbserverConfig(),
    _coordinatorConfig(),
    _disableDispatcherFrontend(),
    _disableDispatcherKickstarter(),
    _address(),
    _authentication(),
    _lock(),
    _role(),
    _idOfPrimary(""),
    _state(STATE_UNDEFINED),
    _initialized(false),
    _clusterEnabled(false) {
    
  storeRole(ROLE_UNDEFINED);
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
  return &Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a role
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::roleToString (ServerState::RoleEnum role) {
  switch (role) {
    case ROLE_UNDEFINED:
      return "UNDEFINED";
    case ROLE_SINGLE:
      return "SINGLE";
    case ROLE_PRIMARY:
      return "PRIMARY";
    case ROLE_SECONDARY:
      return "SECONDARY";
    case ROLE_COORDINATOR:
      return "COORDINATOR";
  }

  TRI_ASSERT(false);
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to a role
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::stringToRole (std::string const& value) {
  if (value == "SINGLE") {
    return ROLE_SINGLE;
  }
  else if (value == "PRIMARY") {
    return ROLE_PRIMARY;
  }
  else if (value == "SECONDARY") {
    return ROLE_SECONDARY;
  }
  else if (value == "COORDINATOR") {
    return ROLE_COORDINATOR;
  }

  return ROLE_UNDEFINED;
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

  TRI_ASSERT(false);
  return "";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief set the authentication data for cluster-internal communication
////////////////////////////////////////////////////////////////////////////////

void ServerState::setAuthentication (std::string const& username,
                                     std::string const& password) {
  _authentication = "Basic " + basics::StringUtils::encodeBase64(username + ":" + password);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the authentication data for cluster-internal communication
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getAuthentication () {
  return _authentication;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the server state (used for testing)
////////////////////////////////////////////////////////////////////////////////

void ServerState::flush () {
  {
    WRITE_LOCKER(_lock);

    if (_id.empty()) {
      return;
    }

    _address = ClusterInfo::instance()->getTargetServerEndpoint(_id);
  }

  storeRole(determineRole(_localInfo, _id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the server is a coordinator
////////////////////////////////////////////////////////////////////////////////

bool ServerState::isCoordinator () {
  return isCoordinator(loadRole());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the server is a coordinator
////////////////////////////////////////////////////////////////////////////////

bool ServerState::isCoordinator (ServerState::RoleEnum role) {
  return (role == ServerState::ROLE_COORDINATOR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the server is a DB server (primary or secondary)
/// running in cluster mode.
////////////////////////////////////////////////////////////////////////////////

bool ServerState::isDBServer () {
  return isDBServer(loadRole());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the server is a DB server (primary or secondary)
/// running in cluster mode.
////////////////////////////////////////////////////////////////////////////////

bool ServerState::isDBServer (ServerState::RoleEnum role) {
  return (role == ServerState::ROLE_PRIMARY ||
          role == ServerState::ROLE_SECONDARY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the server is running in a cluster
////////////////////////////////////////////////////////////////////////////////

bool ServerState::isRunningInCluster () {
  auto role = loadRole();
  
  return (role == ServerState::ROLE_PRIMARY ||
          role == ServerState::ROLE_SECONDARY ||
          role == ServerState::ROLE_COORDINATOR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server role
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::getRole () {
  std::string id;
  std::string info;

  {
    auto role = loadRole();
    
    if (role != ServerState::ROLE_UNDEFINED || ! _clusterEnabled) {
      return role;
    }

    info = _localInfo;
    id = _id;
  }

  if (id.empty()) {
    // We need to announce ourselves in the agency to get a role configured:
    LOG_DEBUG("Announcing our birth in Current/NewServers to the agency...");
    AgencyComm comm;
    AgencyCommResult result;
    Json json(Json::Object, 1);
    json("endpoint", Json(TRI_UNKNOWN_MEM_ZONE, getAddress()));
    std::string description = getDescription();
    if (! description.empty()) {
      json("Description", Json(TRI_UNKNOWN_MEM_ZONE, description));
    }
    result = comm.setValue("Current/NewServers/"+_localInfo, json.json(), 0.0);
    if (! result.successful()) {
      LOG_ERROR("Could not talk to agency!");
      return ROLE_UNDEFINED;
    }
    std::string jsonst = json.toString();
    LOG_DEBUG("Have stored %s under Current/NewServers/%s in agency.",
              jsonst.c_str(), _localInfo.c_str());
  }

  // role not yet set
  RoleEnum role = determineRole(info, id);
  std::string roleString = roleToString(role);

  LOG_DEBUG("Found my role: %s", roleString.c_str());

  storeRole(role);

  return role;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server role
////////////////////////////////////////////////////////////////////////////////

void ServerState::setRole (ServerState::RoleEnum role) {
  storeRole(role);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server local info
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getLocalInfo () {
  READ_LOCKER(_lock);
  return _localInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server local info
////////////////////////////////////////////////////////////////////////////////

void ServerState::setLocalInfo (std::string const& localInfo) {
  if (localInfo.empty()) {
    return;
  }

  WRITE_LOCKER(_lock);
  _localInfo = localInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server id
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getId () {
  READ_LOCKER(_lock);
  return _id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server id
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getPrimaryId () {
  READ_LOCKER(_lock);
  return _idOfPrimary;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server id
////////////////////////////////////////////////////////////////////////////////

void ServerState::setId (std::string const& id) {
  if (id.empty()) {
    return;
  }

  WRITE_LOCKER(_lock);
  _id = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server description
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getDescription () {
  READ_LOCKER(_lock);
  return _description;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server description
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDescription (std::string const& description) {
  if (description.empty()) {
    return;
  }

  WRITE_LOCKER(_lock);
  _description = description;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the server address
////////////////////////////////////////////////////////////////////////////////

std::string ServerState::getAddress () {
  std::string id;

  {
    READ_LOCKER(_lock);
    if (! _address.empty()) {
      return _address;
    }

    id = _id;
  }

  // address not yet set
  if (id.empty()) {
    return "";
  }

  // fetch and set the address
  std::string const address = ClusterInfo::instance()->getTargetServerEndpoint(id);

  {
    WRITE_LOCKER(_lock);
    _address = address;
  }

  return address;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server address
////////////////////////////////////////////////////////////////////////////////

void ServerState::setAddress (std::string const& address) {
  if (address.empty()) {
    return;
  }

  WRITE_LOCKER(_lock);
  _address = address;
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
  auto role = loadRole();

  WRITE_LOCKER(_lock);

  if (state == _state) {
    return;
  }

  if (role == ROLE_PRIMARY) {
    result = checkPrimaryState(state);
  }
  else if (role == ROLE_SECONDARY) {
    result = checkSecondaryState(state);
  }
  else if (role == ROLE_COORDINATOR) {
    result = checkCoordinatorState(state);
  }

  if (result) {
    LOG_INFO("changing state of %s server from %s to %s",
             ServerState::roleToString(role).c_str(),
             ServerState::stateToString(_state).c_str(),
             ServerState::stateToString(state).c_str());

    _state = state;
  }
  else {
    LOG_ERROR("invalid state transition for %s server from %s to %s",
              ServerState::roleToString(role).c_str(),
              ServerState::stateToString(_state).c_str(),
              ServerState::stateToString(state).c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the data path
////////////////////////////////////////////////////////////////////////////////

string ServerState::getDataPath () {
  READ_LOCKER(_lock);
  return _dataPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the data path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDataPath (const string& value) {
  WRITE_LOCKER(_lock);
  _dataPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the log path
////////////////////////////////////////////////////////////////////////////////

string ServerState::getLogPath () {
  READ_LOCKER(_lock);
  return _logPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the log path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setLogPath (const string& value) {
  WRITE_LOCKER(_lock);
  _logPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the agent path
////////////////////////////////////////////////////////////////////////////////

string ServerState::getAgentPath () {
  READ_LOCKER(_lock);
  return _agentPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the data path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setAgentPath (const string& value) {
  WRITE_LOCKER(_lock);
  _agentPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the arangod path
////////////////////////////////////////////////////////////////////////////////

string ServerState::getArangodPath () {
  READ_LOCKER(_lock);
  return _arangodPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the arangod path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setArangodPath (const string& value) {
  WRITE_LOCKER(_lock);
  _arangodPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the JavaScript startup path
////////////////////////////////////////////////////////////////////////////////

string ServerState::getJavaScriptPath () {
  READ_LOCKER(_lock);
  return _javaScriptStartupPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the arangod path
////////////////////////////////////////////////////////////////////////////////

void ServerState::setJavaScriptPath (const string& value) {
  WRITE_LOCKER(_lock);
  _javaScriptStartupPath = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the DBserver config
////////////////////////////////////////////////////////////////////////////////

string ServerState::getDBserverConfig () {
  READ_LOCKER(_lock);
  return _dbserverConfig;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the DBserver config
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDBserverConfig (const string& value) {
  WRITE_LOCKER(_lock);
  _dbserverConfig = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the coordinator config
////////////////////////////////////////////////////////////////////////////////

string ServerState::getCoordinatorConfig () {
  READ_LOCKER(_lock);
  return _coordinatorConfig;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the coordinator config
////////////////////////////////////////////////////////////////////////////////

void ServerState::setCoordinatorConfig (const string& value) {
  WRITE_LOCKER(_lock);
  _coordinatorConfig = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the disable dispatcher frontend flag
////////////////////////////////////////////////////////////////////////////////

bool ServerState::getDisableDispatcherFrontend () {
  READ_LOCKER(_lock);
  return _disableDispatcherFrontend;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the disable dispatcher frontend flag
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDisableDispatcherFrontend (bool value) {
  WRITE_LOCKER(_lock);
  _disableDispatcherFrontend = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the disable dispatcher kickstarter flag
////////////////////////////////////////////////////////////////////////////////

bool ServerState::getDisableDispatcherKickstarter () {
  READ_LOCKER(_lock);
  return _disableDispatcherKickstarter;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the disable dispatcher kickstarter flag
////////////////////////////////////////////////////////////////////////////////

void ServerState::setDisableDispatcherKickstarter (bool value) {
  WRITE_LOCKER(_lock);
  _disableDispatcherKickstarter = value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the server role by fetching data from the agency
/// Note: this method must be called under the _lock
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::determineRole (std::string const& info,
                                                  std::string& id) {
  if (id.empty()) {
    int res = lookupLocalInfoToId(info, id);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("Could not lookupLocalInfoToId");
      return ServerState::ROLE_UNDEFINED;
    }
    // When we get here, we have have successfully looked up our id
    LOG_DEBUG("Learned my own Id: %s", id.c_str());
    setId(id);
  }

  ServerState::RoleEnum role  = checkServersList(id);
  ServerState::RoleEnum role2 = checkCoordinatorsList(id);

  if (role == ServerState::ROLE_UNDEFINED) {
    // role is still unknown. check if we are a coordinator
    role = role2;
  }
  else {
    // we are a primary or a secondary.
    // now we double-check that we are not a coordinator as well
    if (role2 != ServerState::ROLE_UNDEFINED) {
      role = ServerState::ROLE_UNDEFINED;
    }
  }

  return role;
}

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

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/Coordinators for our id
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::checkCoordinatorsList (std::string const& id) {
  // fetch value at Plan/Coordinators
  // we need to do this to determine the server's role

  const std::string key = "Plan/Coordinators";

  AgencyComm comm;
  AgencyCommResult result;

  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = comm.getValues(key, true);
    }
  }

  if (! result.successful()) {
    const std::string endpoints = AgencyComm::getEndpointsString();

    LOG_TRACE("Could not fetch configuration from agency endpoints (%s): "
              "got status code %d, message: %s, key: %s",
              endpoints.c_str(),
              result._statusCode,
              result.errorMessage().c_str(),
              key.c_str());

    return ServerState::ROLE_UNDEFINED;
  }

  if (! result.parse("Plan/Coordinators/", false)) {
    LOG_TRACE("Got an invalid JSON response for Plan/Coordinators");

    return ServerState::ROLE_UNDEFINED;
  }

  // check if we can find ourselves in the list returned by the agency
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.find(id);

  if (it != result._values.end()) {
    // we are in the list. this means we are a primary server
    return ServerState::ROLE_COORDINATOR;
  }

  return ServerState::ROLE_UNDEFINED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server id by using the local info
////////////////////////////////////////////////////////////////////////////////

int ServerState::lookupLocalInfoToId (std::string const& localInfo,
                                      std::string& id) {
  // fetch value at Plan/DBServers
  // we need to do this to determine the server's role

  const std::string key = "Target/MapLocalToID";

  int count = 0;
  while (++count <= 600) {
    AgencyComm comm;
    AgencyCommResult result;

    {
      AgencyCommLocker locker("Target", "READ");

      if (locker.successful()) {
        result = comm.getValues(key, true);
      }
    }

    if (! result.successful()) {
      const std::string endpoints = AgencyComm::getEndpointsString();

      LOG_DEBUG("Could not fetch configuration from agency endpoints (%s): "
                "got status code %d, message: %s, key: %s",
                endpoints.c_str(),
                result._statusCode,
                result.errorMessage().c_str(),
                key.c_str());
    }
    else {
      result.parse("Target/MapLocalToID/", false);
      std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.find(localInfo);

      if (it != result._values.end()) {
        TRI_json_t const* json = it->second._json;
        Json j(TRI_UNKNOWN_MEM_ZONE, json, Json::NOFREE);
        id = triagens::basics::JsonHelper::getStringValue(json, "ID", "");
        if (id.empty()) {
          LOG_ERROR("ID not set!");
          return TRI_ERROR_CLUSTER_COULD_NOT_DETERMINE_ID;
        }
        std::string description
          = triagens::basics::JsonHelper::getStringValue(json, "Description", "");
        if (! description.empty()) {
          setDescription(description);
        }
        return TRI_ERROR_NO_ERROR;
      }
    }
    sleep(1);
  };
  return TRI_ERROR_CLUSTER_COULD_NOT_DETERMINE_ID;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/DBServers for our id
////////////////////////////////////////////////////////////////////////////////

ServerState::RoleEnum ServerState::checkServersList (std::string const& id) {
  // fetch value at Plan/DBServers
  // we need to do this to determine the server's role

  const std::string key = "Plan/DBServers";

  AgencyComm comm;
  AgencyCommResult result;

  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = comm.getValues(key, true);
    }
  }

  if (! result.successful()) {
    const std::string endpoints = AgencyComm::getEndpointsString();

    LOG_TRACE("Could not fetch configuration from agency endpoints (%s): "
              "got status code %d, message: %s, key: %s",
              endpoints.c_str(),
              result._statusCode,
              result.errorMessage().c_str(),
              key.c_str());

    return ServerState::ROLE_UNDEFINED;
  }

  ServerState::RoleEnum role = ServerState::ROLE_UNDEFINED;

  // check if we can find ourselves in the list returned by the agency
  result.parse("Plan/DBServers/", false);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.find(id);

  if (it != result._values.end()) {
    // we are in the list. this means we are a primary server
    role = ServerState::ROLE_PRIMARY;
  }
  else {
    // check if we are a secondary...
    it = result._values.begin();

    while (it != result._values.end()) {
      const std::string name = triagens::basics::JsonHelper::getStringValue((*it).second._json, "");

      if (name == id) {
        role = ServerState::ROLE_SECONDARY;
        _idOfPrimary = it->first;
        break;
      }

      ++it;
    }
  }

  return role;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
