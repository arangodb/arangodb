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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationCluster.h"
#include "Rest/Endpoint.h"
#include "Basics/JsonHelper.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/Logger.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "SimpleHttpClient/ConnectionManager.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/server.h"

using namespace arangodb;
using namespace arangodb::basics;

ApplicationCluster::ApplicationCluster(
    TRI_server_t* server, arangodb::rest::ApplicationDispatcher* dispatcher,
    ApplicationV8* applicationV8)
    : ApplicationFeature("Sharding"),
      _server(server),
      _dispatcher(dispatcher),
      _applicationV8(applicationV8),
      _heartbeat(nullptr),
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
  TRI_ASSERT(_dispatcher != nullptr);
}

ApplicationCluster::~ApplicationCluster() {
  delete _heartbeat;

  // delete connection manager instance
  auto cm = httpclient::ConnectionManager::instance();
  delete cm;
}

void ApplicationCluster::setupOptions(
    std::map<std::string, basics::ProgramOptionsDescription>& options) {
  options["Cluster options:help-cluster"]("cluster.agency-endpoint",
                                          &_agencyEndpoints,
                                          "agency endpoint to connect to")(
      "cluster.agency-prefix", &_agencyPrefix, "agency prefix")(
      "cluster.my-local-info", &_myLocalInfo, "this server's local info")(
      "cluster.my-id", &_myId, "this server's id")(
      "cluster.my-address", &_myAddress, "this server's endpoint")(
      "cluster.username", &_username,
      "username used for cluster-internal communication")(
      "cluster.password", &_password,
      "password used for cluster-internal communication")(
      "cluster.data-path", &_dataPath, "path to cluster database directory")(
      "cluster.log-path", &_logPath, "path to log directory for the cluster")(
      "cluster.agent-path", &_agentPath, "path to the agent for the cluster")(
      "cluster.arangod-path", &_arangodPath,
      "path to the arangod for the cluster")(
      "cluster.dbserver-config", &_dbserverConfig,
      "path to the DBserver configuration")(
      "cluster.coordinator-config", &_coordinatorConfig,
      "path to the coordinator configuration")(
      "cluster.disable-dispatcher-frontend", &_disableDispatcherFrontend,
      "do not show the dispatcher interface")(
      "cluster.disable-dispatcher-kickstarter", &_disableDispatcherKickstarter,
      "disable the kickstarter functionality");
}

