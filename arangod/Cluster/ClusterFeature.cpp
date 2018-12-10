////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Endpoint/Endpoint.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

ClusterFeature::ClusterFeature(application_features::ApplicationServer& server)
  : ApplicationFeature(server, "Cluster"),
    _unregisterOnShutdown(false),
    _enableCluster(false),
    _requirePersistedId(false),
    _heartbeatThread(nullptr),
    _heartbeatInterval(0),
    _agencyCallbackRegistry(nullptr),
    _requestedRole(ServerState::RoleEnum::ROLE_UNDEFINED) {
  setOptional(true);
  startsAfter("DatabasePhase");
  startsAfter("CommunicationPhase");
}

ClusterFeature::~ClusterFeature() {
  if (_enableCluster) {
    AgencyCommManager::shutdown();
  }
}

void ClusterFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("cluster", "Configure the cluster");

  options->addObsoleteOption("--cluster.username",
                             "username used for cluster-internal communication",
                             true);
  options->addObsoleteOption("--cluster.password",
                             "password used for cluster-internal communication",
                             true);
  options->addObsoleteOption("--cluster.disable-dispatcher-kickstarter",
                             "The dispatcher feature isn't available anymore; Use ArangoDBStarter for this now!",
                             true);
  options->addObsoleteOption("--cluster.disable-dispatcher-frontend",
                             "The dispatcher feature isn't available anymore; Use ArangoDBStarter for this now!",
                             true);
  options->addObsoleteOption("--cluster.dbserver-config",
                             "The dbserver-config is not available anymore, Use ArangoDBStarter",
                             true);
  options->addObsoleteOption("--cluster.coordinator-config",
                             "The coordinator-config is not available anymore, Use ArangoDBStarter",
                             true);
  options->addObsoleteOption("--cluster.data-path",
                             "path to cluster database directory",
                             true);
  options->addObsoleteOption("--cluster.log-path",
                             "path to log directory for the cluster",
                             true);
  options->addObsoleteOption("--cluster.arangod-path",
                             "path to the arangod for the cluster",
                             true);

  options->addOption("--cluster.require-persisted-id",
                     "if set to true, then the instance will only start if a UUID file is found in the database on startup. Setting this option will make sure the instance is started using an already existing database directory and not a new one. For the first start, the UUID file must either be created manually or the option must be set to false for the initial startup",
                     new BooleanParameter(&_requirePersistedId));

  options->addOption("--cluster.agency-endpoint",
                     "agency endpoint to connect to",
                     new VectorParameter<StringParameter>(&_agencyEndpoints));

  options->addOption("--cluster.agency-prefix", "agency prefix",
                     new StringParameter(&_agencyPrefix),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addObsoleteOption("--cluster.my-local-info", "this server's local info", false);
  options->addObsoleteOption("--cluster.my-id", "this server's id", false);

  options->addOption("--cluster.my-role", "this server's role",
                     new StringParameter(&_myRole));

  options->addOption("--cluster.my-address", "this server's endpoint (cluster internal)",
                     new StringParameter(&_myEndpoint));

  options->addOption("--cluster.my-advertised-endpoint",
                     "this server's advertised endpoint (e.g. external IP "
                     "address or load balancer, optional)",
                     new StringParameter(&_myAdvertisedEndpoint));

  options->addOption("--cluster.system-replication-factor",
                     "replication factor for system collections",
                     new UInt32Parameter(&_systemReplicationFactor));

  options->addOption("--cluster.create-waits-for-sync-replication",
                     "active coordinator will wait for all replicas to create collection",
                     new BooleanParameter(&_createWaitsForSyncReplication),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--cluster.index-create-timeout",
                     "amount of time (in seconds) the coordinator will wait for an index to be created before giving up",
                     new DoubleParameter(&_indexCreationTimeout),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

void ClusterFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (options->processingResult().touched("cluster.disable-dispatcher-kickstarter") ||
      options->processingResult().touched("cluster.disable-dispatcher-frontend")) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER)
        << "The dispatcher feature isn't available anymore. Use "
        << "ArangoDBStarter for this now! See "
        << "https://github.com/arangodb-helper/ArangoDBStarter/ for more "
        << "details.";
    FATAL_ERROR_EXIT();
  }

  // check if the cluster is enabled
  _enableCluster = !_agencyEndpoints.empty();
  if (!_enableCluster) {
    _requestedRole = ServerState::ROLE_SINGLE;
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
    ServerState::instance()->findHost("localhost");
    return;
  }

  // validate --cluster.agency-endpoint
  if (_agencyEndpoints.empty()) {
    LOG_TOPIC(FATAL, Logger::CLUSTER)
      << "must at least specify one endpoint in --cluster.agency-endpoint";
    FATAL_ERROR_EXIT();
  }

  // validate --cluster.my-address
  if (_myEndpoint.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "unable to determine internal address for server '"
    << ServerState::instance()->getId()
    << "'. Please specify --cluster.my-address or configure the "
    "address for this server in the agency.";
    FATAL_ERROR_EXIT();
  }

  // now we can validate --cluster.my-address
  if (Endpoint::unifiedForm(_myEndpoint).empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "invalid endpoint '" << _myEndpoint
    << "' specified for --cluster.my-address";
    FATAL_ERROR_EXIT();
  }
  if (!_myAdvertisedEndpoint.empty() &&
      Endpoint::unifiedForm(_myAdvertisedEndpoint).empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "invalid endpoint '" << _myAdvertisedEndpoint
    << "' specified for --cluster.my-advertised-endpoint";
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
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "invalid value specified for --cluster.agency-prefix";
    FATAL_ERROR_EXIT();
  }

  // validate system-replication-factor
  if (_systemReplicationFactor == 0) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "system replication factor must be greater 0";
    FATAL_ERROR_EXIT();
  }

  std::string fallback = _myEndpoint;
  // Now extract the hostname/IP:
  auto pos = fallback.find("://");
  if (pos != std::string::npos) {
    fallback = fallback.substr(pos+3);
  }
  pos = fallback.rfind(':');
  if (pos != std::string::npos) {
    fallback = fallback.substr(0, pos);
  }
  auto ss = ServerState::instance();
  ss->findHost(fallback);

  if (!_myRole.empty()) {
    _requestedRole = ServerState::stringToRole(_myRole);

    std::vector<arangodb::ServerState::RoleEnum> const disallowedRoles= {
      /*ServerState::ROLE_SINGLE,*/ ServerState::ROLE_AGENT, ServerState::ROLE_UNDEFINED
    };

    if (std::find(disallowedRoles.begin(), disallowedRoles.end(), _requestedRole) != disallowedRoles.end()) {
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "Invalid role provided for `--cluster.my-role`. Possible values: DBSERVER, PRIMARY, COORDINATOR";
      FATAL_ERROR_EXIT();
    }
    ServerState::instance()->setRole(_requestedRole);
  }
}

