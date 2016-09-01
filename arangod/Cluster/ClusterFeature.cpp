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

#include "ClusterFeature.h"

#include "Agency/AgencyFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Endpoint/Endpoint.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/ConnectionManager.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

ClusterFeature::ClusterFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Cluster"),
      _username("root"),
      _unregisterOnShutdown(false),
      _enableCluster(false),
      _heartbeatThread(nullptr),
      _heartbeatInterval(0),
      _disableHeartbeat(false),
      _agencyCallbackRegistry(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  startsAfter("WorkMonitor");
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("Scheduler");
  startsAfter("V8Dealer");
  startsAfter("Database");
}

ClusterFeature::~ClusterFeature() {
  delete _heartbeatThread;

  if (_enableCluster) {
    AgencyComm::cleanup();
  }

  // delete connection manager instance
  auto cm = httpclient::ConnectionManager::instance();
  delete cm;
}

void ClusterFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("cluster", "Configure the cluster");

  options->addOption("--cluster.agency-endpoint",
                     "agency endpoint to connect to",
                     new VectorParameter<StringParameter>(&_agencyEndpoints));

  options->addOption("--cluster.agency-prefix", "agency prefix",
                     new StringParameter(&_agencyPrefix));

  options->addOption("--cluster.my-local-info", "this server's local info",
                     new StringParameter(&_myLocalInfo));

  options->addOption("--cluster.my-id", "this server's id",
                     new StringParameter(&_myId));

  options->addOption("--cluster.my-role", "this server's role",
                     new StringParameter(&_myRole));

  options->addOption("--cluster.my-address", "this server's endpoint",
                     new StringParameter(&_myAddress));

  options->addOption("--cluster.username",
                     "username used for cluster-internal communication",
                     new StringParameter(&_username));

  options->addOption("--cluster.password",
                     "password used for cluster-internal communication",
                     new StringParameter(&_password));

  options->addOption("--cluster.data-path",
                     "path to cluster database directory",
                     new StringParameter(&_dataPath));

  options->addOption("--cluster.log-path",
                     "path to log directory for the cluster",
                     new StringParameter(&_logPath));

  options->addOption("--cluster.arangod-path",
                     "path to the arangod for the cluster",
                     new StringParameter(&_arangodPath));

  options->addOption("--cluster.dbserver-config",
                     "path to the DBserver configuration",
                     new StringParameter(&_dbserverConfig));

  options->addOption("--cluster.coordinator-config",
                     "path to the coordinator configuration",
                     new StringParameter(&_coordinatorConfig));

  options->addOption("--cluster.system-replication-factor",
                     "replication factor for system collections",
                     new UInt32Parameter(&_systemReplicationFactor));
}

void ClusterFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check if the cluster is enabled
  _enableCluster = !_agencyEndpoints.empty();

  if (!_enableCluster) {
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
    return;
  }

  // validate --cluster.agency-endpoint (currently a noop)
  if (_agencyEndpoints.empty()) {
    LOG(FATAL)
        << "must at least specify one endpoint in --cluster.agency-endpoint";
    FATAL_ERROR_EXIT();
  }

  // validate
  if (_agencyPrefix.empty()) {
    _agencyPrefix = "arango";
  }

  // validate --cluster.agency-prefix
  size_t found = _agencyPrefix.find_first_not_of(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/");

  if (found != std::string::npos || _agencyPrefix.empty()) {
    LOG(FATAL) << "invalid value specified for --cluster.agency-prefix";
    FATAL_ERROR_EXIT();
  }

  // validate --cluster.my-id
  if (_myId.empty()) {
    if (_myLocalInfo.empty()) {
      LOG(FATAL) << "Need to specify a local cluster identifier via "
                    "--cluster.my-local-info";
      FATAL_ERROR_EXIT();
    }

    if (_myAddress.empty()) {
      LOG(FATAL)
          << "must specify --cluster.my-address if --cluster.my-id is empty";
      FATAL_ERROR_EXIT();
    }
  } else {
    size_t found = _myId.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    if (found != std::string::npos) {
      LOG(FATAL) << "invalid value specified for --cluster.my-id";
      FATAL_ERROR_EXIT();
    }
  }

  // validate system-replication-factor
  if (_systemReplicationFactor == 0) {
    LOG(FATAL) << "system replication factor must be greater 0";
    FATAL_ERROR_EXIT();
  }
}

