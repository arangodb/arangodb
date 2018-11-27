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
#include "Endpoint/Endpoint.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "SimpleHttpClient/ConnectionManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

ClusterFeature::ClusterFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Cluster"),
      _unregisterOnShutdown(false),
      _enableCluster(false),
      _heartbeatThread(nullptr),
      _heartbeatInterval(0),
      _disableHeartbeat(false),
      _agencyCallbackRegistry(nullptr),
      _requestedRole(ServerState::RoleEnum::ROLE_UNDEFINED) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Authentication");
  startsAfter("CacheManager");
  startsAfter("Logger");
  startsAfter("Database");
  startsAfter("Scheduler");
  startsAfter("V8Dealer");
}

ClusterFeature::~ClusterFeature() {
  if (_enableCluster) {
    AgencyCommManager::shutdown();
  }

  // delete connection manager instance
  auto cm = httpclient::ConnectionManager::instance();
  delete cm;
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

  options->addOption("--cluster.agency-endpoint",
                     "agency endpoint to connect to",
                     new VectorParameter<StringParameter>(&_agencyEndpoints));

  options->addHiddenOption("--cluster.agency-prefix", "agency prefix",
                     new StringParameter(&_agencyPrefix));
  // FIXME: make obsolete in > 3.3
  //options->addObsoleteOption("--cluster.my-local-info", "this server's local info", false);
  options->addHiddenOption("--cluster.my-local-info", "this server's local info",
                           new StringParameter(&_myLocalInfo));

  options->addObsoleteOption("--cluster.my-id", "this server's id", false);

  options->addOption("--cluster.my-role", "this server's role",
                     new StringParameter(&_myRole));

  options->addOption("--cluster.my-address", "this server's endpoint",
                     new StringParameter(&_myAddress));

  options->addOption("--cluster.system-replication-factor",
                     "replication factor for system collections",
                     new UInt32Parameter(&_systemReplicationFactor));

  options->addHiddenOption("--cluster.create-waits-for-sync-replication",
                     "active coordinator will wait for all replicas to create collection",
                     new BooleanParameter(&_createWaitsForSyncReplication));

  options->addHiddenOption("--cluster.index-create-timeout",
                     "amount of time (in seconds) the coordinator will wait for an index to be created before giving up",
                     new DoubleParameter(&_indexCreationTimeout));
}

void ClusterFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (options->processingResult().touched("cluster.disable-dispatcher-kickstarter") ||
      options->processingResult().touched("cluster.disable-dispatcher-frontend")) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
      << "The dispatcher feature isn't available anymore. Use ArangoDBStarter for this now! See https://github.com/arangodb-helper/ArangoDBStarter/ for more details.";
    FATAL_ERROR_EXIT();
  }

  // check if the cluster is enabled
  _enableCluster = !_agencyEndpoints.empty();

  if (!_enableCluster) {
    _requestedRole = ServerState::ROLE_SINGLE;
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
    auto ss = ServerState::instance();
    ss->findHost("localhost");
    return;
  }

  // validate --cluster.agency-endpoint (currently a noop)
  if (_agencyEndpoints.empty()) {
    LOG_TOPIC(FATAL, Logger::CLUSTER)
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
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "invalid value specified for --cluster.agency-prefix";
    FATAL_ERROR_EXIT();
  }

  // validate system-replication-factor
  if (_systemReplicationFactor == 0) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "system replication factor must be greater 0";
    FATAL_ERROR_EXIT();
  }

  std::string fallback = _myAddress;
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
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "Invalid role provided. Possible values: PRIMARY, "
                    "SECONDARY, COORDINATOR";
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
  auto v8Dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

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

  auto agency =
    application_features::ApplicationServer::getFeature<AgencyFeature>("Agency");

#ifdef DEBUG_SYNC_REPLICATION
  bool startClusterComm = true;
#else
  bool startClusterComm = false;