void ClusterFeature::reportRole(arangodb::ServerState::RoleEnum role) {
  std::string roleString(ServerState::roleToString(role));
  if (role == ServerState::ROLE_UNDEFINED) {
    roleString += ". Determining real role from agency";
  }
  LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "Starting up with role " << roleString;
}

void ClusterFeature::prepare() {
  if (_enableCluster &&
      _requirePersistedId &&
      !ServerState::instance()->hasPersistedId()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "required persisted UUID file '" << ServerState::instance()->getUuidFilename()
    << "' not found. Please make sure this instance is started using an already existing database directory";
    FATAL_ERROR_EXIT();
  }

  // create callback registery
  _agencyCallbackRegistry.reset(
    new AgencyCallbackRegistry(agencyCallbacksPath()));

  // Initialize ClusterInfo library:
  ClusterInfo::createInstance(_agencyCallbackRegistry.get());

  // create an instance (this will not yet create a thread)
  ClusterComm::instance();

  if (ServerState::instance()->isAgent() || _enableCluster) {
    AuthenticationFeature* af = AuthenticationFeature::instance();
    // nullptr happens only during shutdown
    if (af->isActive() && !af->hasUserdefinedJwt()) {
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "Cluster authentication enabled but JWT not set via command line. Please"
                                                  << " provide --server.jwt-secret which is used throughout the cluster.";
      FATAL_ERROR_EXIT();
    }
  }

  // return if cluster is disabled
  if (!_enableCluster) {
    reportRole(ServerState::instance()->getRole());
    return;
  } else {
    reportRole(_requestedRole);
  }

  // register the prefix with the communicator
  AgencyCommManager::initialize(_agencyPrefix);
  TRI_ASSERT(AgencyCommManager::MANAGER != nullptr);

  for (size_t i = 0; i < _agencyEndpoints.size(); ++i) {
    std::string const unified = Endpoint::unifiedForm(_agencyEndpoints[i]);

    if (unified.empty()) {
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "invalid endpoint '" << _agencyEndpoints[i]
                                                  << "' specified for --cluster.agency-endpoint";
      FATAL_ERROR_EXIT();
    }

    AgencyCommManager::MANAGER->addEndpoint(unified);
  }

  // disable error logging for a while
  ClusterComm::instance()->enableConnectionErrorLogging(false);
  // perform an initial connect to the agency
  if (!AgencyCommManager::MANAGER->start()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "Could not connect to any agency endpoints ("
                                                << AgencyCommManager::MANAGER->endpointsString() << ")";
    FATAL_ERROR_EXIT();
  }

  if (!ServerState::instance()->integrateIntoCluster(_requestedRole, _myEndpoint,
                                                     _myAdvertisedEndpoint)) {
    LOG_TOPIC(FATAL, Logger::STARTUP) << "Couldn't integrate into cluster.";
    FATAL_ERROR_EXIT();
  }

  auto role = ServerState::instance()->getRole();
  auto endpoints = AgencyCommManager::MANAGER->endpointsString();

  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "unable to determine unambiguous role for server '"
                                                << ServerState::instance()->getId()
                                                << "'. No role configured in agency (" << endpoints << ")";
    FATAL_ERROR_EXIT();
  }

  // if nonempty, it has already been set above

  // If we are a coordinator, we wait until at least one DBServer is there,
  // otherwise we can do very little, in particular, we cannot create
  // any collection:
  if (role == ServerState::ROLE_COORDINATOR) {

    auto ci = ClusterInfo::instance();
    double start = TRI_microtime();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // in maintainer mode, a developer does not want to spend that much time
    // waiting for extra nodes to start up
    constexpr double waitTime = 5.0;
#else
    constexpr double waitTime = 15.0;
#endif

    while (true) {
      LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "Waiting for DBservers to show up...";
      ci->loadCurrentDBServers();
      std::vector<ServerID> DBServers = ci->getCurrentDBServers();
      if (DBServers.size() >= 1 &&
          (DBServers.size() > 1 || TRI_microtime() - start > waitTime)) {
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "Found " << DBServers.size() << " DBservers.";
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

  }
}

void ClusterFeature::start() {

  if (ServerState::instance()->isAgent() || _enableCluster) {
    ClusterComm::initialize();
  }

  // return if cluster is disabled
  if (!_enableCluster) {
    startHeartbeatThread(nullptr, 5000, 5, std::string());
    return;
  }

  ServerState::instance()->setState(ServerState::STATE_STARTUP);

  // the agency about our state
  AgencyComm comm;
  comm.sendServerState(0.0);

  std::string const version = comm.version();

  ServerState::instance()->setInitialized();

  std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  std::string myId = ServerState::instance()->getId();

  LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "Cluster feature is turned on. Agency version: " << version
                                             << ", Agency endpoints: " << endpoints << ", server id: '" << myId
                                             << "', internal endpoint / address: " << _myEndpoint
                                             << "', advertised endpoint: " << _myAdvertisedEndpoint
                                             << ", role: " << role;

  AgencyCommResult result = comm.getValues("Sync/HeartbeatIntervalMs");

  if (result.successful()) {
    velocypack::Slice HeartbeatIntervalMs =
      result.slice()[0].get(std::vector<std::string>(
                              {AgencyCommManager::path(), "Sync", "HeartbeatIntervalMs"}));

    if (HeartbeatIntervalMs.isInteger()) {
      try {
        _heartbeatInterval = HeartbeatIntervalMs.getUInt();
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "using heartbeat interval value '" << _heartbeatInterval
                                                   << " ms' from agency";
      } catch (...) {
        // Ignore if it is not a small int or uint
      }
    }
  }

  // no value set in agency. use default
  if (_heartbeatInterval == 0) {
    _heartbeatInterval = 5000;  // 1/s
    LOG_TOPIC(WARN, arangodb::Logger::CLUSTER) << "unable to read heartbeat interval from agency. Using "
                                               << "default value '" << _heartbeatInterval << " ms'";
  }

  startHeartbeatThread(_agencyCallbackRegistry.get(), _heartbeatInterval, 5, endpoints);

  comm.increment("Current/Version");

  ServerState::instance()->setState(ServerState::STATE_SERVING);
}

void ClusterFeature::beginShutdown() {
  ClusterComm::instance()->disable();
}

void ClusterFeature::stop() {

  if (_heartbeatThread != nullptr) {
    _heartbeatThread->beginShutdown();
  }

  if (_heartbeatThread != nullptr) {
    int counter = 0;
    while (_heartbeatThread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::microseconds(100000));
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC(WARN, arangodb::Logger::CLUSTER) << "waiting for heartbeat thread to finish";
      }
    }
  }

  ClusterComm::instance()->stopBackgroundThreads();
}