void ClusterFeature::prepare() {
  ServerState::instance()->setAuthentication(_username, _password);
  ServerState::instance()->setDataPath(_dataPath);
  ServerState::instance()->setLogPath(_logPath);
  ServerState::instance()->setArangodPath(_arangodPath);
  ServerState::instance()->setDBserverConfig(_dbserverConfig);
  ServerState::instance()->setCoordinatorConfig(_coordinatorConfig);

  V8DealerFeature* v8Dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  v8Dealer->defineDouble("SYS_DEFAULT_REPLICATION_FACTOR_SYSTEM",
                         _systemReplicationFactor);

  // create callback registery
  _agencyCallbackRegistry.reset(
      new AgencyCallbackRegistry(agencyCallbacksPath()));

  // Initialize ClusterInfo library:
  ClusterInfo::createInstance(_agencyCallbackRegistry.get());

  // initialize ConnectionManager library
  httpclient::ConnectionManager::initialize();

  // create an instance (this will not yet create a thread)
  ClusterComm::instance();

  AgencyFeature* agency =
      application_features::ApplicationServer::getFeature<AgencyFeature>(
          "Agency");

  if (agency->isEnabled() || _enableCluster) {
    // initialize ClusterComm library, must call initialize only once
    ClusterComm::initialize();
  }

  // return if cluster is disabled
  if (!_enableCluster) {
    return;
  }

  ServerState::instance()->setClusterEnabled();

  // register the prefix with the communicator
  AgencyComm::setPrefix(_agencyPrefix);

  for (size_t i = 0; i < _agencyEndpoints.size(); ++i) {
    std::string const unified = Endpoint::unifiedForm(_agencyEndpoints[i]);

    if (unified.empty()) {
      LOG(FATAL) << "invalid endpoint '" << _agencyEndpoints[i]
                 << "' specified for --cluster.agency-endpoint";
      FATAL_ERROR_EXIT();
    }

    AgencyComm::addEndpoint(unified);
  }

  // Now either _myId is set properly or _myId is empty and _myLocalInfo and
  // _myAddress are set.
  if (!_myAddress.empty()) {
    ServerState::instance()->setAddress(_myAddress);
  }

  // disable error logging for a while
  ClusterComm::instance()->enableConnectionErrorLogging(false);

  // perform an initial connect to the agency
  std::string const endpoints = AgencyComm::getEndpointsString();

  if (!AgencyComm::initialize()) {
    LOG(FATAL) << "Could not connect to agency endpoints (" << endpoints << ")";
    FATAL_ERROR_EXIT();
  }

  ServerState::instance()->setLocalInfo(_myLocalInfo);

  if (!_myId.empty()) {
    ServerState::instance()->setId(_myId);
  }

  if (!_myRole.empty()) {
    ServerState::RoleEnum role = ServerState::stringToRole(_myRole);

    if (role == ServerState::ROLE_SINGLE ||
        role == ServerState::ROLE_UNDEFINED) {
      LOG(FATAL) << "Invalid role provided. Possible values: PRIMARY, "
                    "SECONDARY, COORDINATOR";
      FATAL_ERROR_EXIT();
    }

    if (!ServerState::instance()->registerWithRole(role)) {
      LOG(FATAL) << "Couldn't register at agency.";
      FATAL_ERROR_EXIT();
    }
  }

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG(FATAL) << "unable to determine unambiguous role for server '" << _myId
               << "'. No role configured in agency (" << endpoints << ")";
    FATAL_ERROR_EXIT();
  }

  if (role == ServerState::ROLE_SINGLE) {
    LOG(FATAL) << "determined single-server role for server '" << _myId
               << "'. Please check the configurarion in the agency ("
               << endpoints << ")";
    FATAL_ERROR_EXIT();
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

    double start = TRI_microtime();
    while (true) {
      LOG(INFO) << "Waiting for a DBserver to show up...";
      ci->loadCurrentDBServers();
      std::vector<ServerID> DBServers = ci->getCurrentDBServers();
      if (DBServers.size() > 1 || TRI_microtime() - start > 30.0) {
        LOG(INFO) << "Found " << DBServers.size() << " DBservers.";
        break;
      }

      sleep(1);
    };
  }

  if (_myAddress.empty()) {
    LOG(FATAL) << "unable to determine internal address for server '" << _myId
               << "'. Please specify --cluster.my-address or configure the "
                  "address for this server in the agency.";
    FATAL_ERROR_EXIT();
  }

  // now we can validate --cluster.my-address
  std::string const unified = Endpoint::unifiedForm(_myAddress);

  if (unified.empty()) {
    LOG(FATAL) << "invalid endpoint '" << _myAddress
               << "' specified for --cluster.my-address";
    FATAL_ERROR_EXIT();
  }
}

// YYY #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// YYY #warning FRANK split into methods
// YYY #endif