bool ApplicationCluster::prepare() {
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
  ServerState::instance()->setDisableDispatcherFrontend(
      _disableDispatcherFrontend);
  ServerState::instance()->setDisableDispatcherKickstarter(
      _disableDispatcherKickstarter);

  // initialize ConnectionManager library
  httpclient::ConnectionManager::initialize();

  // initialize ClusterComm library
  // must call initialize while still single-threaded
  ClusterComm::initialize();

  if (_disabled) {
    // if ApplicationFeature is disabled
    _enableCluster = false;
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
    return true;
  }

  // check the cluster state
  _enableCluster = !_agencyEndpoints.empty();

  if (_agencyPrefix.empty()) {
    _agencyPrefix = "arango";
  }

  if (!enabled()) {
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
    return true;
  }

  ServerState::instance()->setClusterEnabled();

  // validate --cluster.agency-prefix
  size_t found = _agencyPrefix.find_first_not_of(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/");

  if (found != std::string::npos || _agencyPrefix.empty()) {
    LOG(FATAL) << "invalid value specified for --cluster.agency-prefix"; FATAL_ERROR_EXIT();
  }

  // register the prefix with the communicator
  AgencyComm::setPrefix(_agencyPrefix);

  // validate --cluster.agency-endpoint
  if (_agencyEndpoints.empty()) {
    LOG(FATAL) << "must at least specify one endpoint in --cluster.agency-endpoint"; FATAL_ERROR_EXIT();
  }

  for (size_t i = 0; i < _agencyEndpoints.size(); ++i) {
    std::string const unified =
        arangodb::rest::Endpoint::getUnifiedForm(_agencyEndpoints[i]);

    if (unified.empty()) {
      LOG(FATAL) << "invalid endpoint '" << _agencyEndpoints[i].c_str() << "' specified for --cluster.agency-endpoint"; FATAL_ERROR_EXIT();
    }

    AgencyComm::addEndpoint(unified);
  }

  // validate --cluster.my-id
  if (_myId.empty()) {
    if (_myLocalInfo.empty()) {
      LOG(FATAL) << "invalid value specified for --cluster.my-id and --cluster.my-local-info"; FATAL_ERROR_EXIT();
    }
    if (_myAddress.empty()) {
      LOG(FATAL) << "must specify --cluster.my-address if --cluster.my-id is empty"; FATAL_ERROR_EXIT();
    }
  } else {
    size_t found = _myId.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    if (found != std::string::npos) {
      LOG(FATAL) << "invalid value specified for --cluster.my-id"; FATAL_ERROR_EXIT();
    }
  }
  // Now either _myId is set properly or _myId is empty and _myLocalInfo and
  // _myAddress are set.
  if (!_myAddress.empty()) {
    ServerState::instance()->setAddress(_myAddress);
  }

  // initialize ConnectionManager library
  // httpclient::ConnectionManager::initialize();

  // initialize ClusterComm library
  // must call initialize while still single-threaded
  // ClusterComm::initialize();

  // disable error logging for a while
  ClusterComm::instance()->enableConnectionErrorLogging(false);

  // perform an initial connect to the agency
  std::string const endpoints = AgencyComm::getEndpointsString();

  if (!AgencyComm::tryConnect()) {
    LOG(FATAL) << "Could not connect to agency endpoints (" << endpoints.c_str() << ")"; FATAL_ERROR_EXIT();
  }

  ServerState::instance()->setLocalInfo(_myLocalInfo);
  if (!_myId.empty()) {
    ServerState::instance()->setId(_myId);
  }

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG(FATAL) << "unable to determine unambiguous role for server '" << _myId.c_str() << "'. No role configured in agency (" << endpoints.c_str() << ")"; FATAL_ERROR_EXIT();
  }

  if (_myId.empty()) {
    _myId = ServerState::instance()->getId();  // has been set by getRole!
  }

  // check if my-address is set
  if (_myAddress.empty()) {
    // no address given, now ask the agency for our address
    _myAddress = ServerState::instance()->getAddress();
  }
  // if nonempty, it has already been set above

  // If we are a coordinator, we wait until at least one DBServer is there,
  // otherwise we can do very little, in particular, we cannot create
  // any collection:
  if (role == ServerState::ROLE_COORDINATOR) {
    ClusterInfo* ci = ClusterInfo::instance();
    do {
      LOG(INFO) << "Waiting for a DBserver to show up...";
      ci->loadCurrentDBServers();
      std::vector<ServerID> DBServers = ci->getCurrentDBServers();
      if (DBServers.size() > 0) {
        LOG(INFO) << "Found a DBserver.";
        break;
      }
      sleep(1);
    } while (true);
  }

  return true;
}

bool ApplicationCluster::start() {
  if (!enabled()) {
    return true;
  }

  std::string const endpoints = AgencyComm::getEndpointsString();

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  if (_myAddress.empty()) {
    LOG(FATAL) << "unable to determine internal address for server '" << _myId.c_str() << "'. Please specify --cluster.my-address or configure the address for this server in the agency."; FATAL_ERROR_EXIT();
  }

  // now we can validate --cluster.my-address
  std::string const unified =
      arangodb::rest::Endpoint::getUnifiedForm(_myAddress);

  if (unified.empty()) {
    LOG(FATAL) << "invalid endpoint '" << _myAddress.c_str() << "' specified for --cluster.my-address"; FATAL_ERROR_EXIT();
  }

  ServerState::instance()->setState(ServerState::STATE_STARTUP);

  // the agency about our state
  AgencyComm comm;
  comm.sendServerState(0.0);

  std::string const version = comm.getVersion();

  ServerState::instance()->setInitialized();

  LOG(INFO) << "Cluster feature is turned on. Agency version: " << version.c_str() << ", Agency endpoints: " << endpoints.c_str() << ", server id: '" << _myId.c_str() << "', internal address: " << _myAddress.c_str() << ", role: " << ServerState::roleToString(role).c_str();

  if (!_disableHeartbeat) {
    AgencyCommResult result = comm.getValues("Sync/HeartbeatIntervalMs", false);

    if (result.successful()) {
      result.parse("", false);

      std::map<std::string, AgencyCommResultEntry>::const_iterator it =
          result._values.begin();

      if (it != result._values.end()) {
        VPackSlice slice = (*it).second._vpack->slice();
        _heartbeatInterval =
            arangodb::basics::VelocyPackHelper::stringUInt64(slice);

        LOG(INFO) << "using heartbeat interval value '" << _heartbeatInterval << " ms' from agency";
      }
    }

    // no value set in agency. use default
    if (_heartbeatInterval == 0) {
      _heartbeatInterval = 5000; // 1/s

      LOG(WARN) << "unable to read heartbeat interval from agency. Using default value '" << _heartbeatInterval << " ms'";
    }

    // start heartbeat thread
    _heartbeat = new HeartbeatThread(_server, _dispatcher, _applicationV8,
                                     _heartbeatInterval * 1000, 5);

    if (_heartbeat == nullptr) {
      LOG(FATAL) << "unable to start cluster heartbeat thread"; FATAL_ERROR_EXIT();
    }

    if (!_heartbeat->init() || !_heartbeat->start()) {
      LOG(FATAL) << "heartbeat could not connect to agency endpoints (" << endpoints.c_str() << ")"; FATAL_ERROR_EXIT();
    }

    while (!_heartbeat->isReady()) {
      // wait until heartbeat is ready
      usleep(10000);
    }
  }

  return true;
}