void ClusterFeature::unprepare() {
  if (!_enableCluster) {
    ClusterComm::cleanup();
    return;
  }

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
      std::this_thread::sleep_for(std::chrono::microseconds(100000));
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC(WARN, arangodb::Logger::CLUSTER) << "waiting for heartbeat thread to finish";
      }
    }
  }

  if (_unregisterOnShutdown) {
    ServerState::instance()->unregister();
  }

  comm.sendServerState(0.0);

  // Try only once to unregister because maybe the agencycomm
  // is shutting down as well...

  // Remove from role list
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  std::string alk = ServerState::roleToAgencyListKey(role);
  std::string me = ServerState::instance()->getId();

  AgencyWriteTransaction unreg;
  unreg.operations.push_back(AgencyOperation("Current/" + alk + "/" + me,
                                             AgencySimpleOperationType::DELETE_OP));
  // Unregister
  unreg.operations.push_back(
    AgencyOperation("Current/ServersRegistered/" + me,
                    AgencySimpleOperationType::DELETE_OP));
  unreg.operations.push_back(
    AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));
  comm.sendTransactionWithFailover(unreg, 120.0);

  while (_heartbeatThread->isRunning()) {
    std::this_thread::sleep_for(std::chrono::microseconds(50000));
  }

  AgencyCommManager::MANAGER->stop();

  ClusterInfo::cleanup();
}

void ClusterFeature::setUnregisterOnShutdown(bool unregisterOnShutdown) {
  _unregisterOnShutdown = unregisterOnShutdown;
}

/// @brief common routine to start heartbeat with or without cluster active
void ClusterFeature::startHeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                                          uint64_t interval_ms,
                                          uint64_t maxFailsBeforeWarning,
                                          const std::string & endpoints) {

  _heartbeatThread = std::make_shared<HeartbeatThread>(
    agencyCallbackRegistry, std::chrono::microseconds(interval_ms * 1000), maxFailsBeforeWarning);

  if (!_heartbeatThread->init() || !_heartbeatThread->start()) {
    // failure only occures in cluster mode.
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "heartbeat could not connect to agency endpoints ("
                                                << endpoints << ")";
    FATAL_ERROR_EXIT();
  }

  while (!_heartbeatThread->isReady()) {
    // wait until heartbeat is ready
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
  }
}

void ClusterFeature::syncDBServerStatusQuo() {
  if (_heartbeatThread != nullptr) {
    _heartbeatThread->syncDBServerStatusQuo(true);
  }
}