void ClusterFeature::start() {
  // return if cluster is disabled
  if (!_enableCluster) {
    return;
  }

  ServerState::instance()->setState(ServerState::STATE_STARTUP);

  // the agency about our state
  AgencyComm comm;
  comm.sendServerState(0.0);

  std::string const version = comm.getVersion();

  ServerState::instance()->setInitialized();

  std::string const endpoints = AgencyComm::getEndpointsString();

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  LOG(INFO) << "Cluster feature is turned on. Agency version: " << version
            << ", Agency endpoints: " << endpoints << ", server id: '" << _myId
            << "', internal address: " << _myAddress
            << ", role: " << ServerState::roleToString(role);

  if (!_disableHeartbeat) {
    AgencyCommResult result = comm.getValues("Sync/HeartbeatIntervalMs");

    if (result.successful()) {
      velocypack::Slice HeartbeatIntervalMs =
          result.slice()[0].get(std::vector<std::string>(
              {AgencyComm::prefix(), "Sync", "HeartbeatIntervalMs"}));

      if (HeartbeatIntervalMs.isInteger()) {
        try {
          _heartbeatInterval = HeartbeatIntervalMs.getUInt();
          LOG(INFO) << "using heartbeat interval value '" << _heartbeatInterval
                    << " ms' from agency";
        } catch (...) {
          // Ignore if it is not a small int or uint
        }
      }
    }

    // no value set in agency. use default
    if (_heartbeatInterval == 0) {
      _heartbeatInterval = 5000;  // 1/s

      LOG(WARN) << "unable to read heartbeat interval from agency. Using "
                << "default value '" << _heartbeatInterval << " ms'";
    }

    // start heartbeat thread
    _heartbeatThread = new HeartbeatThread(_agencyCallbackRegistry.get(),
                                           _heartbeatInterval * 1000, 5);

    if (!_heartbeatThread->init() || !_heartbeatThread->start()) {
      LOG(FATAL) << "heartbeat could not connect to agency endpoints ("
                 << endpoints << ")";
      FATAL_ERROR_EXIT();
    }

    while (!_heartbeatThread->isReady()) {
      // wait until heartbeat is ready
      usleep(10000);
    }
  }

  AgencyCommResult result;

  while (true) {
    VPackBuilder builder;
    try {
      VPackObjectBuilder b(&builder);
      builder.add("endpoint", VPackValue(_myAddress));
    } catch (...) {
      LOG(FATAL) << "out of memory";
      FATAL_ERROR_EXIT();
    }

    result = comm.setValue("Current/ServersRegistered/" + _myId,
                           builder.slice(), 0.0);

    if (!result.successful()) {
      LOG(FATAL) << "unable to register server in agency: http code: "
                 << result.httpCode() << ", body: " << result.body();
      FATAL_ERROR_EXIT();
    } else {
      break;
    }

    sleep(1);
  }

  if (role == ServerState::ROLE_COORDINATOR) {
    ServerState::instance()->setState(ServerState::STATE_SERVING);
  } else if (role == ServerState::ROLE_PRIMARY) {
    ServerState::instance()->setState(ServerState::STATE_SERVINGASYNC);
  } else if (role == ServerState::ROLE_SECONDARY) {
    ServerState::instance()->setState(ServerState::STATE_SYNCING);
  }

  DispatcherFeature* dispatcher =
      ApplicationServer::getFeature<DispatcherFeature>("Dispatcher");

  dispatcher->buildAqlQueue();
}

void ClusterFeature::unprepare() {
  if (_enableCluster) {
    if (_heartbeatThread != nullptr) {
      _heartbeatThread->beginShutdown();
    }

    // change into shutdown state
    ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

    AgencyComm comm;
    comm.sendServerState(0.0);

    if (_heartbeatThread != nullptr) {
      int counter = 0;
      while (_heartbeatThread->isRunning()) {
        usleep(100000);
        // emit warning after 5 seconds
        if (++counter == 10 * 5) {
          LOG(WARN) << "waiting for heartbeat thread to finish";
        }
      }
    }
    if (_unregisterOnShutdown) {
      ServerState::instance()->unregister();
    }
  }

  ClusterComm::cleanup();

  if (!_enableCluster) {
    return;
  }

  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
  comm.sendServerState(0.0);

  // Try only once to unregister because maybe the agencycomm
  // is shutting down as well...

  ServerState::RoleEnum role = ServerState::instance()->getRole();

  AgencyWriteTransaction unreg;

  // Remove from role
  if (role == ServerState::ROLE_PRIMARY) {
    unreg.operations.push_back(AgencyOperation(
        "Current/DBServers/" + _myId, AgencySimpleOperationType::DELETE_OP));
  } else if (role == ServerState::ROLE_COORDINATOR) {
    unreg.operations.push_back(AgencyOperation(
        "Current/Coordinators/" + _myId, AgencySimpleOperationType::DELETE_OP));
  }

  // Unregister
  unreg.operations.push_back(
      AgencyOperation("Current/ServersRegistered/" + _myId,
                      AgencySimpleOperationType::DELETE_OP));

  comm.sendTransactionWithFailover(unreg, 120.0);

  while (_heartbeatThread->isRunning()) {
    usleep(50000);
  }

  // ClusterComm::cleanup();
  AgencyComm::cleanup();
}

void ClusterFeature::setUnregisterOnShutdown(bool unregisterOnShutdown) {
  _unregisterOnShutdown = unregisterOnShutdown;
}
