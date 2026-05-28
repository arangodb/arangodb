////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "ClusterOptionsProvider.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Basics/system-functions.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/HeartbeatThread.h"
#include "Endpoint/Endpoint.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

struct ClusterFeatureScale {
  static metrics::LogScale<uint64_t> scale() { return {2, 58, 120000, 10}; }
};

DECLARE_HISTOGRAM(arangodb_agencycomm_request_time_msec, ClusterFeatureScale,
                  "Request time for Agency requests [ms]");

ClusterFeature::ClusterFeature(ApplicationServer& server,
                               metrics::MetricsFeature& metrics)
    : application_features::ApplicationFeature{server, *this},
      _metrics{metrics},
      _agency_comm_request_time_ms(
          _metrics.add(arangodb_agencycomm_request_time_msec{})) {
  setOptional(true);
  startsAfter<application_features::CommunicationFeaturePhase>();
  startsAfter<application_features::DatabaseFeaturePhase>();
}

ClusterFeature::~ClusterFeature() { shutdown(); }

void ClusterFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  ClusterOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void ClusterFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (options->processingResult().touched(
          "cluster.disable-dispatcher-kickstarter") ||
      options->processingResult().touched(
          "cluster.disable-dispatcher-frontend")) {
    LOG_TOPIC("33707", FATAL, arangodb::Logger::CLUSTER)
        << "The dispatcher feature isn't available anymore. Use "
        << "ArangoDB Starter for this now! See "
        << "https://github.com/arangodb-helper/arangodb/ for more "
        << "details.";
    FATAL_ERROR_EXIT();
  }

  ClusterOptionsProvider provider;
  provider.validateOptions(options, _options);

  if (!_options.enableCluster) {
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
    ServerState::instance()->findHost("localhost");
    return;
  }

  // validate --cluster.agency-endpoint
  if (_options.agencyEndpoints.empty()) {
    LOG_TOPIC("d283a", FATAL, Logger::CLUSTER)
        << "must at least specify one endpoint in --cluster.agency-endpoint";
    FATAL_ERROR_EXIT();
  }

  // validate --cluster.my-address
  if (_options.myEndpoint.empty()) {
    LOG_TOPIC("c1532", FATAL, arangodb::Logger::CLUSTER)
        << "unable to determine internal address for server '"
        << ServerState::instance()->getId()
        << "'. Please specify --cluster.my-address or configure the "
           "address for this server in the agency.";
    FATAL_ERROR_EXIT();
  }

  if (Endpoint::unifiedForm(_options.myEndpoint).empty()) {
    LOG_TOPIC("41256", FATAL, arangodb::Logger::CLUSTER)
        << "invalid endpoint '" << _options.myEndpoint
        << "' specified for --cluster.my-address";
    FATAL_ERROR_EXIT();
  }
  if (!_options.myAdvertisedEndpoint.empty() &&
      Endpoint::unifiedForm(_options.myAdvertisedEndpoint).empty()) {
    LOG_TOPIC("ece6a", FATAL, arangodb::Logger::CLUSTER)
        << "invalid endpoint '" << _options.myAdvertisedEndpoint
        << "' specified for --cluster.my-advertised-endpoint";
    FATAL_ERROR_EXIT();
  }

  std::string fallback = _options.myEndpoint;
  auto pos = fallback.find("://");
  if (pos != std::string::npos) {
    fallback = fallback.substr(pos + 3);
  }
  pos = fallback.rfind(':');
  if (pos != std::string::npos) {
    fallback.resize(pos);
  }
  auto ss = ServerState::instance();
  ss->findHost(fallback);

  if (!_options.myRole.empty()) {
    _options.requestedRole = ServerState::stringToRole(_options.myRole);

    std::vector<arangodb::ServerState::RoleEnum> const disallowedRoles = {
        ServerState::ROLE_AGENT, ServerState::ROLE_UNDEFINED};

    if (std::find(disallowedRoles.begin(), disallowedRoles.end(),
                  _options.requestedRole) != disallowedRoles.end()) {
      LOG_TOPIC("198c3", FATAL, arangodb::Logger::CLUSTER)
          << "Invalid role provided for `--cluster.my-role`. Possible values: "
             "DBSERVER, PRIMARY, COORDINATOR";
      FATAL_ERROR_EXIT();
    }
    ServerState::instance()->setRole(_options.requestedRole);
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

// IMPORTANT: Please make sure that you understand that the agency is not
// available before ::start and this should not be accessed in this section.
void ClusterFeature::prepare() {
  if (_options.enableCluster && _options.requirePersistedId &&
      !ServerState::instance()->hasPersistedId()) {
    LOG_TOPIC("d2194", FATAL, arangodb::Logger::CLUSTER)
        << "required persisted UUID file '"
        << ServerState::instance()->getUuidFilename()
        << "' not found. Please make sure this instance is started using an "
           "already existing database directory";
    FATAL_ERROR_EXIT();
  }

  // in the unit tests we have situations where prepare is called on an already
  // prepared feature
  if (_agencyCache == nullptr || _clusterInfo == nullptr) {
    TRI_ASSERT(_agencyCache == nullptr);
    TRI_ASSERT(_clusterInfo == nullptr);
    allocateMembers();
  }

  if (ServerState::instance()->isAgent() || _options.enableCluster) {
    AuthenticationFeature* af = AuthenticationFeature::instance();
    if (af->isActive() && !af->hasUserdefinedJwt()) {
      LOG_TOPIC("6e615", FATAL, arangodb::Logger::CLUSTER)
          << "Cluster authentication enabled but JWT not set via command line. "
             "Please provide --server.jwt-secret-keyfile or "
             "--server.jwt-secret-folder which is used throughout the cluster.";
      FATAL_ERROR_EXIT();
    }
  }

  // return if cluster is disabled
  if (!_options.enableCluster) {
    reportRole(ServerState::instance()->getRole());
    return;
  }

  reportRole(_options.requestedRole);

  network::ConnectionPool::Config config;
  config.numIOThreads = 2u;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10000;
  config.verifyHosts = false;
  config.clusterInfo = &clusterInfo();
  config.name = "AgencyComm";

  config.metrics = network::ConnectionPool::Metrics::fromMetricsFeature(
      _metrics, config.name);

  _asyncAgencyCommPool = std::make_unique<network::ConnectionPool>(config);

  // register the prefix with the communicator
  AgencyCommHelper::initialize(_options.agencyPrefix);
  AsyncAgencyCommManager::initialize(server());
  TRI_ASSERT(AsyncAgencyCommManager::INSTANCE != nullptr);
  AsyncAgencyCommManager::INSTANCE->setSkipScheduler(true);
  AsyncAgencyCommManager::INSTANCE->pool(_asyncAgencyCommPool.get());

  for (auto const& agencyEndpoint : _options.agencyEndpoints) {
    std::string unified = Endpoint::unifiedForm(agencyEndpoint);

    if (unified.empty()) {
      LOG_TOPIC("1b759", FATAL, arangodb::Logger::CLUSTER)
          << "invalid endpoint '" << agencyEndpoint
          << "' specified for --cluster.agency-endpoint";
      FATAL_ERROR_EXIT();
    }

    // server id is not yet known at startup time; it will be filled in by
    // HeartbeatThread::updateAgentPool once the agency reports its pool.
    AsyncAgencyCommManager::INSTANCE->addAgent(/*serverId*/ {}, unified);
  }

  bool ok = AgencyComm(server()).ensureStructureInitialized();
  LOG_TOPIC("d8ce6", DEBUG, Logger::AGENCYCOMM)
      << "structures " << (ok ? "are" : "failed to") << " initialize";

  if (!ok) {
    LOG_TOPIC("54560", FATAL, arangodb::Logger::CLUSTER)
        << "Could not connect to any agents ("
        << inspection::json(AsyncAgencyCommManager::INSTANCE->agents()) << ")";
    FATAL_ERROR_EXIT();
  }

  if (!ServerState::instance()->integrateIntoCluster(
          _options.requestedRole, _options.myEndpoint,
          _options.myAdvertisedEndpoint)) {
    LOG_TOPIC("fea1e", FATAL, Logger::STARTUP)
        << "Couldn't integrate into cluster.";
    FATAL_ERROR_EXIT();
  }

  auto role = ServerState::instance()->getRole();
  if (role == ServerState::ROLE_UNDEFINED) {
    // no role found
    LOG_TOPIC("613f4", FATAL, arangodb::Logger::CLUSTER)
        << "unable to determine unambiguous role for server '"
        << ServerState::instance()->getId()
        << "'. No role configured in agency ("
        << inspection::json(AsyncAgencyCommManager::INSTANCE->agents()) << ")";
    FATAL_ERROR_EXIT();
  }
}

DECLARE_COUNTER(arangodb_dropped_followers_total,
                "Number of drop-follower events");
DECLARE_COUNTER(
    arangodb_refused_followers_total,
    "Number of refusal answers from a follower during synchronous replication");
DECLARE_COUNTER(arangodb_sync_wrong_checksum_total,
                "Number of times a mismatching shard checksum was detected "
                "when syncing shards");
DECLARE_COUNTER(arangodb_sync_rebuilds_total,
                "Number of times a follower shard needed to be completely "
                "rebuilt because of too many synchronization failures");
DECLARE_COUNTER(arangodb_sync_tree_rebuilds_total,
                "Number of times a shard rebuilt its revision tree "
                "completely because of too many synchronization failures");
DECLARE_COUNTER(arangodb_potentially_dirty_document_reads_total,
                "Number of document reads which could be dirty");
DECLARE_COUNTER(arangodb_dirty_read_queries_total,
                "Number of queries which could be doing dirty reads");
DECLARE_COUNTER(arangodb_network_connectivity_failures_coordinators_total,
                "Number of times the cluster-internal connectivity check for "
                "Coordinators failed.");
DECLARE_COUNTER(arangodb_network_connectivity_failures_dbservers_total,
                "Number of times the cluster-internal connectivity check for "
                "DB-Servers failed.");

// IMPORTANT: Please read the first comment block a couple of lines down, before
// Adding code to this section.
void ClusterFeature::start() {
  // return if cluster is disabled
  if (!_options.enableCluster) {
    startHeartbeatThread(nullptr, 5000, 5,
                         _options.noHeartbeatDelayBeforeShutdown,
                         std::string());
    return;
  }

  auto role = ServerState::instance()->getRole();
  TRI_ASSERT(role != ServerState::ROLE_UNDEFINED);

  // We need to wait for any cluster operation, which needs access to the
  // agency cache for it to become ready. The essentials in the cluster, namely
  // ClusterInfo etc, need to start after first poll result from the agency.
  // This is of great importance to not accidentally delete data facing an
  // empty agency. There are also other measures that guard against such a
  // outcome. But there is also no point continuing with a first agency poll.
  if (role != ServerState::ROLE_AGENT && role != ServerState::ROLE_UNDEFINED) {
    if (!_agencyCache->start()) {
      LOG_TOPIC("4680e", FATAL, Logger::CLUSTER)
          << "unable to start agency cache thread";
      FATAL_ERROR_EXIT();
    }

    LOG_TOPIC("bae31", DEBUG, Logger::CLUSTER)
        << "Waiting for agency cache to become ready.";

    _agencyCache->waitFor(1).waitAndGet();
    LOG_TOPIC("13eab", DEBUG, Logger::CLUSTER)
        << "Agency cache is ready. Starting cluster cache syncers";
  }

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
          << "Waiting for DB-Servers to show up...";

      _clusterInfo->loadCurrentDBServers();
      std::vector<ServerID> DBServers = _clusterInfo->getCurrentDBServers();
      if (DBServers.size() >= 1 &&
          (DBServers.size() > 1 || TRI_microtime() - start > waitTime)) {
        LOG_TOPIC("22f55", INFO, arangodb::Logger::CLUSTER)
            << "Found " << DBServers.size() << " DB-Servers.";
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  ServerState::instance()->setState(ServerState::STATE_STARTUP);

  // tell the agency about our state
  AgencyComm comm(server());
  comm.sendServerState(120.0);

  auto const version = comm.version();

  std::string const endpoints =
      AsyncAgencyCommManager::INSTANCE->getCurrentAgent().endpoint;

  std::string myId = ServerState::instance()->getId();

  if (role == ServerState::RoleEnum::ROLE_DBSERVER) {
    _followersDroppedCounter =
        &_metrics.add(arangodb_dropped_followers_total{});
    _followersRefusedCounter =
        &_metrics.add(arangodb_refused_followers_total{});
    _followersWrongChecksumCounter =
        &_metrics.add(arangodb_sync_wrong_checksum_total{});
    _followersTotalRebuildCounter =
        &_metrics.add(arangodb_sync_rebuilds_total{});
    _syncTreeRebuildCounter =
        &_metrics.add(arangodb_sync_tree_rebuilds_total{});
  } else if (role == ServerState::RoleEnum::ROLE_COORDINATOR) {
    _potentiallyDirtyDocumentReadsCounter =
        &_metrics.add(arangodb_potentially_dirty_document_reads_total{});
    _dirtyReadQueriesCounter =
        &_metrics.add(arangodb_dirty_read_queries_total{});
  }

  if (role == ServerState::RoleEnum::ROLE_DBSERVER ||
      role == ServerState::RoleEnum::ROLE_COORDINATOR) {
    _connectivityCheckFailsCoordinators = &_metrics.add(
        arangodb_network_connectivity_failures_coordinators_total{});
    _connectivityCheckFailsDBServers =
        &_metrics.add(arangodb_network_connectivity_failures_dbservers_total{});
  }

  LOG_TOPIC("b6826", INFO, arangodb::Logger::CLUSTER)
      << "Cluster feature is turned on"
      << (_options.forceOneShard ? " with one-shard mode" : "")
      << ". Agency version: " << version << ", Agency endpoints: " << endpoints
      << ", server id: '" << myId
      << "', internal endpoint / address: " << _options.myEndpoint
      << ", advertised endpoint: "
      << (_options.myAdvertisedEndpoint.empty() ? "-"
                                                : _options.myAdvertisedEndpoint)
      << ", role: " << ServerState::roleToString(role);

  TRI_ASSERT(_agencyCache);
  auto [acb, idx] = _agencyCache->read(std::vector<std::string>{
      AgencyCommHelper::path("Sync/HeartbeatIntervalMs")});
  auto result = acb->slice();

  if (result.isArray()) {
    velocypack::Slice HeartbeatIntervalMs =
        result[0].get(std::vector<std::string>(
            {AgencyCommHelper::path(), "Sync", "HeartbeatIntervalMs"}));

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

  startHeartbeatThread(_agencyCallbackRegistry.get(), _heartbeatInterval, 5,
                       _options.noHeartbeatDelayBeforeShutdown, endpoints);
  _clusterInfo->startSyncers();

  comm.increment("Current/Version");

  AsyncAgencyCommManager::INSTANCE->setSkipScheduler(false);
  ServerState::instance()->setState(ServerState::STATE_SERVING);

#ifdef USE_ENTERPRISE
  // If we are on a coordinator, we want to have a callback which is called
  // whenever a hotbackup restore is done:
  if (role == ServerState::ROLE_COORDINATOR) {
    auto hotBackupRestoreDone = [this](VPackSlice const& result) -> bool {
      if (!server().isStopping()) {
        LOG_TOPIC("12636", INFO, Logger::BACKUP)
            << "Got a hotbackup restore "
               "event, getting new cluster-wide unique IDs...";
        this->_clusterInfo->uniqid(1000000);
      }
      return true;
    };
    _hotbackupRestoreCallback =
        std::make_shared<AgencyCallback>(server(), "Sync/HotBackupRestoreDone",
                                         hotBackupRestoreDone, true, false);
    Result r =
        _agencyCallbackRegistry->registerCallback(_hotbackupRestoreCallback);
    if (r.fail()) {
      LOG_TOPIC("82516", WARN, Logger::BACKUP)
          << "Could not register hotbackup restore callback, this could lead "
             "to problems after a restore!";
    }
  }
#endif

  if (_options.connectivityCheckInterval > 0 &&
      (role == ServerState::ROLE_COORDINATOR ||
       role == ServerState::ROLE_DBSERVER)) {
    // if connectivity checks are enabled, start the first one 15s after
    // ClusterFeature start. we also add a bit of random noise to the start
    // time offset so that when multiple servers are started at the same time,
    // they don't execute their connectivity checks all at the same time
    scheduleConnectivityCheck(15 +
                              RandomGenerator::interval(std::uint32_t(15)));
  }
}

void ClusterFeature::beginShutdown() {
  if (_options.enableCluster) {
    _clusterInfo->beginShutdown();

    std::lock_guard<std::mutex> guard(_connectivityCheckMutex);
    _connectivityCheck.reset();
  }
  _agencyCache->beginShutdown();
}

void ClusterFeature::stop() {
  shutdownHeartbeatThread();

  if (_options.enableCluster) {
    {
      std::lock_guard<std::mutex> guard(_connectivityCheckMutex);
      _connectivityCheck.reset();
    }

#ifdef USE_ENTERPRISE
    if (_hotbackupRestoreCallback != nullptr) {
      if (!_agencyCallbackRegistry->unregisterCallback(
              _hotbackupRestoreCallback)) {
        LOG_TOPIC("84152", DEBUG, Logger::BACKUP)
            << "Strange, we could not "
               "unregister the hotbackup restore callback.";
      }
    }
#endif

    // change into shutdown state
    ServerState::instance()->setState(ServerState::STATE_SHUTDOWN);

    // wait only a few seconds to broadcast our "shut down" state.
    // if we wait much longer, and the agency has already been shut
    // down, we may cause our instance to hopelessly hang and try
    // to write something into a non-existing agency.
    AgencyComm comm(server());
    // this will be stored in transient only
    comm.sendServerState(4.0);

    // the following ops will be stored in Plan/Current (for unregister) or
    // Current (for logoff)
    if (_options.unregisterOnShutdown) {
      // also use a relatively short timeout here, for the same reason as above.
      ServerState::instance()->unregister(30.0);
    } else {
      // log off the server from the agency, without permanently removing it
      // from the cluster setup.
      ServerState::instance()->logoff(10.0);
    }

    AsyncAgencyCommManager::INSTANCE->setStopping(true);

    shutdown();

    // We try to actively cancel all open requests that may still be in the
    // Agency. We cannot react to them anymore.
    _asyncAgencyCommPool->shutdownConnections();
    _asyncAgencyCommPool->drainConnections();
    _asyncAgencyCommPool->stop();
  }
}

void ClusterFeature::unprepare() {
  if (_options.enableCluster) {
    _clusterInfo->unprepare();
  }
  _agencyCache.reset();
}

void ClusterFeature::shutdown() try {
  if (!_options.enableCluster) {
    shutdownHeartbeatThread();
  }

  if (_clusterInfo != nullptr) {
    _clusterInfo->beginShutdown();
  }

  // force shutdown of AgencyCache. under normal circumstances the cache will
  // have been shut down already when we get here, but there are rare cases in
  // which ClusterFeature::stop() isn't called (e.g. during testing or if
  // something goes very wrong at startup)
  shutdownAgencyCache();

  // force shutdown of Plan/Current syncers. under normal circumstances they
  // have been shut down already when we get here, but there are rare cases in
  // which ClusterFeature::stop() isn't called (e.g. during testing or if
  // something goes very wrong at startup)
  waitForSyncersToStop();

  // make sure agency cache is unreachable now
  _agencyCache.reset();

  // must make sure that the HeartbeatThread is fully stopped before
  // we destroy the AgencyCallbackRegistry.
  _heartbeatThread.reset();

  if (_asyncAgencyCommPool) {
    _asyncAgencyCommPool->drainConnections();
    _asyncAgencyCommPool->stop();
  }
} catch (...) {
  // this is called from the dtor. not much we can do here except logging
  LOG_TOPIC("9f538", WARN, Logger::CLUSTER)
      << "caught exception during cluster shutdown";
}

void ClusterFeature::setUnregisterOnShutdown(bool unregisterOnShutdown) {
  _options.unregisterOnShutdown = unregisterOnShutdown;
}

/// @brief common routine to start heartbeat with or without cluster active
void ClusterFeature::startHeartbeatThread(
    AgencyCallbackRegistry* agencyCallbackRegistry, uint64_t interval_ms,
    uint64_t maxFailsBeforeWarning, double noHeartbeatDelayBeforeShutdown,
    std::string const& endpoints) {
  _heartbeatThread = std::make_shared<HeartbeatThread>(
      server(), agencyCallbackRegistry,
      std::chrono::microseconds(interval_ms * 1000), maxFailsBeforeWarning,
      noHeartbeatDelayBeforeShutdown);

  if (!_heartbeatThread->init() || !_heartbeatThread->start()) {
    // failure only occures in cluster mode.
    LOG_TOPIC("7e050", FATAL, arangodb::Logger::CLUSTER)
        << "heartbeat could not connect to agency endpoints (" << endpoints
        << ")";
    FATAL_ERROR_EXIT();
  }

  while (!_heartbeatThread->isReady()) {
    // wait until heartbeat is ready
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void ClusterFeature::pruneAsyncAgencyConnectionPool() {
  _asyncAgencyCommPool->pruneConnections();
}

void ClusterFeature::shutdownHeartbeatThread() {
  if (_heartbeatThread != nullptr) {
    _heartbeatThread->beginShutdown();
    auto start = std::chrono::steady_clock::now();
    size_t counter = 0;
    while (_heartbeatThread->isRunning()) {
      if (std::chrono::steady_clock::now() - start > std::chrono::seconds(65)) {
        LOG_TOPIC("d8a5b", FATAL, Logger::CLUSTER)
            << "exiting prematurely as we failed terminating the heartbeat "
               "thread";
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        FATAL_ERROR_ABORT();
#else
        FATAL_ERROR_EXIT();
#endif
      }
      if (++counter % 50 == 0) {
        LOG_TOPIC("acaa9", WARN, arangodb::Logger::CLUSTER)
            << "waiting for heartbeat thread to finish";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

/// @brief wait for the Plan and Current syncer to shut down
/// note: this may be called multiple times during shutdown
void ClusterFeature::waitForSyncersToStop() {
  if (_clusterInfo != nullptr) {
    _clusterInfo->waitForSyncersToStop();
  }
}

/// @brief wait for the AgencyCache to shut down
/// note: this may be called multiple times during shutdown
void ClusterFeature::shutdownAgencyCache() {
  if (_agencyCache != nullptr) {
    _agencyCache->beginShutdown();
    auto start = std::chrono::steady_clock::now();
    size_t counter = 0;
    while (_agencyCache->isRunning()) {
      if (std::chrono::steady_clock::now() - start > std::chrono::seconds(65)) {
        LOG_TOPIC("b5a8d", FATAL, Logger::CLUSTER)
            << "exiting prematurely as we failed terminating the agency cache";
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        FATAL_ERROR_ABORT();
#else
        FATAL_ERROR_EXIT();
#endif
      }
      if (++counter % 50 == 0) {
        LOG_TOPIC("acab0", WARN, arangodb::Logger::CLUSTER)
            << "waiting for agency cache thread to finish";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

void ClusterFeature::notify() {
  if (_heartbeatThread != nullptr) {
    _heartbeatThread->notify();
  }
}

std::shared_ptr<HeartbeatThread> ClusterFeature::heartbeatThread() {
  return _heartbeatThread;
}

ClusterInfo& ClusterFeature::clusterInfo() {
  if (!_clusterInfo) {
    if (!server().isStopping()) {
      TRI_ASSERT(_clusterInfo != nullptr)
          << "_clusterInfo is null, but server is not shutting down";

      LOG_TOPIC("325b6", ERR, arangodb::Logger::CLUSTER)
          << "_clusterInfo is null, but server is not shutting down";
      //  log crash dump feature
      crash_handler::CrashHandler::logBacktrace();
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  return *_clusterInfo;
}

AgencyCache& ClusterFeature::agencyCache() {
  if (_agencyCache == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  return *_agencyCache;
}

void ClusterFeature::allocateMembers() {
  _agencyCallbackRegistry = std::make_unique<AgencyCallbackRegistry>(
      server(), *this, server().getFeature<DatabaseFeature>(), _metrics,
      agencyCallbacksPath());
  _agencyCache = std::make_unique<AgencyCache>(
      server(), *_agencyCallbackRegistry, _syncerShutdownCode);
  _clusterInfo = std::make_unique<ClusterInfo>(server(), *_agencyCache,
                                               *_agencyCallbackRegistry,
                                               _syncerShutdownCode, _metrics);
}

void ClusterFeature::addDirty(
    containers::FlatHashSet<std::string> const& databases, bool callNotify) {
  if (databases.size() > 0) {
    std::lock_guard guard{_dirtyLock};
    for (auto const& database : databases) {
      if (_dirtyDatabases.emplace(database).second) {
        LOG_TOPIC("35b75", DEBUG, Logger::MAINTENANCE)
            << "adding " << database << " to dirty databases";
      }
    }
    if (callNotify) {
      notify();
    }
  }
}

void ClusterFeature::addDirty(
    containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder>> const&
        databases) {
  if (databases.size() > 0) {
    std::lock_guard guard{_dirtyLock};
    bool addedAny = false;
    for (auto const& database : databases) {
      if (_dirtyDatabases.emplace(database.first).second) {
        addedAny = true;
        LOG_TOPIC("35b77", DEBUG, Logger::MAINTENANCE)
            << "adding " << database << " to dirty databases";
      }
    }
    if (addedAny) {
      notify();
    }
  }
}

void ClusterFeature::addDirty(std::string const& database) {
  std::lock_guard guard{_dirtyLock};
  if (_dirtyDatabases.emplace(database).second) {
    LOG_TOPIC("357b9", DEBUG, Logger::MAINTENANCE)
        << "adding " << database << " to dirty databases";
  }
  // This notify is needed even if no database is added
  notify();
}

containers::FlatHashSet<std::string> ClusterFeature::dirty() {
  std::lock_guard guard{_dirtyLock};
  containers::FlatHashSet<std::string> ret;
  ret.swap(_dirtyDatabases);
  return ret;
}

bool ClusterFeature::isDirty(std::string const& dbName) const {
  std::lock_guard guard{_dirtyLock};
  return _dirtyDatabases.find(dbName) != _dirtyDatabases.end();
}

std::unordered_set<std::string> ClusterFeature::allDatabases() const {
  std::unordered_set<std::string> allDBNames;
  auto const tmp = server().getFeature<DatabaseFeature>().getDatabaseNames();
  allDBNames.reserve(tmp.size());
  for (auto const& i : tmp) {
    allDBNames.emplace(i);
  }
  return allDBNames;
}

void ClusterFeature::scheduleConnectivityCheck(std::uint32_t inSeconds) {
  TRI_ASSERT(_options.connectivityCheckInterval > 0);

  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler == nullptr || inSeconds == 0) {
    return;
  }

  std::lock_guard<std::mutex> guard(_connectivityCheckMutex);

  if (server().isStopping()) {
    return;
  }

  auto workItem = arangodb::SchedulerFeature::SCHEDULER->queueDelayed(
      "connectivity-check", RequestLane::INTERNAL_LOW,
      std::chrono::seconds(inSeconds), [this](bool canceled) {
        if (canceled) {
          return;
        }

        if (!this->server().isStopping()) {
          runConnectivityCheck();
        }
        scheduleConnectivityCheck(_options.connectivityCheckInterval +
                                  RandomGenerator::interval(std::uint32_t(3)));
      });

  _connectivityCheck = std::move(workItem);
}

void ClusterFeature::runConnectivityCheck() {
  TRI_ASSERT(ServerState::instance()->isCoordinator() ||
             ServerState::instance()->isDBServer());

  TRI_ASSERT(_connectivityCheckFailsCoordinators != nullptr);
  TRI_ASSERT(_connectivityCheckFailsDBServers != nullptr);

  NetworkFeature const& nf = server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    return;
  }

  if (_clusterInfo == nullptr) {
    return;
  }

  // we want to contact coordinators and DB servers, potentially
  // including _ourselves_ (we need to be able to send requests
  // to ourselves)
  auto servers = _clusterInfo->getCurrentCoordinators();
  for (auto& it : _clusterInfo->getCurrentDBServers()) {
    servers.emplace_back(std::move(it));
  }

  LOG_TOPIC("601e3", DEBUG, Logger::CLUSTER)
      << "sending connectivity check requests to " << servers.size()
      << " servers: " << servers;

  // run a basic connectivity check by calling /_api/version
  static constexpr double timeout = 10.0;
  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(timeout);

  std::vector<futures::Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& server : servers) {
    futures.emplace_back(network::sendRequest(pool, "server:" + server,
                                              fuerte::RestVerb::Get,
                                              "/_api/version", {}, reqOpts));
  }

  for (futures::Future<network::Response>& f : futures) {
    if (this->server().isStopping()) {
      break;
    }
    network::Response const& r = f.waitAndGet();
    TRI_ASSERT(r.destination.starts_with("server:"));

    if (r.ok()) {
      LOG_TOPIC("803c0", DEBUG, Logger::CLUSTER)
          << "connectivity check for endpoint " << r.destination
          << " successful";
    } else {
      LOG_TOPIC("43fc0", WARN, Logger::CLUSTER)
          << "unable to connect to endpoint " << r.destination << " within "
          << timeout << " seconds: " << r.combinedResult().errorMessage();

      auto ep = std::string_view(r.destination);
      if (!ep.starts_with("server:")) {
        TRI_ASSERT(false);
        continue;
      }
      // strip "server:" prefix
      ep = ep.substr(strlen("server:"));
      if (ep.starts_with("PRMR-")) {
        // DB-Server
        _connectivityCheckFailsDBServers->count();
      } else if (ep.starts_with("CRDN-")) {
        _connectivityCheckFailsCoordinators->count();
      } else {
        // unknown server type!
        TRI_ASSERT(false);
      }
    }
  }
}

ResultT<std::string> ClusterFeature::getDeploymentId() const {
  // Check if ServerState is available (not during early startup or shutdown)
  if (ServerState::instance() == nullptr) {
    return ResultT<std::string>::error(
        TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
        "server state not yet available or already shut down");
  }

  std::string deploymentId;

  // For single servers, use the server ID
  if (ServerState::instance()->isSingleServer()) {
    // Create a UUID with the lower 48 bits from server ID
    // and constant upper 80 bits
    boost::uuids::uuid uuid;

    // Get the server ID (only lower 48 bits are usable)
    uint64_t serverId = ServerIdFeature::getId().id();
    uint64_t serverIdLower48 = serverId & 0xFFFFFFFFFFFFULL;

    // Set the first 10 bytes (80 bits) to a constant pattern
    // Using a recognizable pattern for ArangoDB single server deployment IDs
    uuid.data[0] = 0x61;  // 'a'
    uuid.data[1] = 0x72;  // 'r'
    uuid.data[2] = 0x61;  // 'a'
    uuid.data[3] = 0x6e;  // 'n'
    uuid.data[4] = 0x67;  // 'g'
    uuid.data[5] = 0x6f;  // 'o'
    uuid.data[6] = 0x40;  // Version 4: random bits
    uuid.data[7] = 0x00;
    uuid.data[8] = 0x00;
    uuid.data[9] = 0x00;

    // Set the last 6 bytes (48 bits) to the server ID
    uuid.data[10] = static_cast<uint8_t>((serverIdLower48 >> 40) & 0xFF);
    uuid.data[11] = static_cast<uint8_t>((serverIdLower48 >> 32) & 0xFF);
    uuid.data[12] = static_cast<uint8_t>((serverIdLower48 >> 24) & 0xFF);
    uuid.data[13] = static_cast<uint8_t>((serverIdLower48 >> 16) & 0xFF);
    uuid.data[14] = static_cast<uint8_t>((serverIdLower48 >> 8) & 0xFF);
    uuid.data[15] = static_cast<uint8_t>(serverIdLower48 & 0xFF);

    deploymentId = boost::uuids::to_string(uuid);
    return ResultT<std::string>::success(std::move(deploymentId));
  } else if (ServerState::instance()->isCoordinator()) {
    // In cluster mode, use AgencyCache to read cluster ID synchronously
    if (_agencyCache == nullptr) {
      return ResultT<std::string>::error(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
                                         "agency cache not available");
    }

    try {
      // Read from AgencyCache (path "Cluster" will be prefixed with "arango/")
      VPackBuilder builder;
      _agencyCache->get(builder, "Cluster");

      VPackSlice slice = builder.slice();
      if (slice.isString()) {
        deploymentId = slice.copyString();
        return ResultT<std::string>::success(std::move(deploymentId));
      }

      return ResultT<std::string>::error(
          TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
          "cluster ID not found in agency cache");
    } catch (std::exception const& e) {
      return ResultT<std::string>::error(TRI_ERROR_HTTP_SERVER_ERROR, e.what());
    }
  }

  return ResultT<std::string>::error(TRI_ERROR_INTERNAL,
                                     "unexpected server state");
}
