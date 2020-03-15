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

#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Endpoint/Endpoint.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
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
    : ApplicationFeature(server, "Cluster") {
  setOptional(true);
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<DatabaseFeaturePhase>();
}

ClusterFeature::~ClusterFeature() {
  if (_enableCluster) {
    AgencyCommManager::shutdown();
  }
}

void ClusterFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("cluster", "Configure the cluster");

  options->addObsoleteOption("--cluster.username",
                             "username used for cluster-internal communication", true);
  options->addObsoleteOption("--cluster.password",
                             "password used for cluster-internal communication", true);
  options->addObsoleteOption("--cluster.disable-dispatcher-kickstarter",
                             "The dispatcher feature isn't available anymore; "
                             "Use ArangoDBStarter for this now!",
                             true);
  options->addObsoleteOption("--cluster.disable-dispatcher-frontend",
                             "The dispatcher feature isn't available anymore; "
                             "Use ArangoDBStarter for this now!",
                             true);
  options->addObsoleteOption(
      "--cluster.dbserver-config",
      "The dbserver-config is not available anymore, Use ArangoDBStarter", true);
  options->addObsoleteOption(
      "--cluster.coordinator-config",
      "The coordinator-config is not available anymore, Use ArangoDBStarter", true);
  options->addObsoleteOption("--cluster.data-path",
                             "path to cluster database directory", true);
  options->addObsoleteOption("--cluster.log-path",
                             "path to log directory for the cluster", true);
  options->addObsoleteOption("--cluster.arangod-path",
                             "path to the arangod for the cluster", true);
  options->addObsoleteOption("--cluster.my-local-info",
                             "this server's local info", false);
  options->addObsoleteOption("--cluster.my-id", "this server's id", false);

  options->addOption(
      "--cluster.require-persisted-id",
      "if set to true, then the instance will only start if a UUID file is "
      "found in the database on startup. Setting this option will make sure "
      "the instance is started using an already existing database directory "
      "and not a new one. For the first start, the UUID file must either be "
      "created manually or the option must be set to false for the initial "
      "startup",
      new BooleanParameter(&_requirePersistedId));

  options->addOption("--cluster.agency-endpoint",
                     "agency endpoint to connect to",
                     new VectorParameter<StringParameter>(&_agencyEndpoints),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer));

  options->addOption("--cluster.agency-prefix", "agency prefix",
                     new StringParameter(&_agencyPrefix),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer));

  options->addOption("--cluster.my-role", "this server's role",
                     new StringParameter(&_myRole));

  options->addOption("--cluster.my-address",
                     "this server's endpoint (cluster internal)",
                     new StringParameter(&_myEndpoint),

                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer));

  options->addOption("--cluster.my-advertised-endpoint",
                     "this server's advertised endpoint (e.g. external IP "
                     "address or load balancer, optional)",
                     new StringParameter(&_myAdvertisedEndpoint),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer));

  options
      ->addOption("--cluster.write-concern",
                  "write concern used for writes to new collections",
                  new UInt32Parameter(&_writeConcern),
                  arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                               arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30600);

  options->addOption("--cluster.system-replication-factor",
                     "default replication factor for system collections",
                     new UInt32Parameter(&_systemReplicationFactor),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator));

  options
      ->addOption("--cluster.default-replication-factor",
                  "default replication factor for non-system collections",
                  new UInt32Parameter(&_defaultReplicationFactor),
                  arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                               arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30600);

  options
      ->addOption("--cluster.min-replication-factor",
                  "minimum replication factor for new collections",
                  new UInt32Parameter(&_minReplicationFactor),
                  arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                               arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30600);

  options
      ->addOption(
          "--cluster.max-replication-factor",
          "maximum replication factor for new collections (0 = unrestricted)",
          new UInt32Parameter(&_maxReplicationFactor),
          arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                       arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30600);

  options
      ->addOption("--cluster.max-number-of-shards",
                  "maximum number of shards when creating new collections (0 = "
                  "unrestricted)",
                  new UInt32Parameter(&_maxNumberOfShards),
                  arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                               arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30501);

  options
      ->addOption("--cluster.force-one-shard",
                  "force one-shard mode for all new collections",
                  new BooleanParameter(&_forceOneShard),
                  arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                               arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30600);

  options->addOption(
      "--cluster.create-waits-for-sync-replication",
      "active coordinator will wait for all replicas to create collection",
      new BooleanParameter(&_createWaitsForSyncReplication),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                   arangodb::options::Flags::OnCoordinator,
                                   arangodb::options::Flags::OnDBServer,
                                   arangodb::options::Flags::Hidden));

  options->addOption(
      "--cluster.index-create-timeout",
      "amount of time (in seconds) the coordinator will wait for an index to "
      "be created before giving up",
      new DoubleParameter(&_indexCreationTimeout),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                   arangodb::options::Flags::OnCoordinator,
                                   arangodb::options::Flags::Hidden));
}

void ClusterFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (options->processingResult().touched(
          "cluster.disable-dispatcher-kickstarter") ||
      options->processingResult().touched(
          "cluster.disable-dispatcher-frontend")) {
    LOG_TOPIC("33707", FATAL, arangodb::Logger::CLUSTER)
        << "The dispatcher feature isn't available anymore. Use "
        << "ArangoDBStarter for this now! See "
        << "https://github.com/arangodb-helper/arangodb/ for more "
        << "details.";
    FATAL_ERROR_EXIT();
  }

  if (_forceOneShard) {
    _maxNumberOfShards = 1;
  } else if (_maxNumberOfShards == 0) {
    LOG_TOPIC("e83c2", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--max-number-of-shards`. The value must be at "
           "least 1";
    FATAL_ERROR_EXIT();
  }

  if (_minReplicationFactor == 0) {
    // min replication factor must not be 0
    LOG_TOPIC("2fbdd", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.min-replication-factor`. The value "
           "must be at least 1";
    FATAL_ERROR_EXIT();
  }

  if (_maxReplicationFactor > 10) {
    // 10 is a hard-coded limit for the replication factor
    LOG_TOPIC("886c6", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.max-replication-factor`. The value "
           "must not exceed 10";
    FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(_minReplicationFactor > 0);
  if (!options->processingResult().touched(
          "cluster.default-replication-factor")) {
    // no default replication factor set. now use the minimum value, which is
    // guaranteed to be at least 1
    _defaultReplicationFactor = _minReplicationFactor;
  }

  if (!options->processingResult().touched(
          "cluster.system-replication-factor")) {
    // no system replication factor set. now make sure it is between min and max
    if (_systemReplicationFactor > _maxReplicationFactor) {
      _systemReplicationFactor = _maxReplicationFactor;
    } else if (_systemReplicationFactor < _minReplicationFactor) {
      _systemReplicationFactor = _minReplicationFactor;
    }
  }

  if (_defaultReplicationFactor == 0) {
    // default replication factor must not be 0
    LOG_TOPIC("fc8a9", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.default-replication-factor`. The "
           "value must be at least 1";
    FATAL_ERROR_EXIT();
  }

  if (_systemReplicationFactor == 0) {
    // default replication factor must not be 0
    LOG_TOPIC("46935", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.system-replication-factor`. The value "
           "must be at least 1";
    FATAL_ERROR_EXIT();
  }

  if (_defaultReplicationFactor > 0 && _maxReplicationFactor > 0 &&
      _defaultReplicationFactor > _maxReplicationFactor) {
    LOG_TOPIC("5af7e", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.default-replication-factor`. Must not "
           "be higher than `--cluster.max-replication-factor`";
    FATAL_ERROR_EXIT();
  }

  if (_defaultReplicationFactor > 0 && _defaultReplicationFactor < _minReplicationFactor) {
    LOG_TOPIC("b9aea", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.default-replication-factor`. Must not "
           "be lower than `--cluster.min-replication-factor`";
    FATAL_ERROR_EXIT();
  }

  if (_systemReplicationFactor > 0 && _maxReplicationFactor > 0 &&
      _systemReplicationFactor > _maxReplicationFactor) {
    LOG_TOPIC("6cf0c", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.system-replication-factor`. Must not "
           "be higher than `--cluster.max-replication-factor`";
    FATAL_ERROR_EXIT();
  }

  if (_systemReplicationFactor > 0 && _systemReplicationFactor < _minReplicationFactor) {
    LOG_TOPIC("dfc38", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.system-replication-factor`. Must not "
           "be lower than `--cluster.min-replication-factor`";
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
    LOG_TOPIC("d283a", FATAL, Logger::CLUSTER)
        << "must at least specify one endpoint in --cluster.agency-endpoint";
    FATAL_ERROR_EXIT();
  }

  // validate --cluster.my-address
  if (_myEndpoint.empty()) {
    LOG_TOPIC("c1532", FATAL, arangodb::Logger::CLUSTER)
        << "unable to determine internal address for server '"
        << ServerState::instance()->getId()
        << "'. Please specify --cluster.my-address or configure the "
           "address for this server in the agency.";
    FATAL_ERROR_EXIT();
  }

  // now we can validate --cluster.my-address
  if (Endpoint::unifiedForm(_myEndpoint).empty()) {
    LOG_TOPIC("41256", FATAL, arangodb::Logger::CLUSTER)
        << "invalid endpoint '" << _myEndpoint << "' specified for --cluster.my-address";
    FATAL_ERROR_EXIT();
  }
  if (!_myAdvertisedEndpoint.empty() &&
      Endpoint::unifiedForm(_myAdvertisedEndpoint).empty()) {
    LOG_TOPIC("ece6a", FATAL, arangodb::Logger::CLUSTER)
        << "invalid endpoint '" << _myAdvertisedEndpoint
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
    LOG_TOPIC("7259b", FATAL, arangodb::Logger::CLUSTER)
        << "invalid value specified for --cluster.agency-prefix";
    FATAL_ERROR_EXIT();
  }

  // validate system-replication-factor
  if (_systemReplicationFactor == 0) {
    LOG_TOPIC("cb945", FATAL, arangodb::Logger::CLUSTER)
        << "system replication factor must be greater 0";
    FATAL_ERROR_EXIT();
  }

  std::string fallback = _myEndpoint;
  // Now extract the hostname/IP:
  auto pos = fallback.find("://");
  if (pos != std::string::npos) {
    fallback = fallback.substr(pos + 3);
  }
  pos = fallback.rfind(':');
  if (pos != std::string::npos) {
    fallback = fallback.substr(0, pos);
  }
  auto ss = ServerState::instance();
  ss->findHost(fallback);

  if (!_myRole.empty()) {
    _requestedRole = ServerState::stringToRole(_myRole);

    std::vector<arangodb::ServerState::RoleEnum> const disallowedRoles = {
        /*ServerState::ROLE_SINGLE,*/ ServerState::ROLE_AGENT, ServerState::ROLE_UNDEFINED};

    if (std::find(disallowedRoles.begin(), disallowedRoles.end(), _requestedRole) !=
        disallowedRoles.end()) {
      LOG_TOPIC("198c3", FATAL, arangodb::Logger::CLUSTER)
          << "Invalid role provided for `--cluster.my-role`. Possible values: "
             "DBSERVER, PRIMARY, COORDINATOR";
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
  LOG_TOPIC("3bb7d", INFO, arangodb::Logger::CLUSTER)
      << "Starting up with role " << roleString;
}

void ClusterFeature::prepare() {
  if (_enableCluster && _requirePersistedId && !ServerState::instance()->hasPersistedId()) {
    LOG_TOPIC("d2194", FATAL, arangodb::Logger::CLUSTER)
        << "required persisted UUID file '"
        << ServerState::instance()->getUuidFilename()
        << "' not found. Please make sure this instance is started using an "
           "already existing database directory";
    FATAL_ERROR_EXIT();
  }

  server().getFeature<arangodb::MetricsFeature>().histogram(
      StaticStrings::AgencyCommRequestTimeMs, log_scale_t<uint64_t>(2, 58, 120000, 10),
      "Request time for Agency requests");

  // create callback registery
  _agencyCallbackRegistry.reset(new AgencyCallbackRegistry(server(), agencyCallbacksPath()));

  // Initialize ClusterInfo library:
  _clusterInfo = std::make_unique<ClusterInfo>(server(), _agencyCallbackRegistry.get());

  if (ServerState::instance()->isAgent() || _enableCluster) {
    AuthenticationFeature* af = AuthenticationFeature::instance();
    // nullptr happens only during shutdown
    if (af->isActive() && !af->hasUserdefinedJwt()) {
      LOG_TOPIC("6e615", FATAL, arangodb::Logger::CLUSTER)
          << "Cluster authentication enabled but JWT not set via command line. "
             "Please provide --server.jwt-secret-keyfile or "
             "--server.jwt-secret-folder which is used throughout the cluster.";
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

  network::ConnectionPool::Config config;
  config.numIOThreads = 2u;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 1000;
  config.verifyHosts = false;
  config.clusterInfo = &clusterInfo();
  config.name = "AgencyComm";

  _pool = std::make_unique<network::ConnectionPool>(config);

  // register the prefix with the communicator
  AgencyCommManager::initialize(server(), _agencyPrefix);
  TRI_ASSERT(AgencyCommManager::MANAGER != nullptr);
  AsyncAgencyCommManager::initialize(server());
  AsyncAgencyCommManager::INSTANCE->pool(_pool.get());

  for (size_t i = 0; i < _agencyEndpoints.size(); ++i) {
    std::string const unified = Endpoint::unifiedForm(_agencyEndpoints[i]);

    if (unified.empty()) {
      LOG_TOPIC("1b759", FATAL, arangodb::Logger::CLUSTER)
          << "invalid endpoint '" << _agencyEndpoints[i]
          << "' specified for --cluster.agency-endpoint";
      FATAL_ERROR_EXIT();
    }

    AgencyCommManager::MANAGER->addEndpoint(unified);
    AsyncAgencyCommManager::INSTANCE->addEndpoint(unified);
  }

  // perform an initial connect to the agency
  if (!AgencyCommManager::MANAGER->start()) {
    LOG_TOPIC("54560", FATAL, arangodb::Logger::CLUSTER)
        << "Could not connect to any agency endpoints ("
        << AgencyCommManager::MANAGER->endpointsString() << ")";
    FATAL_ERROR_EXIT();
  }

  if (!ServerState::instance()->integrateIntoCluster(_requestedRole, _myEndpoint,
                                                     _myAdvertisedEndpoint)) {
    LOG_TOPIC("fea1e", FATAL, Logger::STARTUP)
        << "Couldn't integrate into cluster.";
    FATAL_ERROR_EXIT();
  }

  auto role = ServerState::instance()->getRole();
  auto endpoints = AgencyCommManager::MANAGER->endpointsString();

  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG_TOPIC("613f4", FATAL, arangodb::Logger::CLUSTER)
        << "unable to determine unambiguous role for server '"
        << ServerState::instance()->getId()
        << "'. No role configured in agency (" << endpoints << ")";
    FATAL_ERROR_EXIT();
  }

  // if nonempty, it has already been set above

  // If we are a coordinator, we wait until at least one DBServer is there,
  // otherwise we can do very little, in particular, we cannot create
  // any collection:
  if (role == ServerState::ROLE_COORDINATOR) {
    double start = TRI_microtime();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // in maintainer mode, a developer does not want to spend that much time
    // waiting for extra nodes to start up
    constexpr double waitTime = 5.0;
#else
    constexpr double waitTime = 15.0;
#endif

    while (true) {
      LOG_TOPIC("d4db4", INFO, arangodb::Logger::CLUSTER)
          << "Waiting for DBservers to show up...";
      _clusterInfo->loadCurrentDBServers();
      std::vector<ServerID> DBServers = _clusterInfo->getCurrentDBServers();
      if (DBServers.size() >= 1 &&
          (DBServers.size() > 1 || TRI_microtime() - start > waitTime)) {
        LOG_TOPIC("22f55", INFO, arangodb::Logger::CLUSTER)
            << "Found " << DBServers.size() << " DBservers.";
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void ClusterFeature::start() {
  // return if cluster is disabled
  if (!_enableCluster) {
    startHeartbeatThread(nullptr, 5000, 5, std::string());
    return;
  }

  ServerState::instance()->setState(ServerState::STATE_STARTUP);

  // the agency about our state
  AgencyComm comm(server());
  comm.sendServerState();

  std::string const version = comm.version();

  ServerState::instance()->setInitialized();

  std::string const endpoints = AgencyCommManager::MANAGER->endpointsString();

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  std::string myId = ServerState::instance()->getId();

  if (role == ServerState::RoleEnum::ROLE_DBSERVER) {
    _dropped_follower_counter = server().getFeature<arangodb::MetricsFeature>().counter(
        StaticStrings::DroppedFollowerCount, 0,
        "Number of drop-follower events");
  }

  LOG_TOPIC("b6826", INFO, arangodb::Logger::CLUSTER)
      << "Cluster feature is turned on"
      << (_forceOneShard ? " with one-shard mode" : "")
      << ". Agency version: " << version << ", Agency endpoints: " << endpoints
      << ", server id: '" << myId << "', internal endpoint / address: " << _myEndpoint
      << "', advertised endpoint: " << _myAdvertisedEndpoint << ", role: " << role;

  AgencyCommResult result = comm.getValues("Sync/HeartbeatIntervalMs");

  if (result.successful()) {
    velocypack::Slice HeartbeatIntervalMs = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Sync", "HeartbeatIntervalMs"}));

    if (HeartbeatIntervalMs.isInteger()) {
      try {
        _heartbeatInterval = HeartbeatIntervalMs.getUInt();
        LOG_TOPIC("805b2", INFO, arangodb::Logger::CLUSTER)
            << "using heartbeat interval value '" << _heartbeatInterval
            << " ms' from agency";
      } catch (...) {
        // Ignore if it is not a small int or uint
      }
    }
  }

  // no value set in agency. use default
  if (_heartbeatInterval == 0) {
    _heartbeatInterval = 5000;  // 1/s
    LOG_TOPIC("3d871", WARN, arangodb::Logger::CLUSTER)
        << "unable to read heartbeat interval from agency. Using "
        << "default value '" << _heartbeatInterval << " ms'";
  }

  startHeartbeatThread(_agencyCallbackRegistry.get(), _heartbeatInterval, 5, endpoints);

  comm.increment("Current/Version");

  ServerState::instance()->setState(ServerState::STATE_SERVING);
}

void ClusterFeature::beginShutdown() {}

void ClusterFeature::stop() { shutdownHeartbeatThread(); }

void ClusterFeature::unprepare() {
  if (!_enableCluster) {
    return;
  }

  shutdownHeartbeatThread();

  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm(server());
  comm.sendServerState();

  if (_heartbeatThread != nullptr) {
    int counter = 0;
    while (_heartbeatThread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // emit warning after 5 seconds
      if (++counter == 10 * 5) {
        LOG_TOPIC("26835", WARN, arangodb::Logger::CLUSTER)
            << "waiting for heartbeat thread to finish";
      }
    }
  }

  if (_unregisterOnShutdown) {
    ServerState::instance()->unregister();
  }

  comm.sendServerState();

  // Try only once to unregister because maybe the agencycomm
  // is shutting down as well...

  // Remove from role list
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  // nice variable name :S
  std::string alk = ServerState::roleToAgencyListKey(role);
  std::string me = ServerState::instance()->getId();

  AgencyWriteTransaction unreg;
  unreg.operations.push_back(AgencyOperation("Current/" + alk + "/" + me,
                                             AgencySimpleOperationType::DELETE_OP));
  // Unregister
  unreg.operations.push_back(AgencyOperation("Current/ServersRegistered/" + me,
                                             AgencySimpleOperationType::DELETE_OP));
  unreg.operations.push_back(
      AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));

  constexpr int maxTries = 10;
  int tries = 0;
  while (true) {
    AgencyCommResult res = comm.sendTransactionWithFailover(unreg, 120.0);
    if (res.successful()) {
      break;
    }

    if (res.httpCode() == TRI_ERROR_HTTP_SERVICE_UNAVAILABLE || !res.connected()) {
      LOG_TOPIC("1776b", INFO, Logger::CLUSTER)
          << "unable to unregister server from agency, because agency is in "
             "shutdown";
      break;
    }

    if (++tries < maxTries) {
      // try again
      LOG_TOPIC("c7af5", ERR, Logger::CLUSTER)
          << "unable to unregister server from agency "
          << "(attempt " << tries << " of " << maxTries << "): " << res.errorMessage();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
      // give up
      LOG_TOPIC("c8fc4", ERR, Logger::CLUSTER)
          << "giving up unregistering server from agency: " << res.errorMessage();
      break;
    }
  }

  TRI_ASSERT(tries <= maxTries);

  while (_heartbeatThread != nullptr && _heartbeatThread->isRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  _pool.reset();
  AgencyCommManager::MANAGER->stop();

  _clusterInfo->cleanup();
}

void ClusterFeature::setUnregisterOnShutdown(bool unregisterOnShutdown) {
  _unregisterOnShutdown = unregisterOnShutdown;
}

/// @brief common routine to start heartbeat with or without cluster active
void ClusterFeature::startHeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                                          uint64_t interval_ms, uint64_t maxFailsBeforeWarning,
                                          std::string const& endpoints) {
  _heartbeatThread =
      std::make_shared<HeartbeatThread>(server(), agencyCallbackRegistry,
                                        std::chrono::microseconds(interval_ms * 1000),
                                        maxFailsBeforeWarning);

  if (!_heartbeatThread->init() || !_heartbeatThread->start()) {
    // failure only occures in cluster mode.
    LOG_TOPIC("7e050", FATAL, arangodb::Logger::CLUSTER)
        << "heartbeat could not connect to agency endpoints (" << endpoints << ")";
    FATAL_ERROR_EXIT();
  }

  while (!_heartbeatThread->isReady()) {
    // wait until heartbeat is ready
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void ClusterFeature::shutdownHeartbeatThread() {
  if (_heartbeatThread == nullptr) {
    return;
  }

  _heartbeatThread->beginShutdown();

  int counter = 0;
  while (_heartbeatThread->isRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // emit warning after 5 seconds
    if (++counter == 10 * 5) {
      LOG_TOPIC("acaa9", WARN, arangodb::Logger::CLUSTER)
          << "waiting for heartbeat thread to finish";
    }
  }
}

void ClusterFeature::syncDBServerStatusQuo() {
  if (_heartbeatThread != nullptr) {
    _heartbeatThread->syncDBServerStatusQuo(true);
  }
}

std::shared_ptr<HeartbeatThread> ClusterFeature::heartbeatThread() {
  return _heartbeatThread;
}

ClusterInfo& ClusterFeature::clusterInfo() {
  if (!_clusterInfo) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  return *_clusterInfo;
}
