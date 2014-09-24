////////////////////////////////////////////////////////////////////////////////
/// @brief sharding configuration
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationCluster.h"
#include "Rest/Endpoint.h"
#include "Basics/JsonHelper.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/logging.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "SimpleHttpClient/ConnectionManager.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/server.h"

using namespace triagens;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                          class ApplicationCluster
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationCluster::ApplicationCluster (TRI_server_t* server,
                                        triagens::rest::ApplicationDispatcher* dispatcher,
                                        ApplicationV8* applicationV8)
  : ApplicationFeature("Sharding"),
    _server(server),
    _dispatcher(dispatcher),
    _applicationV8(applicationV8),
    _heartbeat(0),
    _heartbeatInterval(0),
    _agencyEndpoints(),
    _agencyPrefix(),
    _myId(),
    _myAddress(),
    _username("root"),
    _password(),
    _dataPath(),
    _logPath(),
    _agentPath(),
    _arangodPath(),
    _dbserverConfig(),
    _coordinatorConfig(),
    _disableDispatcherFrontend(true),
    _disableDispatcherKickstarter(true),
    _enableCluster(false),
    _disableHeartbeat(false) {

  TRI_ASSERT(_dispatcher != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationCluster::~ApplicationCluster () {
  if (_heartbeat != 0) {
    // flat line.....
    delete _heartbeat;
  }

  ServerState::cleanup();
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
    ("cluster.my-id", &_myId, "this server's id")
    ("cluster.my-address", &_myAddress, "this server's endpoint")
    ("cluster.username", &_username, "username used for cluster-internal communication")
    ("cluster.password", &_password, "password used for cluster-internal communication")
    ("cluster.data-path", &_dataPath, "path to cluster database directory")
    ("cluster.log-path", &_logPath, "path to log directory for the cluster")
    ("cluster.agent-path", &_agentPath, "path to the agent for the cluster")
    ("cluster.arangod-path", &_arangodPath, "path to the arangod for the cluster")
    ("cluster.dbserver-config", &_dbserverConfig, "path to the DBserver configuration")
    ("cluster.coordinator-config", &_coordinatorConfig, "path to the coordinator configuration")
    ("cluster.disable-dispatcher-frontend", &_disableDispatcherFrontend, "do not show the dispatcher interface")
    ("cluster.disable-dispatcher-kickstarter", &_disableDispatcherKickstarter, "disable the kickstarter functionality")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationCluster::prepare () {

  // initialise ServerState library
  ServerState::initialise();

  // set authentication data
  ServerState::instance()->setAuthentication(_username, _password);

  // overwrite memory area
  _username = _password = "someotherusername";

  ServerState::instance()->setDataPath(_dataPath);
  ServerState::instance()->setLogPath(_logPath);
  ServerState::instance()->setAgentPath(_agentPath);
  ServerState::instance()->setArangodPath(_arangodPath);
  ServerState::instance()->setDBserverConfig(_dbserverConfig);
  ServerState::instance()->setCoordinatorConfig(_coordinatorConfig);
  ServerState::instance()->setDisableDispatcherFrontend(_disableDispatcherFrontend);
  ServerState::instance()->setDisableDispatcherKickstarter(_disableDispatcherKickstarter);

  // check the cluster state
  _enableCluster = (! _agencyEndpoints.empty() || ! _agencyPrefix.empty());

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
  if (_agencyEndpoints.empty()) {
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

  // initialise ClusterInfo library
  ClusterInfo::initialise();

  // initialise ClusterComm library
  ClusterComm::initialise();

  // disable error logging for a while
  ClusterComm::instance()->enableConnectionErrorLogging(false);

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


  ServerState::RoleEnum role = ServerState::instance()->getRole();

  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG_FATAL_AND_EXIT("unable to determine unambiguous role for server '%s'. No role configured in agency (%s)",
                       _myId.c_str(),
                       endpoints.c_str());
  }

  // check if my-address is set
  if (_myAddress.empty()) {
    // no address given, now ask the agency for out address
    _myAddress = ServerState::instance()->getAddress();
  }
  else {
    // register our own address
    ServerState::instance()->setAddress(_myAddress);
  }

  if (_myAddress.empty()) {
    LOG_FATAL_AND_EXIT("unable to determine internal address for server '%s'. "
                       "Please specify --cluster.my-address or configure the address for this server in the agency.",
                       _myId.c_str());
  }

  // now we can validate --cluster.my-address
  const string unified = triagens::rest::Endpoint::getUnifiedForm(_myAddress);

  if (unified.empty()) {
    LOG_FATAL_AND_EXIT("invalid endpoint '%s' specified for --cluster.my-address",
                       _myAddress.c_str());
  }

  ServerState::instance()->setState(ServerState::STATE_STARTUP);

  // initialise ConnectionManager library
  httpclient::ConnectionManager::instance()->initialise();

  // the agency about our state
  AgencyComm comm;
  comm.sendServerState(0.0);

  const std::string version = comm.getVersion();

  ServerState::instance()->setInitialised();

  LOG_INFO("Cluster feature is turned on. "
           "Agency version: %s, Agency endpoints: %s, "
           "server id: '%s', internal address: %s, role: %s",
           version.c_str(),
           endpoints.c_str(),
           _myId.c_str(),
           _myAddress.c_str(),
           ServerState::roleToString(role).c_str());

  if (! _disableHeartbeat) {
    AgencyCommResult result = comm.getValues("Sync/HeartbeatIntervalMs", false);

    if (result.successful()) {
      result.parse("", false);

      std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

      if (it != result._values.end()) {
        _heartbeatInterval = triagens::basics::JsonHelper::stringUInt64((*it).second._json);

        LOG_INFO("using heartbeat interval value '%llu ms' from agency",
                 (unsigned long long) _heartbeatInterval);
      }
    }

    // no value set in agency. use default
    if (_heartbeatInterval == 0) {
      _heartbeatInterval = 1000; // 1/s

      LOG_WARNING("unable to read heartbeat interval from agency. Using default value '%llu ms'",
                  (unsigned long long) _heartbeatInterval);
    }


    // start heartbeat thread
    _heartbeat = new HeartbeatThread(_server, _dispatcher, _applicationV8, _heartbeatInterval * 1000, 5);

    if (_heartbeat == 0) {
      LOG_FATAL_AND_EXIT("unable to start cluster heartbeat thread");
    }

    if (! _heartbeat->init() || ! _heartbeat->start()) {
      LOG_FATAL_AND_EXIT("heartbeat could not connect to agency endpoints (%s)",
                         endpoints.c_str());
    }

    while (! _heartbeat->ready()) {
      // wait until heartbeat is ready
      usleep(10000);
    }
  }

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
  {
    AgencyComm comm;
    AgencyCommResult result;

    AgencyCommLocker locker("Current", "WRITE");

    if (locker.successful()) {
      TRI_json_t* ep = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, _myAddress.c_str(), _myAddress.size());
      if (ep == 0) {
        locker.unlock();
        LOG_FATAL_AND_EXIT("out of memory");
      }
      TRI_json_t* json = TRI_CreateArray2Json(TRI_UNKNOWN_MEM_ZONE, 1);
      if (json == 0) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, ep);
        locker.unlock();
        LOG_FATAL_AND_EXIT("out of memory");
      }
      TRI_Insert2ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "endpoint", ep);

      result = comm.setValue("Current/ServersRegistered/" + _myId, json, 0.0);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }

    if (! result.successful()) {
      locker.unlock();
      LOG_FATAL_AND_EXIT("unable to register server in agency: http code: %d, body: %s",
                         (int) result.httpCode(),
                         result.body().c_str());
    }

    if (role == ServerState::ROLE_COORDINATOR) {
      TRI_json_t* json = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, "none", 4);

      if (json == 0) {
        locker.unlock();
        LOG_FATAL_AND_EXIT("out of memory");
      }

      ServerState::instance()->setState(ServerState::STATE_SERVING);

      // register coordinator
      AgencyCommResult result = comm.setValue("Current/Coordinators/" + _myId, json, 0.0);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      if (! result.successful()) {
        locker.unlock();
        LOG_FATAL_AND_EXIT("unable to register coordinator in agency");
      }
    }
    else if (role == ServerState::ROLE_PRIMARY) {
      TRI_json_t* json = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, "none", 4);

      if (json == 0) {
        locker.unlock();
        LOG_FATAL_AND_EXIT("out of memory");
      }

      ServerState::instance()->setState(ServerState::STATE_SERVINGASYNC);

      // register server
      AgencyCommResult result = comm.setValue("Current/DBServers/" + _myId, json, 0.0);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      if (! result.successful()) {
        locker.unlock();
        LOG_FATAL_AND_EXIT("unable to register db server in agency");
      }
    }
    else if (role == ServerState::ROLE_SECONDARY) {
      locker.unlock();
      LOG_FATAL_AND_EXIT("secondary server tasks are currently not implemented");
    }
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

  if (_heartbeat != 0) {
    _heartbeat->stop();
  }

  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
  comm.sendServerState(0.0);
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
  comm.sendServerState(0.0);

  if (_heartbeat != 0) {
    _heartbeat->stop();
  }

  {
    AgencyCommLocker locker("Current", "WRITE");

    if (locker.successful()) {
      // unregister ourselves
      ServerState::RoleEnum role = ServerState::instance()->getRole();

      if (role == ServerState::ROLE_PRIMARY) {
        comm.removeValues("Current/DBServers/" + _myId, false);
      }
      else if (role == ServerState::ROLE_COORDINATOR) {
        comm.removeValues("Current/Coordinators/" + _myId, false);
      }

      // unregister ourselves
      comm.removeValues("Current/ServersRegistered/" + _myId, false);
    }
  }

  ClusterComm::cleanup();
  ClusterInfo::cleanup();
  AgencyComm::cleanup();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