bool ApplicationCluster::open() {
  if (!enabled()) {
    return true;
  }

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  // tell the agency that we are ready
  {
    AgencyComm comm;
    AgencyCommResult result;

    AgencyCommLocker locker("Current", "WRITE");

    if (locker.successful()) {
      VPackBuilder builder;
      try {
        VPackObjectBuilder b(&builder);
        builder.add("endpoint", VPackValue(_myAddress));
      } catch (...) {
        locker.unlock();
        LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
      }

      result = comm.setValue("Current/ServersRegistered/" + _myId, builder.slice(), 0.0);
    }

    if (!result.successful()) {
      locker.unlock();
      LOG(FATAL) << "unable to register server in agency: http code: " << result.httpCode() << ", body: " << result.body().c_str(); FATAL_ERROR_EXIT();
    }

    if (role == ServerState::ROLE_COORDINATOR) {
      VPackBuilder builder;
      try {
        builder.add(VPackValue("none"));
      } catch (...) {
        locker.unlock();
        LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
      }

      ServerState::instance()->setState(ServerState::STATE_SERVING);

      // register coordinator
      AgencyCommResult result =
          comm.setValue("Current/Coordinators/" + _myId, builder.slice(), 0.0);

      if (!result.successful()) {
        locker.unlock();
        LOG(FATAL) << "unable to register coordinator in agency"; FATAL_ERROR_EXIT();
      }
    } else if (role == ServerState::ROLE_PRIMARY) {
      VPackBuilder builder;
      try {
        builder.add(VPackValue("none"));
      } catch (...) {
        locker.unlock();
        LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
      }

      ServerState::instance()->setState(ServerState::STATE_SERVINGASYNC);

      // register server
      AgencyCommResult result =
          comm.setValue("Current/DBServers/" + _myId, builder.slice(), 0.0);

      if (!result.successful()) {
        locker.unlock();
        LOG(FATAL) << "unable to register db server in agency"; FATAL_ERROR_EXIT();
      }
    } else if (role == ServerState::ROLE_SECONDARY) {
      std::string keyName = std::string("\"") + _myId + std::string("\"");
      VPackBuilder builder;
      try {
        builder.add(VPackValue(keyName));
      } catch (...) {
        locker.unlock();
        LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
      }

      ServerState::instance()->setState(ServerState::STATE_SYNCING);

      // register server
      AgencyCommResult result = comm.setValue(
          "Current/DBServers/" + ServerState::instance()->getPrimaryId(),
          builder.slice(), 0.0);

      if (!result.successful()) {
        locker.unlock();
        LOG(FATAL) << "unable to register secondary db server in agency"; FATAL_ERROR_EXIT();
      }
    }
  }

  return true;
}

void ApplicationCluster::close() {
  if (!enabled()) {
    return;
  }

  if (_heartbeat != nullptr) {
    _heartbeat->stop();
  }

  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
  comm.sendServerState(0.0);
}

void ApplicationCluster::stop() {
  ClusterComm::cleanup();
  if (!enabled()) {
    return;
  }

  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
  comm.sendServerState(0.0);

  if (_heartbeat != nullptr) {
    _heartbeat->stop();
  }

  {
    AgencyCommLocker locker("Current", "WRITE");

    if (locker.successful()) {
      // unregister ourselves
      ServerState::RoleEnum role = ServerState::instance()->getRole();

      if (role == ServerState::ROLE_PRIMARY) {
        comm.removeValues("Current/DBServers/" + _myId, false);
      } else if (role == ServerState::ROLE_COORDINATOR) {
        comm.removeValues("Current/Coordinators/" + _myId, false);
      }

      // unregister ourselves
      comm.removeValues("Current/ServersRegistered/" + _myId, false);
    }
  }

  // ClusterComm::cleanup();
  AgencyComm::cleanup();
}
