////////////////////////////////////////////////////////////////////////////////
/// @brief sharding configuration
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationCluster.h"
#include "Rest/Endpoint.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterState.h"
#include "Cluster/ClusterComm.h"
#include "BasicsC/logging.h"

using namespace triagens;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                         class ApplicationCluster
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationCluster::ApplicationCluster () 
  : ApplicationFeature("Sharding"),
    _heartbeat(0),
    _heartbeatInterval(1000),
    _agencyEndpoints(),
    _agencyPrefix(),
    _myId(),
    _myAddress(),
    _enableCluster(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationCluster::~ApplicationCluster () {
  if (_heartbeat != 0) {
    // flat line.....
    delete _heartbeat;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationCluster::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options["Cluster options:help-cluster"]
    ("cluster.agency-endpoint", &_agencyEndpoints, "agency endpoint to connect to")
    ("cluster.agency-prefix", &_agencyPrefix, "agency prefix")
    ("cluster.heartbeat-interval", &_heartbeatInterval, "heartbeat interval (in ms)")
    ("cluster.my-id", &_myId, "this server's id")
    ("cluster.my-address", &_myAddress, "this server's endpoint")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare validate the startup options
////////////////////////////////////////////////////////////////////////////////

bool ApplicationCluster::prepare () {
  _enableCluster = (_agencyEndpoints.size() > 0 || ! _agencyPrefix.empty());

  if (! enabled()) {
    return true;
  }

  // validate --cluster.agency-prefix
  size_t found = _agencyPrefix.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/");

  if (found != std::string::npos || _agencyPrefix.empty()) {
    LOG_FATAL_AND_EXIT("invalid value specified for --cluster.agency-prefix");
  }

  // register the prefix with the communicator
  AgencyComm::setPrefix(_agencyPrefix);

  
  // validate --cluster.agency-endpoint
  if (_agencyEndpoints.size() == 0) {
    LOG_FATAL_AND_EXIT("must at least specify one endpoint in --cluster.agency-endpoint");
  }

  for (size_t i = 0; i < _agencyEndpoints.size(); ++i) {
    const string unified = triagens::rest::Endpoint::getUnifiedForm(_agencyEndpoints[i]);

    if (unified.empty()) {
      LOG_FATAL_AND_EXIT("invalid endpoint '%s' specified for --cluster.agency-endpoint", 
                         _agencyEndpoints[i].c_str());
    }

    AgencyComm::addEndpoint(unified);
  }

  // validate --cluster.my-id
  if (_myId.empty()) {
    LOG_FATAL_AND_EXIT("invalid value specified for --cluster.my-id");
  }
  else {
    size_t found = _myId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    
    if (found != std::string::npos) {
      LOG_FATAL_AND_EXIT("invalid value specified for --cluster.my-id");
    }
  }

  // validate --cluster.heartbeat-interval
  if (_heartbeatInterval < 10) {
    LOG_FATAL_AND_EXIT("invalid value specified for --cluster.heartbeat-interval");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationCluster::start () {
  if (! enabled()) {
    return true;
  }
  
  ServerState::instance()->setId(_myId);
  
  // perfom an initial connect to the agency
  const std::string endpoints = AgencyComm::getEndpointsString();

  if (! AgencyComm::tryConnect()) {
    LOG_FATAL_AND_EXIT("Could not connect to agency endpoints (%s)", 
                       endpoints.c_str());
  }


  ServerState::RoleEnum role = checkServersList();

  if (role == ServerState::ROLE_UNDEFINED) {
    // role is still unknown. check if we are a coordinator
    role = checkCoordinatorsList();
  }
  else {
    // we are a primary or a secondary.
    // now we double-check that we are not a coordinator as well
    if (checkCoordinatorsList() != ServerState::ROLE_UNDEFINED) {
      role = ServerState::ROLE_UNDEFINED;
    }
  }

  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG_FATAL_AND_EXIT("unable to determine unambiguous role for server '%s'. No role configured in agency (%s)", 
                       _myId.c_str(),
                       endpoints.c_str());
  }

  // check if my-address is set
  if (_myAddress.empty()) {
    // no address given, now ask the agency for out address
    _myAddress = getEndpointForId();
  }
    
  if (_myAddress.empty()) {
    LOG_FATAL_AND_EXIT("unable to determine internal address for server '%s'. "
                       "Please specify --cluster.my-address or configure the address for this server in the agency.", 
                       _myId.c_str());
  }
  
  // register our own address
  ServerState::instance()->setAddress(_myAddress);
  
  // now we can validate --cluster.my-address
  const string unified = triagens::rest::Endpoint::getUnifiedForm(_myAddress);

  if (unified.empty()) {
    LOG_FATAL_AND_EXIT("invalid endpoint '%s' specified for --cluster.my-address", 
                       _myAddress.c_str());
  }

  ServerState::instance()->setRole(role);
  ServerState::instance()->setState(ServerState::STATE_STARTUP);
 
  // the agency about our state 
  AgencyComm comm;
  comm.sendServerState();

  const std::string version = comm.getVersion();

  LOG_INFO("Cluster feature is turned on. "
           "Agency version: %s, Agency endpoints: %s, "
           "server id: '%s', internal address: %s, role: %s",
           version.c_str(),
           endpoints.c_str(),
           _myId.c_str(),
           _myAddress.c_str(),
           ServerState::roleToString(role).c_str());

  // start heartbeat thread
  _heartbeat = new HeartbeatThread(_heartbeatInterval * 1000, 5);

  if (_heartbeat == 0) {
    LOG_FATAL_AND_EXIT("unable to start cluster heartbeat thread");
  }

  if (! _heartbeat->init() || ! _heartbeat->start()) {
    LOG_FATAL_AND_EXIT("heartbeat could not connect to agency endpoints (%s)", 
                       endpoints.c_str());
  }
 
  // initialise ClusterState class
  ClusterState::instance()->initialise();

  // initialise ClusterComm library
  ClusterComm::instance()->initialise();

  return true;
}
  
////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationCluster::open () {
  if (! enabled()) {
    return true;
  }

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  // tell the agency that we are ready
  AgencyComm comm;
  AgencyCommResult result = comm.setValue("Current/ServersRegistered/" + _myId, _myAddress);

  if (! result.successful()) {
    LOG_FATAL_AND_EXIT("unable to register server in agency");
  }

  if (role == ServerState::ROLE_COORDINATOR) {
    ServerState::instance()->setState(ServerState::STATE_SERVING);
  
    // register coordinator
    AgencyCommResult result = comm.setValue("Current/Coordinators/" + _myId, "none");
    if (! result.successful()) {
      LOG_FATAL_AND_EXIT("unable to register coordinator in agency");
    }
  }
  else if (role == ServerState::ROLE_PRIMARY) {
    ServerState::instance()->setState(ServerState::STATE_SERVINGASYNC);

    // register server
    AgencyCommResult result = comm.setValue("Current/DBServers/" + _myId, "none");
    if (! result.successful()) {
      LOG_FATAL_AND_EXIT("unable to register db server in agency");
    }
  }
  else if (role == ServerState::ROLE_SECONDARY) {
    LOG_FATAL_AND_EXIT("secondary server tasks are currently not implemented");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationCluster::close () {
  if (! enabled()) {
    return;
  }
  
  _heartbeat->stop();
  
  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
  comm.sendServerState();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationCluster::stop () {
  if (! enabled()) {
    return;
  }
  
  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
  comm.sendServerState();

  _heartbeat->stop();
 
  // unregister ourselves 
  comm.removeValues("Current/ServersRegistered/" + _myId, false);
  
  ClusterComm::cleanup();
  ClusterState::cleanup();
  AgencyComm::cleanup();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server's endpoint by scanning Target/MapIDToEnpdoint for 
/// our id
////////////////////////////////////////////////////////////////////////////////
  
std::string ApplicationCluster::getEndpointForId () const {
  // fetch value at Target/MapIDToEndpoint
  AgencyComm comm;
  AgencyCommResult result = comm.getValues("Target/MapIDToEndpoint/" + _myId, false);
 
  if (result.successful()) {
    std::map<std::string, std::string> out;

    if (! result.flattenJson(out, "Target/MapIDToEndpoint/", false)) {
      LOG_FATAL_AND_EXIT("Got an invalid JSON response for Target/MapIDToEndpoint");
    }

    // check if we can find ourselves in the list returned by the agency
    std::map<std::string, std::string>::const_iterator it = out.find(_myId);

    if (it != out.end()) {
      LOG_TRACE("using remote value '%s' for --cluster.my-address", 
                (*it).second.c_str());

      return (*it).second;
    }
  }

  // not found
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/Coordinators for our id
////////////////////////////////////////////////////////////////////////////////
  
ServerState::RoleEnum ApplicationCluster::checkCoordinatorsList () const {
  // fetch value at Plan/Coordinators
  // we need this to determine the server's role
  const std::string key = "Plan/Coordinators";

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, true);
 
  if (! result.successful()) {
    const std::string endpoints = AgencyComm::getEndpointsString();

    LOG_FATAL_AND_EXIT("Could not fetch configuration from agency endpoints (%s): "
                       "got status code %d, message: %s, key: %s",
                       endpoints.c_str(), 
                       result._statusCode,
                       result.errorMessage().c_str(),
                       key.c_str());
  }
  
  std::map<std::string, std::string> out;
  if (! result.flattenJson(out, "Plan/Coordinators/", false)) {
    LOG_FATAL_AND_EXIT("Got an invalid JSON response for Plan/Coordinators");
  }

  // check if we can find ourselves in the list returned by the agency
  std::map<std::string, std::string>::const_iterator it = out.find(_myId);

  if (it != out.end()) {
    // we are in the list. this means we are a primary server
    return ServerState::ROLE_COORDINATOR;
  }

  return ServerState::ROLE_UNDEFINED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/DBServers for our id
////////////////////////////////////////////////////////////////////////////////
  
ServerState::RoleEnum ApplicationCluster::checkServersList () const {
  // fetch value at Plan/DBServers
  // we need this to determine the server's role
  const std::string key = "Plan/DBServers";

  AgencyComm comm;
  AgencyCommResult result = comm.getValues(key, true);
 
  if (! result.successful()) {
    const std::string endpoints = AgencyComm::getEndpointsString();

    LOG_FATAL_AND_EXIT("Could not fetch configuration from agency endpoints (%s): "
                       "got status code %d, message: %s, key: %s", 
                       endpoints.c_str(), 
                       result._statusCode,
                       result.errorMessage().c_str(),
                       key.c_str());
  }
 
  std::map<std::string, std::string> out;
  if (! result.flattenJson(out, "Plan/DBServers/", false)) {
    LOG_FATAL_AND_EXIT("Got an invalid JSON response for Plan/DBServers");
  }

  ServerState::RoleEnum role = ServerState::ROLE_UNDEFINED;

  // check if we can find ourselves in the list returned by the agency
  std::map<std::string, std::string>::const_iterator it = out.find(_myId);

  if (it != out.end()) {
    // we are in the list. this means we are a primary server
    role = ServerState::ROLE_PRIMARY;
  }
  else {
    // check if we are a secondary...
    it = out.begin();

    while (it != out.end()) {
      const std::string value = (*it).second;
      if (value == _myId) {
        role = ServerState::ROLE_SECONDARY;
        break;
      }

      ++it;
    }
  }

  return role;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
