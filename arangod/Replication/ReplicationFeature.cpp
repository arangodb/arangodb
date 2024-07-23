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

#include "ReplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/Counter.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/Gauge.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

DECLARE_COUNTER(arangodb_replication_cluster_inventory_requests_total,
                "(DC-2-DC only) Number of times the database and collection "
                "overviews have been requested.");
DECLARE_GAUGE(arangodb_replication_clients, uint64_t,
              "Number of replication clients connected");

namespace arangodb {

ReplicationFeature::ReplicationFeature(Server& server)
    : ArangodFeature{server, *this},
      _connectTimeout(10.0),
      _requestTimeout(600.0),
      _forceConnectTimeout(false),
      _forceRequestTimeout(false),
      _replicationApplierAutoStart(true),
      _syncByRevision(true),
      _autoRepairRevisionTrees(true),
      _connectionCache{
          server.getFeature<application_features::CommunicationFeaturePhase>(),
          httpclient::ConnectionCache::Options{5}},
      _parallelTailingInvocations(0),
      _maxParallelTailingInvocations(0),
      _quickKeysLimit(1000000),
      _inventoryRequests(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_replication_cluster_inventory_requests_total{})),
      _clients(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_replication_clients{})) {
  static_assert(
      Server::isCreatedAfter<ReplicationFeature,
                             application_features::CommunicationFeaturePhase,
                             metrics::MetricsFeature>());

  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<DatabaseFeature>();
  startsAfter<RocksDBEngine>();
  startsAfter<RocksDBRecoveryManager>();
  startsAfter<ServerIdFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<SystemDatabaseFeature>();
}

ReplicationFeature::~ReplicationFeature() = default;

void ReplicationFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("replication", "replication");
  options->addOption(
      "--replication.auto-start",
      "Enable or disable the automatic start of replication appliers.",
      new BooleanParameter(&_replicationApplierAutoStart),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOldOption("server.disable-replication-applier",
                        "replication.auto-start");
  options->addOldOption("database.replication-applier",
                        "replication.auto-start");

  options->addObsoleteOption(
      "--replication.active-failover",
      "Enable active-failover during asynchronous replication.", false);
  options->addOldOption("--replication.automatic-failover",
                        "--replication.active-failover");

  options->addOption(
      "--replication.max-parallel-tailing-invocations",
      "The maximum number of concurrently allowed WAL tailing invocations "
      "(0 = unlimited).",
      new UInt64Parameter(&_maxParallelTailingInvocations),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption("--replication.connect-timeout",
                     "The default timeout value for replication connection "
                     "attempts (in seconds).",
                     new DoubleParameter(&_connectTimeout));
  options->addOption("--replication.request-timeout",
                     "The default timeout value for replication requests "
                     "(in seconds).",
                     new DoubleParameter(&_requestTimeout));

  options->addOption(
      "--replication.quick-keys-limit",
      "Limit at which 'quick' calls to the replication keys API return "
      "only the document count for the second run.",
      new UInt64Parameter(&_quickKeysLimit),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--replication.sync-by-revision",
      "Whether to use the newer revision-based replication protocol.",
      new BooleanParameter(&_syncByRevision));

  options
      ->addOption("--replication.auto-repair-revision-trees",
                  "Whether to automatically repair revision trees of shards "
                  "after too many shard synchronization failures.",
                  new BooleanParameter(&_autoRepairRevisionTrees),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(31006);

  options->addObsoleteOption(
      "--replication.active-failover-leader-grace-period",
      "The amount of time (in seconds) for which the current leader will "
      "continue to assume its leadership even if it lost connection to the "
      "agency (0 = unlimited)",
      true);
}

void ReplicationFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  if (_connectTimeout < 1.0) {
    _connectTimeout = 1.0;
  }
  if (options->processingResult().touched("--replication.connect-timeout")) {
    _forceConnectTimeout = true;
  }

  if (_requestTimeout < 3.0) {
    _requestTimeout = 3.0;
  }
  if (options->processingResult().touched("--replication.request-timeout")) {
    _forceRequestTimeout = true;
  }
}

void ReplicationFeature::prepare() {
  if (ServerState::instance()->isCoordinator()) {
    setEnabled(false);
    return;
  }
}

void ReplicationFeature::start() {
  _globalReplicationApplier = std::make_unique<GlobalReplicationApplier>(
      GlobalReplicationApplier::loadConfiguration(server()));

  try {
    _globalReplicationApplier->loadState();
  } catch (...) {
    // :snake:
  }

  LOG_TOPIC("1214b", DEBUG, Logger::REPLICATION)
      << "checking global applier startup. autoStart: "
      << _globalReplicationApplier->autoStart()
      << ", hasState: " << _globalReplicationApplier->hasState();

  if (_globalReplicationApplier->autoStart() &&
      _globalReplicationApplier->hasState() && _replicationApplierAutoStart) {
    _globalReplicationApplier->startTailing(/*initialTick*/ 0,
                                            /*useTick*/ false);
  }
}

void ReplicationFeature::beginShutdown() {
  try {
    if (_globalReplicationApplier != nullptr) {
      _globalReplicationApplier->stop();
    }
  } catch (...) {
    // ignore any error
  }
}

void ReplicationFeature::stop() {
  try {
    if (_globalReplicationApplier != nullptr) {
      _globalReplicationApplier->stop();
      _globalReplicationApplier->stopAndJoin();
    }
  } catch (...) {
    // ignore any error
  }
}

void ReplicationFeature::unprepare() {
  if (_globalReplicationApplier != nullptr) {
    _globalReplicationApplier->stopAndJoin();
  }
  _globalReplicationApplier.reset();
}

httpclient::ConnectionCache& ReplicationFeature::connectionCache() {
  return _connectionCache;
}

/// @brief track the number of (parallel) tailing operations
/// will throw an exception if the number of concurrently running operations
/// would exceed the configured maximum
void ReplicationFeature::trackTailingStart() {
  if (++_parallelTailingInvocations > _maxParallelTailingInvocations &&
      _maxParallelTailingInvocations > 0) {
    // we are above the configured maximum
    --_parallelTailingInvocations;
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "too many parallel invocations of WAL tailing operations");
  }
}