#endif

  if (agency->isEnabled() || _enableCluster) {
    startClusterComm = true;
    AuthenticationFeature* af = AuthenticationFeature::instance();
    // nullptr happens only during shutdown
    if (af->isActive() && !af->hasUserdefinedJwt()) {
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "Cluster authentication enabled but JWT not set via command line. Please"
        << " provide --server.jwt-secret which is used throughout the cluster.";
      FATAL_ERROR_EXIT();
    }
  }

  if (startClusterComm) {
    // initialize ClusterComm library, must call initialize only once
    ClusterComm::initialize();
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

  // Now either _myId is set properly or _myId is empty and _myLocalInfo and
  // _myAddress are set.
  if (!_myAddress.empty()) {
    ServerState::instance()->setAddress(_myAddress);
  }

  // disable error logging for a while
  ClusterComm::instance()->enableConnectionErrorLogging(false);
  // perform an initial connect to the agency
  if (!AgencyCommManager::MANAGER->start()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "Could not connect to any agency endpoints ("
               << AgencyCommManager::MANAGER->endpointsString() << ")";
    FATAL_ERROR_EXIT();
  }

  // FIXME: remove mylocalInfo > 3.3
  if (!ServerState::instance()->integrateIntoCluster(_requestedRole, _myAddress, _myLocalInfo)) {
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

    auto ci = ClusterInfo::instance();
    double start = TRI_microtime();

    while (true) {
      LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "Waiting for DBservers to show up...";
      ci->loadCurrentDBServers();
      std::vector<ServerID> DBServers = ci->getCurrentDBServers();
      if (DBServers.size() >= 1 &&
          (DBServers.size() > 1 || TRI_microtime() - start > 15.0)) {
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) << "Found " << DBServers.size() << " DBservers.";
        break;
      }
      sleep(1);
    }

  }

  if (_myAddress.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "unable to determine internal address for server '"
               << ServerState::instance()->getId()
               << "'. Please specify --cluster.my-address or configure the "
                  "address for this server in the agency.";
    FATAL_ERROR_EXIT();
  }

  // now we can validate --cluster.my-address
  if (Endpoint::unifiedForm(_myAddress).empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "invalid endpoint '" << _myAddress
               << "' specified for --cluster.my-address";
    FATAL_ERROR_EXIT();
  }

}

void ClusterFeature::start() {
  // return if cluster is disabled
  if (!_enableCluster) {
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
            << "', internal address: " << _myAddress
            << ", role: " << role;

  if (!_disableHeartbeat) {
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

    // start heartbeat thread
    _heartbeatThread = std::make_shared<HeartbeatThread>(
        _agencyCallbackRegistry.get(), std::chrono::microseconds(_heartbeatInterval * 1000), 5);

    if (!_heartbeatThread->init() || !_heartbeatThread->start()) {
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "heartbeat could not connect to agency endpoints ("
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
      builder.add("host", VPackValue(ServerState::instance()->getHost()));
      builder.add("version", VPackValue(rest::Version::getNumericServerVersion()));
      builder.add("versionString",  VPackValue(rest::Version::getServerVersion()));
      builder.add("engine", VPackValue(EngineSelectorFeature::engineName()));
    } catch (...) {
      LOG_TOPIC(FATAL, arangodb::Logger::CLUSTER) << "out of memory";
      FATAL_ERROR_EXIT();
    }

    result = comm.setValue("Current/ServersRegistered/" + myId,
                           builder.slice(), 0.0);

    if (result.successful()) {
      break;
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
        << "failed to register server in agency: http code: "
        << result.httpCode() << ", body: '" << result.body() << "', retrying ...";
    }

    sleep(1);
  }

  ServerState::instance()->setState(ServerState::STATE_SERVING);
}

void ClusterFeature::beginShutdown() {
  ClusterComm::instance()->disable();
}

void ClusterFeature::stop() {

  if (_enableCluster) {
    if (_heartbeatThread != nullptr) {
      _heartbeatThread->beginShutdown();
    }

    if (_heartbeatThread != nullptr) {
      int counter = 0;
      while (_heartbeatThread->isRunning()) {
        usleep(100000);
        // emit warning after 5 seconds
        if (++counter == 10 * 5) {
          LOG_TOPIC(WARN, arangodb::Logger::CLUSTER) << "waiting for heartbeat thread to finish";
        }
      }
    }
  }

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
          LOG_TOPIC(WARN, arangodb::Logger::CLUSTER) << "waiting for heartbeat thread to finish";
        }
      }
    }

    if (_unregisterOnShutdown) {
      ServerState::instance()->unregister();
    }
  }

  if (!_enableCluster) {
    ClusterComm::cleanup();
    return;
  }

  // change into shutdown state
  ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

  AgencyComm comm;
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
  comm.sendTransactionWithFailover(unreg, 120.0);

  while (_heartbeatThread->isRunning()) {
    usleep(50000);
  }

  AgencyCommManager::MANAGER->stop();
  ClusterComm::cleanup();

  ClusterInfo::cleanup();
}

void ClusterFeature::setUnregisterOnShutdown(bool unregisterOnShutdown) {
  _unregisterOnShutdown = unregisterOnShutdown;
}