/// @brief count down the number of parallel tailing operations
/// must only be called after a successful call to trackTailingstart
void ReplicationFeature::trackTailingEnd() noexcept {
  --_parallelTailingInvocations;
}

void ReplicationFeature::trackInventoryRequest() noexcept {
  ++_inventoryRequests;
}

double ReplicationFeature::checkConnectTimeout(double value) const {
  if (_forceConnectTimeout) {
    return _connectTimeout;
  }
  return value;
}

double ReplicationFeature::checkRequestTimeout(double value) const {
  if (_forceRequestTimeout) {
    return _requestTimeout;
  }
  return value;
}

bool ReplicationFeature::syncByRevision() const noexcept {
  return _syncByRevision;
}

bool ReplicationFeature::autoRepairRevisionTrees() const noexcept {
  return _autoRepairRevisionTrees;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
// only used during testing
void ReplicationFeature::autoRepairRevisionTrees(bool value) noexcept {
  _autoRepairRevisionTrees = value;
}
#endif

// start the replication applier for a single database
void ReplicationFeature::startApplier(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(vocbase->replicationApplier() != nullptr);

  if (!ServerState::instance()->isClusterRole() &&
      vocbase->replicationApplier()->autoStart()) {
    if (!_replicationApplierAutoStart) {
      LOG_TOPIC("c5378", INFO, arangodb::Logger::REPLICATION)
          << "replication applier explicitly deactivated for database '"
          << vocbase->name() << "'";
    } else {
      try {
        vocbase->replicationApplier()->startTailing(/*initialTick*/ 0,
                                                    /*useTick*/ false);
      } catch (std::exception const& ex) {
        LOG_TOPIC("2038f", WARN, arangodb::Logger::REPLICATION)
            << "unable to start replication applier for database '"
            << vocbase->name() << "': " << ex.what();
      } catch (...) {
        LOG_TOPIC("76ad6", WARN, arangodb::Logger::REPLICATION)
            << "unable to start replication applier for database '"
            << vocbase->name() << "'";
      }
    }
  }
}

GlobalReplicationApplier* ReplicationFeature::globalReplicationApplier() const {
  TRI_ASSERT(_globalReplicationApplier != nullptr);
  return _globalReplicationApplier.get();
}

void ReplicationFeature::disableReplicationApplier() {
  _replicationApplierAutoStart = false;
}

// stop the replication applier for a single database
void ReplicationFeature::stopApplier(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  if (!ServerState::instance()->isClusterRole() &&
      vocbase->replicationApplier() != nullptr) {
    vocbase->replicationApplier()->stopAndJoin();
  }
}

/// @brief returns the connect timeout for replication requests
double ReplicationFeature::connectTimeout() const { return _connectTimeout; }

/// @brief returns the request timeout for replication requests
double ReplicationFeature::requestTimeout() const { return _requestTimeout; }

}  // namespace arangodb
