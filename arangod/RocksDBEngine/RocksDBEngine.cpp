////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBLogger.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/build.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Manager.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationClients.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Rest/Version.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/LanguageCheckFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RocksDBEngine/Listeners/RocksDBBackgroundErrorListener.h"
#include "RocksDBEngine/Listeners/RocksDBMetricsListener.h"
#include "RocksDBEngine/Listeners/RocksDBThrottle.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBBackgroundThread.h"
#include "RocksDBEngine/RocksDBChecksumEnv.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBIncrementalSync.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillFeature.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBOptimizerRules.h"
#include "RocksDBEngine/RocksDBOptionFeature.h"
#include "RocksDBEngine/RocksDBPersistedLog.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "RocksDBEngine/RocksDBRestHandlers.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBChecksumEnv.h"
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBUpgrade.h"
#include "RocksDBEngine/RocksDBV8Functions.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBWalAccess.h"
#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Manager.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalView.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/ticks.h"
#include "Inspection/VPack.h"

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/sst_file_reader.h>
#include <rocksdb/statistics.h>
#include <rocksdb/table.h>
#include <rocksdb/transaction_log.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>

#include <iomanip>
#include <limits>
#include <utility>

// we will not use the multithreaded index creation that uses rocksdb's sst
// file ingestion until rocksdb external file ingestion is fixed to have
// correct sequence numbers for the files without gaps
#undef USE_SST_INGESTION

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

DECLARE_GAUGE(rocksdb_wal_sequence, uint64_t, "Current RocksDB WAL sequence");
DECLARE_GAUGE(
    rocksdb_wal_sequence_lower_bound, uint64_t,
    "RocksDB WAL sequence number until which background thread has caught up");
DECLARE_GAUGE(rocksdb_archived_wal_files, uint64_t,
              "Number of archived RocksDB WAL files");
DECLARE_GAUGE(rocksdb_prunable_wal_files, uint64_t,
              "Number of prunable RocksDB WAL files");
DECLARE_GAUGE(rocksdb_wal_pruning_active, uint64_t,
              "Whether or not RocksDB WAL file pruning is active");
DECLARE_GAUGE(arangodb_revision_tree_memory_usage, uint64_t,
              "Total memory consumed by all revision trees");
DECLARE_COUNTER(arangodb_revision_tree_rebuilds_success_total,
                "Number of successful revision tree rebuilds");
DECLARE_COUNTER(arangodb_revision_tree_rebuilds_failure_total,
                "Number of failed revision tree rebuilds");
DECLARE_COUNTER(arangodb_revision_tree_hibernations_total,
                "Number of revision tree hibernations");
DECLARE_COUNTER(arangodb_revision_tree_resurrections_total,
                "Number of revision tree resurrections");

// global flag to cancel all compactions. will be flipped to true on shutdown
static std::atomic<bool> cancelCompactions{false};

// minimum value for --rocksdb.sync-interval (in ms)
// a value of 0 however means turning off the syncing altogether!
static constexpr uint64_t minSyncInterval = 5;

static constexpr uint64_t databaseIdForGlobalApplier = 0;

// handles for recovery helpers
std::vector<std::shared_ptr<RocksDBRecoveryHelper>>
    RocksDBEngine::_recoveryHelpers;

RocksDBFilePurgePreventer::RocksDBFilePurgePreventer(RocksDBEngine* engine)
    : _engine(engine) {
  TRI_ASSERT(_engine != nullptr);
  _engine->_purgeLock.lockRead();
}

RocksDBFilePurgePreventer::~RocksDBFilePurgePreventer() {
  if (_engine != nullptr) {
    _engine->_purgeLock.unlockRead();
  }
}

RocksDBFilePurgePreventer::RocksDBFilePurgePreventer(
    RocksDBFilePurgePreventer&& other)
    : _engine(other._engine) {
  // steal engine from other
  other._engine = nullptr;
}

RocksDBFilePurgeEnabler::RocksDBFilePurgeEnabler(RocksDBEngine* engine)
    : _engine(nullptr) {
  TRI_ASSERT(engine != nullptr);

  if (engine->_purgeLock.tryLockWrite()) {
    // we got the lock
    _engine = engine;
  }
}

RocksDBFilePurgeEnabler::~RocksDBFilePurgeEnabler() {
  if (_engine != nullptr) {
    _engine->_purgeLock.unlockWrite();
  }
}

RocksDBFilePurgeEnabler::RocksDBFilePurgeEnabler(
    RocksDBFilePurgeEnabler&& other)
    : _engine(other._engine) {
  // steal engine from other
  other._engine = nullptr;
}

// create the storage engine
RocksDBEngine::RocksDBEngine(Server& server,
                             RocksDBOptionsProvider const& optionsProvider)
    : StorageEngine(server, kEngineName, name(), Server::id<RocksDBEngine>(),
                    std::make_unique<RocksDBIndexFactory>(server)),
      _optionsProvider(optionsProvider),
      _db(nullptr),
      _walAccess(std::make_unique<RocksDBWalAccess>(*this)),
      _maxTransactionSize(transaction::Options::defaultMaxTransactionSize),
      _intermediateCommitSize(
          transaction::Options::defaultIntermediateCommitSize),
      _intermediateCommitCount(
          transaction::Options::defaultIntermediateCommitCount),
      _maxParallelCompactions(2),
      _pruneWaitTime(10.0),
      _pruneWaitTimeInitial(180.0),
      _maxWalArchiveSizeLimit(0),
      _releasedTick(0),
      _syncInterval(100),
      _syncDelayThreshold(5000),
      _requiredDiskFreePercentage(0.01),
      _requiredDiskFreeBytes(16 * 1024 * 1024),
      _useThrottle(true),
      _useReleasedTick(false),
      _debugLogging(false),
      _useEdgeCache(true),
      _verifySst(false),
#ifdef USE_ENTERPRISE
      _createShaFiles(true),
#else
      _createShaFiles(false),
#endif
      _useRangeDeleteInWal(true),
      _lastHealthCheckSuccessful(false),
      _dbExisted(false),
      _runningRebuilds(0),
      _runningCompactions(0),
      _metricsWalSequenceLowerBound(
          server.getFeature<metrics::MetricsFeature>().add(
              rocksdb_wal_sequence_lower_bound{})),
      _metricsArchivedWalFiles(server.getFeature<metrics::MetricsFeature>().add(
          rocksdb_archived_wal_files{})),
      _metricsPrunableWalFiles(server.getFeature<metrics::MetricsFeature>().add(
          rocksdb_prunable_wal_files{})),
      _metricsWalPruningActive(server.getFeature<metrics::MetricsFeature>().add(
          rocksdb_wal_pruning_active{})),
      _metricsTreeMemoryUsage(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_revision_tree_memory_usage{})),
      _metricsTreeRebuildsSuccess(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_revision_tree_rebuilds_success_total{})),
      _metricsTreeRebuildsFailure(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_revision_tree_rebuilds_failure_total{})),
      _metricsTreeHibernations(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_revision_tree_hibernations_total{})),
      _metricsTreeResurrections(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_revision_tree_resurrections_total{})) {
  startsAfter<BasicFeaturePhaseServer>();
  // inherits order from StorageEngine but requires "RocksDBOption" that is used
  // to configure this engine
  startsAfter<RocksDBOptionFeature>();
  startsAfter<LanguageFeature>();
  startsAfter<LanguageCheckFeature>();
}

RocksDBEngine::~RocksDBEngine() {
  _recoveryHelpers.clear();
  shutdownRocksDBInstance();
}

/// shuts down the RocksDB instance. this is called from unprepare
/// and the dtor
void RocksDBEngine::shutdownRocksDBInstance() noexcept {
  if (_db == nullptr) {
    return;
  }

  // turn off RocksDBThrottle, and release our pointers to it
  if (_throttleListener != nullptr) {
    _throttleListener->stopThread();
  }

  for (rocksdb::ColumnFamilyHandle* h :
       RocksDBColumnFamilyManager::allHandles()) {
    _db->DestroyColumnFamilyHandle(h);
  }

  // now prune all obsolete WAL files
  try {
    determinePrunableWalFiles(0);
    pruneWalFiles();
  } catch (...) {
    // this is allowed to go wrong on shutdown
    // we must not throw an exception from here
  }

  try {
    // do a final WAL sync here before shutting down
    Result res = RocksDBSyncThread::sync(_db->GetBaseDB());
    if (res.fail()) {
      LOG_TOPIC("14ede", WARN, Logger::ENGINES)
          << "could not sync RocksDB WAL: " << res.errorMessage();
    }

    rocksdb::Status status = _db->Close();

    if (!status.ok()) {
      Result res = rocksutils::convertStatus(status);
      LOG_TOPIC("2b9c1", ERR, Logger::ENGINES)
          << "could not shutdown RocksDB: " << res.errorMessage();
    }
  } catch (...) {
    // this is allowed to go wrong on shutdown
    // we must not throw an exception from here
  }

  delete _db;
  _db = nullptr;
}

// inherited from ApplicationFeature
// ---------------------------------

// add the storage engine's specific options to the global list of options
void RocksDBEngine::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("rocksdb", "RocksDB engine");

  /// @brief minimum required percentage of free disk space for considering the
  /// server "healthy". this is expressed as a floating point value between 0
  /// and 1! if set to 0.0, the % amount of free disk is ignored in checks.
  options
      ->addOption(
          "--rocksdb.minimum-disk-free-percent",
          "The minimum percentage of free disk space for considering the "
          "server healthy in health checks (0 = disable the check).",
          new DoubleParameter(&_requiredDiskFreePercentage, /*base*/ 1.0,
                              /*minValue*/ 0.0, /*maxValue*/ 1.0),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  /// @brief minimum number of free bytes on disk for considering the server
  /// healthy. if set to 0, the number of free bytes on disk is ignored in
  /// checks.
  options
      ->addOption("--rocksdb.minimum-disk-free-bytes",
                  "The minimum number of free disk bytes for considering the "
                  "server healthy in health checks (0 = disable the check).",
                  new UInt64Parameter(&_requiredDiskFreeBytes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  // control transaction size for RocksDB engine
  options
      ->addOption("--rocksdb.max-transaction-size",
                  "The transaction size limit (in bytes).",
                  new UInt64Parameter(&_maxTransactionSize))
      .setLongDescription(R"(Transactions store all keys and values in RAM, so
large transactions run the risk of causing out-of-memory situations. This
setting allows you to ensure that it does not happen by limiting the size of
any individual transaction. Transactions whose operations would consume more
RAM than this threshold value are aborted automatically with error 32
("resource limit exceeded").)");

  options->addOption("--rocksdb.intermediate-commit-size",
                     "An intermediate commit is performed automatically "
                     "when a transaction has accumulated operations of this "
                     "size (in bytes), and a new transaction is started.",
                     new UInt64Parameter(&_intermediateCommitSize));

  options->addOption("--rocksdb.intermediate-commit-count",
                     "An intermediate commit is performed automatically "
                     "when this number of operations is reached in a "
                     "transaction, and a new transaction is started.",
                     new UInt64Parameter(&_intermediateCommitCount));

  options
      ->addOption("--rocksdb.max-parallel-compactions",
                  "The maximum number of parallel compactions jobs.",
                  new UInt64Parameter(&_maxParallelCompactions))
      .setIntroducedIn(30711);

  options
      ->addOption(
          "--rocksdb.sync-interval",
          "The interval for automatic, non-requested disk syncs (in "
          "milliseconds, 0 = turn automatic syncing off)",
          new UInt64Parameter(&_syncInterval),
          arangodb::options::makeFlags(arangodb::options::Flags::OsLinux,
                                       arangodb::options::Flags::OsMac,
                                       arangodb::options::Flags::OnDBServer,
                                       arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Automatic synchronization of data from RocksDB's
write-ahead logs to disk is only performed for not-yet synchronized data, and
only for operations that have been executed without the `waitForSync`
attribute.)");

  options
      ->addOption(
          "--rocksdb.sync-delay-threshold",
          "The threshold for self-observation of WAL disk syncs "
          "(in milliseconds, 0 = no warnings). Any WAL disk sync longer ago "
          "than this threshold triggers a warning ",
          new UInt64Parameter(&_syncDelayThreshold),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30608)
      .setIntroducedIn(30705);

  options
      ->addOption("--rocksdb.wal-file-timeout",
                  "The timeout after which unused WAL files are deleted "
                  "(in seconds).",
                  new DoubleParameter(&_pruneWaitTime),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Data of ongoing transactions is stored in RAM.
Transactions that get too big (in terms of number of operations involved or the
total size of data created or modified by the transaction) are committed
automatically. Effectively, this means that big user transactions are split into
multiple smaller RocksDB transactions that are committed individually.
The entire user transaction does not necessarily have ACID properties in this
case.)");

  options
      ->addOption("--rocksdb.wal-file-timeout-initial",
                  "The initial timeout (in seconds) after which unused WAL "
                  "files deletion kicks in after server start.",
                  new DoubleParameter(&_pruneWaitTimeInitial),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If you decrease the value, the server starts the
removal of obsolete WAL files earlier after server start. This is useful in
testing environments that are space-restricted and do not require keeping much
WAL file data at all.)");

  options
      ->addOption("--rocksdb.throttle", "Enable write-throttling.",
                  new BooleanParameter(&_useThrottle),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If enabled, dynamically throttles the ingest rate
of writes if necessary to reduce chances of compactions getting too far behind
and blocking incoming writes.)");

  options
      ->addOption(
          "--rocksdb.throttle-slots",
          "The number of historic metrics to use for throttle value "
          "calculation.",
          new UInt64Parameter(&_throttleSlots, /*base*/ 1, /*minValue*/ 1),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(If throttling is enabled, this parameter controls
the number of previous intervals to use for throttle value calculation.)");

  options
      ->addOption(
          "--rocksdb.throttle-frequency",
          "The frequency for write-throttle calculations (in milliseconds).",
          new UInt64Parameter(&_throttleFrequency),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(If the throttling is enabled, it recalculates a
new maximum ingestion rate with this frequency.)");

  options
      ->addOption(
          "--rocksdb.throttle-scaling-factor",
          "The adaptiveness scaling factor for write-throttle calculations.",
          new UInt64Parameter(&_throttleScalingFactor),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(There is normally no need to change this value.)");

  options
      ->addOption("--rocksdb.throttle-max-write-rate",
                  "The maximum write rate enforced by throttle (in bytes per "
                  "second, 0 = unlimited).",
                  new UInt64Parameter(&_throttleMaxWriteRate),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(The actual write rate established by the
throttling is the minimum of this value and the value that the regular throttle
calculation produces, i.e. this option can be used to set a fixed upper bound
on the write rate.)");

  options
      ->addOption("--rocksdb.throttle-slow-down-writes-trigger",
                  "The number of level 0 files whose payload "
                  "is not considered in throttle calculations when penalizing "
                  "the presence of L0 files.",
                  new UInt64Parameter(&_throttleSlowdownWritesTrigger),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(There is normally no need to change this value.)");

  options
      ->addOption("--rocksdb.throttle-lower-bound-bps",
                  "The lower bound for throttle's write bandwidth "
                  "(in bytes per second).",
                  new UInt64Parameter(&_throttleLowerBoundBps),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805);

#ifdef USE_ENTERPRISE
  options->addOption("--rocksdb.create-sha-files",
                     "Whether to enable the generation of sha256 files for "
                     "each .sst file.",
                     new BooleanParameter(&_createShaFiles),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle,
                         arangodb::options::Flags::Enterprise));
#endif

  options
      ->addOption("--rocksdb.use-range-delete-in-wal",
                  "Enable range delete markers in the write-ahead log (WAL). "
                  "Potentially incompatible with older arangosync versions.",
                  new BooleanParameter(&_useRangeDeleteInWal),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30903)
      .setLongDescription(R"(Controls whether the collection truncate operation
in the cluster can use RangeDelete operations in RocksDB. Using RangeDeletes is
fast and reduces the algorithmic complexity of the truncate operation to O(1),
compared to O(n) for when this option is turned off (with n being the number of
documents in the collection/shard).

Previous versions of ArangoDB used RangeDeletes only on a single server, but
never in a cluster. 

The default value for this option is `true`, and you should only change this
value in case of emergency. This option is only honored in the cluster.
Single server and Active Failover deployments do not use RangeDeletes regardless
of the value of this option.

Note that it is not guaranteed that all truncate operations use a RangeDelete
operation. For collections containing a low number of documents, the O(n)
truncate method may still be used.)");

  options
      ->addOption("--rocksdb.debug-logging",
                  "Whether to enable RocksDB debug logging.",
                  new BooleanParameter(&_debugLogging),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If set to `true`, enables verbose logging of
RocksDB's actions into the logfile written by ArangoDB (if the
`--rocksdb.use-file-logging` option is off), or RocksDB's own log (if the
`--rocksdb.use-file-logging` option is on).

This option is turned off by default, but you can enable it for debugging
RocksDB internals and performance.)");

  options
      ->addOption("--rocksdb.edge-cache",
                  "Whether to use the in-memory cache for edges",
                  new BooleanParameter(&_useEdgeCache),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30604)
      .setDeprecatedIn(31000);

  options
      ->addOption("--rocksdb.verify-sst",
                  "Verify the validity of .sst files present in the "
                  "`engine-rocksdb` directory on startup.",
                  new BooleanParameter(&_verifySst),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Command,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100)
      .setLongDescription(R"(If set to `true`, during startup, all .sst files
in the `engine-rocksdb` folder in the database directory are checked for
potential corruption and errors. The server process stops after the check and
returns an exit code of `0` if the validation was successful, or a non-zero
exit code if there is an error in any of the .sst files.)");

  options
      ->addOption("--rocksdb.wal-archive-size-limit",
                  "The maximum total size (in bytes) of archived WAL files to "
                  "keep on the leader (0 = unlimited).",
                  new UInt64Parameter(&_maxWalArchiveSizeLimit),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(A value of `0` does not restrict the size of the
archive, so the leader removes archived WAL files when there are no replication
clients needing them. Any non-zero value restricts the size of the WAL files
archive to about the specified value and trigger WAL archive file deletion once
the threshold is reached. You can use this to get rid of archived WAL files in
a disk size-constrained environment.

**Note**: The value is only a threshold, so the archive may get bigger than 
the configured value until the background thread actually deletes files from
the archive. Also note that deletion from the archive only kicks in after
`--rocksdb.wal-file-timeout-initial` seconds have elapsed after server start.

Archived WAL files are normally deleted automatically after a short while when
there is no follower attached that may read from the archive. However, in case
when there are followers attached that may read from the archive, WAL files
normally remain in the archive until their contents have been streamed to the
followers. In case there are slow followers that cannot catch up, this causes a
growth of the WAL files archive over time.

You can use the option to force a deletion of WAL files from the archive even if
there are followers attached that may want to read the archive. In case the
option is set and a leader deletes files from the archive that followers want to
read, this aborts the replication on the followers. Followers can restart the
replication doing a resync, however.)");

#ifdef USE_ENTERPRISE
  collectEnterpriseOptions(options);
#endif
}

// validate the storage engine's specific options
void RocksDBEngine::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  transaction::Options::setLimits(_maxTransactionSize, _intermediateCommitSize,
                                  _intermediateCommitCount);
#ifdef USE_ENTERPRISE
  validateEnterpriseOptions(options);
#endif

  if (_throttleScalingFactor == 0) {
    _throttleScalingFactor = 1;
  }

  if (_throttleSlots < 8) {
    _throttleSlots = 8;
  }

  if (_syncInterval > 0) {
    if (_syncInterval < minSyncInterval) {
      // _syncInterval = 0 means turned off!
      LOG_TOPIC("bbd68", FATAL, arangodb::Logger::CONFIG)
          << "invalid value for --rocksdb.sync-interval. Please use a value "
          << "of at least " << minSyncInterval;
      FATAL_ERROR_EXIT();
    }

    if (_syncDelayThreshold > 0 && _syncDelayThreshold <= _syncInterval) {
      if (!options->processingResult().touched("rocksdb.sync-interval") &&
          options->processingResult().touched("rocksdb.sync-delay-threshold")) {
        // user has not set --rocksdb.sync-interval, but set
        // --rocksdb.sync-delay-threshold
        LOG_TOPIC("c3f45", WARN, arangodb::Logger::CONFIG)
            << "invalid value for --rocksdb.sync-delay-threshold. should be "
               "higher "
            << "than the value of --rocksdb.sync-interval (" << _syncInterval
            << ")";
      }

      _syncDelayThreshold = 10 * _syncInterval;
      LOG_TOPIC("c0fa3", WARN, arangodb::Logger::CONFIG)
          << "auto-adjusting value of --rocksdb.sync-delay-threshold to "
          << _syncDelayThreshold << " ms";
    }
  }

  if (_pruneWaitTimeInitial < 10) {
    LOG_TOPIC("a9667", WARN, arangodb::Logger::ENGINES)
        << "consider increasing the value for "
           "--rocksdb.wal-file-timeout-initial. "
        << "Replication clients might have trouble to get in sync";
  }
}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void RocksDBEngine::prepare() {
  // get base path from DatabaseServerFeature
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  _basePath = databasePathFeature.directory();

  TRI_ASSERT(!_basePath.empty());

#ifdef USE_ENTERPRISE
  prepareEnterprise();
#endif
}

void RocksDBEngine::verifySstFiles(rocksdb::Options const& options) const {
  TRI_ASSERT(!_path.empty());

  rocksdb::SstFileReader sstReader(options);
  for (auto const& fileName : TRI_FullTreeDirectory(_path.c_str())) {
    if (!fileName.ends_with(".sst")) {
      continue;
    }
    std::string filename = basics::FileUtils::buildFilename(_path, fileName);
    rocksdb::Status res = sstReader.Open(fileName);
    if (res.ok()) {
      res = sstReader.VerifyChecksum();
    }
    if (!res.ok()) {
      auto result = rocksutils::convertStatus(res);
      LOG_TOPIC("2943c", FATAL, arangodb::Logger::STARTUP)
          << "error when verifying .sst file '" << filename
          << "': " << result.errorMessage();
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_SST_FILE_CHECK);
    }
  }
  exit(EXIT_SUCCESS);
}

void RocksDBEngine::start() {
  // it is already decided that rocksdb is used
  TRI_ASSERT(isEnabled());
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  if (ServerState::instance()->isAgent() &&
      !server().options()->processingResult().touched(
          "rocksdb.wal-file-timeout-initial")) {
    // reduce --rocksb.wal-file-timeout-initial to 15 seconds for agency nodes
    // as we probably won't need the WAL for WAL tailing and replication here
    _pruneWaitTimeInitial = 15;
  }

  LOG_TOPIC("107fd", TRACE, arangodb::Logger::ENGINES)
      << "rocksdb version " << rest::Version::getRocksDBVersion()
      << ", supported compression types: " << getCompressionSupport();

  // set the database sub-directory for RocksDB
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  _path = databasePathFeature.subdirectoryName("engine-rocksdb");

  [[maybe_unused]] bool createdEngineDir = false;
  if (!basics::FileUtils::isDirectory(_path)) {
    std::string systemErrorStr;
    long errorNo;

    auto res =
        TRI_CreateRecursiveDirectory(_path.c_str(), errorNo, systemErrorStr);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("b2958", TRACE, arangodb::Logger::ENGINES)
          << "created RocksDB data directory '" << _path << "'";
      createdEngineDir = true;
    } else {
      LOG_TOPIC("a5ae3", FATAL, arangodb::Logger::ENGINES)
          << "unable to create RocksDB data directory '" << _path
          << "': " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }

#ifdef USE_SST_INGESTION
  _idxPath = basics::FileUtils::buildFilename(_path, "tmp-idx-creation");
  if (basics::FileUtils::isDirectory(_idxPath)) {
    for (auto const& fileName : TRI_FullTreeDirectory(_idxPath.c_str())) {
      TRI_UnlinkFile(basics::FileUtils::buildFilename(path, fileName).data());
    }
  } else {
    auto errorMsg = TRI_ERROR_NO_ERROR;
    if (!basics::FileUtils::createDirectory(_idxPath, &errorMsg)) {
      LOG_TOPIC("6d10f", FATAL, Logger::ENGINES)
          << "Cannot create tmp-idx-creation directory: " << TRI_last_error();
      FATAL_ERROR_EXIT();
    }
  }
#endif

  uint64_t totalSpace;
  uint64_t freeSpace;
  if (TRI_GetDiskSpaceInfo(_path, totalSpace, freeSpace).ok() &&
      totalSpace != 0) {
    LOG_TOPIC("b71b9", DEBUG, arangodb::Logger::ENGINES)
        << "total disk space for database directory mount: "
        << basics::StringUtils::formatSize(totalSpace)
        << ", free disk space for database directory mount: "
        << basics::StringUtils::formatSize(freeSpace) << " ("
        << (100.0 * double(freeSpace) / double(totalSpace)) << "% free)";
  }

  rocksdb::TransactionDBOptions transactionOptions =
      _optionsProvider.getTransactionDBOptions();

  // we only want to modify DBOptions here, no ColumnFamily options or the like
  _dbOptions = _optionsProvider.getOptions();
  if (_dbOptions.wal_dir.empty()) {
    _dbOptions.wal_dir = basics::FileUtils::buildFilename(_path, "journals");
  }
  LOG_TOPIC("bc82a", TRACE, arangodb::Logger::ENGINES)
      << "initializing RocksDB, path: '" << _path << "', WAL directory '"
      << _dbOptions.wal_dir << "'";

  if (_verifySst) {
    rocksdb::Options options;
#ifdef USE_ENTERPRISE
    configureEnterpriseRocksDBOptions(options, createdEngineDir);
#else
    options.env = rocksdb::Env::Default();
#endif
    verifySstFiles(options);
  }

  if (_createShaFiles) {
    _checksumEnv =
        std::make_unique<checksum::ChecksumEnv>(rocksdb::Env::Default(), _path);
    _dbOptions.env = _checksumEnv.get();
    static_cast<checksum::ChecksumEnv*>(_checksumEnv.get())
        ->getHelper()
        ->checkMissingShaFiles();  // this works even if done before
                                   // configureEnterpriseRocksDBOptions() is
                                   // called when there's encryption, because
                                   // checkMissingShafiles() only looks for
                                   // existing sst files without their sha files
                                   // in the directory and writes the missing
                                   // sha files.
  } else {
    _dbOptions.env = rocksdb::Env::Default();
  }

#ifdef USE_ENTERPRISE
  configureEnterpriseRocksDBOptions(_dbOptions, createdEngineDir);
#endif

  _dbOptions.env->SetBackgroundThreads(
      static_cast<int>(_optionsProvider.numThreadsHigh()),
      rocksdb::Env::Priority::HIGH);
  _dbOptions.env->SetBackgroundThreads(
      static_cast<int>(_optionsProvider.numThreadsLow()),
      rocksdb::Env::Priority::LOW);

  if (_debugLogging) {
    _dbOptions.info_log_level = rocksdb::InfoLogLevel::DEBUG_LEVEL;
  }

  std::shared_ptr<RocksDBLogger> logger;

  if (!_optionsProvider.useFileLogging()) {
    // if option "--rocksdb.use-file-logging" is set to false, we will use
    // our own logger that logs to ArangoDB's logfile
    logger = std::make_shared<RocksDBLogger>(_dbOptions.info_log_level);
    _dbOptions.info_log = logger;

    if (!_debugLogging) {
      logger->disable();
    }
  }

  if (_useThrottle) {
    _throttleListener = std::make_shared<RocksDBThrottle>(
        _throttleSlots, _throttleFrequency, _throttleScalingFactor,
        _throttleMaxWriteRate, _throttleSlowdownWritesTrigger,
        _throttleLowerBoundBps);
    _dbOptions.listeners.push_back(_throttleListener);
  }

  _errorListener = std::make_shared<RocksDBBackgroundErrorListener>();
  _dbOptions.listeners.push_back(_errorListener);
  _dbOptions.listeners.push_back(
      std::make_shared<RocksDBMetricsListener>(server()));

  rocksdb::BlockBasedTableOptions tableOptions =
      _optionsProvider.getTableOptions();
  // create column families
  std::vector<rocksdb::ColumnFamilyDescriptor> cfFamilies;
  auto addFamily = [this,
                    &cfFamilies](RocksDBColumnFamilyManager::Family family) {
    rocksdb::ColumnFamilyOptions specialized =
        _optionsProvider.getColumnFamilyOptions(family);
    std::string name = RocksDBColumnFamilyManager::name(family);
    cfFamilies.emplace_back(name, specialized);
  };
  // no prefix families for default column family (Has to be there)
  addFamily(RocksDBColumnFamilyManager::Family::Definitions);
  addFamily(RocksDBColumnFamilyManager::Family::Documents);
  addFamily(RocksDBColumnFamilyManager::Family::PrimaryIndex);
  addFamily(RocksDBColumnFamilyManager::Family::EdgeIndex);
  addFamily(RocksDBColumnFamilyManager::Family::VPackIndex);
  addFamily(RocksDBColumnFamilyManager::Family::GeoIndex);
  addFamily(RocksDBColumnFamilyManager::Family::FulltextIndex);
  addFamily(RocksDBColumnFamilyManager::Family::ReplicatedLogs);
  addFamily(RocksDBColumnFamilyManager::Family::ZkdIndex);

  bool dbExisted = checkExistingDB(cfFamilies);

  LOG_TOPIC("ab45b", DEBUG, Logger::STARTUP)
      << "opening RocksDB instance in '" << _path << "'";

  std::vector<rocksdb::ColumnFamilyHandle*> cfHandles;

  rocksdb::Status status = rocksdb::TransactionDB::Open(
      _dbOptions, transactionOptions, _path, cfFamilies, &cfHandles, &_db);

  if (!status.ok()) {
    std::string error;
    if (status.IsIOError()) {
      error =
          "; Maybe your filesystem doesn't provide required features? (Cifs? "
          "NFS?)";
    }

    LOG_TOPIC("fe3df", FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB engine: " << status.ToString()
        << error;
    FATAL_ERROR_EXIT();
  }
  if (cfFamilies.size() != cfHandles.size()) {
    LOG_TOPIC("ffc6d", FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB column families";
    FATAL_ERROR_EXIT();
  }
  if (cfHandles.size() <
      RocksDBColumnFamilyManager::minNumberOfColumnFamilies) {
    LOG_TOPIC("e572e", FATAL, arangodb::Logger::STARTUP)
        << "unexpected number of column families found in database. "
        << "got " << cfHandles.size() << ", expecting at least "
        << RocksDBColumnFamilyManager::minNumberOfColumnFamilies;
    FATAL_ERROR_EXIT();
  }

  // give throttle access to families
  if (_useThrottle) {
    _throttleListener->setFamilies(cfHandles);
  }

  TRI_ASSERT(_db != nullptr);

  // set our column families
  RocksDBColumnFamilyManager::set(RocksDBColumnFamilyManager::Family::Invalid,
                                  _db->DefaultColumnFamily());
  RocksDBColumnFamilyManager::set(
      RocksDBColumnFamilyManager::Family::Definitions, cfHandles[0]);
  RocksDBColumnFamilyManager::set(RocksDBColumnFamilyManager::Family::Documents,
                                  cfHandles[1]);
  RocksDBColumnFamilyManager::set(
      RocksDBColumnFamilyManager::Family::PrimaryIndex, cfHandles[2]);
  RocksDBColumnFamilyManager::set(RocksDBColumnFamilyManager::Family::EdgeIndex,
                                  cfHandles[3]);
  RocksDBColumnFamilyManager::set(
      RocksDBColumnFamilyManager::Family::VPackIndex, cfHandles[4]);
  RocksDBColumnFamilyManager::set(RocksDBColumnFamilyManager::Family::GeoIndex,
                                  cfHandles[5]);
  RocksDBColumnFamilyManager::set(
      RocksDBColumnFamilyManager::Family::FulltextIndex, cfHandles[6]);
  RocksDBColumnFamilyManager::set(
      RocksDBColumnFamilyManager::Family::ReplicatedLogs, cfHandles[7]);
  RocksDBColumnFamilyManager::set(RocksDBColumnFamilyManager::Family::ZkdIndex,
                                  cfHandles[8]);
  TRI_ASSERT(RocksDBColumnFamilyManager::get(
                 RocksDBColumnFamilyManager::Family::Definitions)
                 ->GetID() == 0);

  // will crash the process if version does not match
  arangodb::rocksdbStartupVersionCheck(server(), _db, dbExisted);

  _dbExisted = dbExisted;

  // only enable logger after RocksDB start
  if (logger != nullptr) {
    logger->enable();
  }

  if (_optionsProvider.limitOpenFilesAtStartup()) {
    _db->SetDBOptions({{"max_open_files", "-1"}});
  }

  // limit the total size of WAL files. This forces the flush of memtables of
  // column families still backed by WAL files. If we would not do this, WAL
  // files may linger around forever and will not get removed
  _db->SetDBOptions({{"max_total_wal_size",
                      std::to_string(_optionsProvider.maxTotalWalSize())}});

  {
    auto& feature = server().getFeature<FlushFeature>();
    _useReleasedTick = feature.isEnabled();
  }

  // useReleasedTick should be true on DB servers and single servers
  TRI_ASSERT((arangodb::ServerState::instance()->isCoordinator() ||
              arangodb::ServerState::instance()->isAgent()) ||
             _useReleasedTick);

  if (_syncInterval > 0) {
    _syncThread = std::make_unique<RocksDBSyncThread>(
        *this, std::chrono::milliseconds(_syncInterval),
        std::chrono::milliseconds(_syncDelayThreshold));
    if (!_syncThread->start()) {
      LOG_TOPIC("63919", FATAL, Logger::ENGINES)
          << "could not start rocksdb sync thread";
      FATAL_ERROR_EXIT();
    }
  }

  TRI_ASSERT(_db != nullptr);
  _settingsManager = std::make_unique<RocksDBSettingsManager>(*this);
  _replicationManager = std::make_unique<RocksDBReplicationManager>(*this);

  struct SchedulerExecutor : RocksDBAsyncLogWriteBatcher::IAsyncExecutor {
    explicit SchedulerExecutor(ArangodServer& server)
        : _scheduler(server.getFeature<SchedulerFeature>().SCHEDULER) {}

    void operator()(fu2::unique_function<void() noexcept> func) override {
      if (_scheduler->server().isStopping()) {
        return;
      }
      _scheduler->queue(RequestLane::CLUSTER_INTERNAL, std::move(func));
    }

    Scheduler* _scheduler;
  };

  _logPersistor = std::make_shared<RocksDBAsyncLogWriteBatcher>(
      RocksDBColumnFamilyManager::get(
          RocksDBColumnFamilyManager::Family::ReplicatedLogs),
      _db->GetRootDB(), std::make_shared<SchedulerExecutor>(server()),
      server().getFeature<ReplicatedLogFeature>().options());

  _settingsManager->retrieveInitialValues();

  double const counterSyncSeconds = 2.5;
  _backgroundThread =
      std::make_unique<RocksDBBackgroundThread>(*this, counterSyncSeconds);
  if (!_backgroundThread->start()) {
    LOG_TOPIC("a5e96", FATAL, Logger::ENGINES)
        << "could not start rocksdb counter manager thread";
    FATAL_ERROR_EXIT();
  }

  if (!systemDatabaseExists()) {
    addSystemDatabase();
  }

  if (!useEdgeCache()) {
    LOG_TOPIC("46557", INFO, Logger::ENGINES)
        << "in-memory cache for edges is disabled";
  }

  // to populate initial health check data
  HealthData hd = healthCheck();
  if (hd.res.fail()) {
    LOG_TOPIC("4cf5b", ERR, Logger::ENGINES) << hd.res.errorMessage();
  }
}

void RocksDBEngine::beginShutdown() {
  TRI_ASSERT(isEnabled());

  // block the creation of new replication contexts
  if (_replicationManager != nullptr) {
    _replicationManager->beginShutdown();
  }

  // from now on, all started compactions can be canceled.
  // note that this is only a best-effort hint to RocksDB and
  // may not be followed immediately.
  ::cancelCompactions.store(true, std::memory_order_release);
}

void RocksDBEngine::stop() {
  TRI_ASSERT(isEnabled());

  // in case we missed the beginShutdown somehow, call it again
  replicationManager()->beginShutdown();
  replicationManager()->dropAll();

  if (_backgroundThread) {
    // stop the press
    _backgroundThread->beginShutdown();

    if (_settingsManager) {
      auto syncRes = _settingsManager->sync(/*force*/ true);
      if (syncRes.fail()) {
        LOG_TOPIC("0582f", WARN, Logger::ENGINES)
            << "caught exception while shutting down RocksDB engine: "
            << syncRes.errorMessage();
      }
    }

    // wait until background thread stops
    while (_backgroundThread->isRunning()) {
      std::this_thread::yield();
    }
    _backgroundThread.reset();
  }

  if (_syncThread) {
    // _syncThread may be a nullptr, in case automatic syncing is turned off
    _syncThread->beginShutdown();

    // wait until sync thread stops
    while (_syncThread->isRunning()) {
      std::this_thread::yield();
    }
    _syncThread.reset();
  }

  waitForCompactionJobsToFinish();
}

void RocksDBEngine::unprepare() {
  TRI_ASSERT(isEnabled());
  waitForCompactionJobsToFinish();
  shutdownRocksDBInstance();
}

void RocksDBEngine::trackRevisionTreeHibernation() noexcept {
  ++_metricsTreeHibernations;
}

void RocksDBEngine::trackRevisionTreeResurrection() noexcept {
  ++_metricsTreeResurrections;
}

void RocksDBEngine::trackRevisionTreeMemoryIncrease(
    std::uint64_t value) noexcept {
  if (value != 0) {
    _metricsTreeMemoryUsage += value;
  }
}

void RocksDBEngine::trackRevisionTreeMemoryDecrease(
    std::uint64_t value) noexcept {
  if (value != 0) {
    [[maybe_unused]] auto old = _metricsTreeMemoryUsage.fetch_sub(value);
    TRI_ASSERT(old >= value);
  }
}

bool RocksDBEngine::hasBackgroundError() const {
  return _errorListener != nullptr && _errorListener->called();
}

std::unique_ptr<transaction::Manager> RocksDBEngine::createTransactionManager(
    transaction::ManagerFeature& feature) {
  return std::make_unique<transaction::Manager>(feature);
}

std::shared_ptr<TransactionState> RocksDBEngine::createTransactionState(
    TRI_vocbase_t& vocbase, TransactionId tid,
    transaction::Options const& options) {
  if (vocbase.replicationVersion() == replication::Version::TWO &&
      (tid.isLeaderTransactionId() || tid.isLegacyTransactionId()) &&
      ServerState::instance()->isRunningInCluster() &&
      !options.allowDirtyReads && options.requiresReplication) {
    return std::make_shared<ReplicatedRocksDBTransactionState>(vocbase, tid,
                                                               options);
  }
  return std::make_shared<SimpleRocksDBTransactionState>(vocbase, tid, options);
}

void RocksDBEngine::addParametersForNewCollection(VPackBuilder& builder,
                                                  VPackSlice info) {
  if (!info.hasKey(StaticStrings::ObjectId)) {
    builder.add(StaticStrings::ObjectId,
                VPackValue(std::to_string(TRI_NewTickServer())));
  }
  if (!info.get(StaticStrings::CacheEnabled).isBool()) {
    builder.add(StaticStrings::CacheEnabled, VPackValue(false));
  }
}

// create storage-engine specific collection
std::unique_ptr<PhysicalCollection> RocksDBEngine::createPhysicalCollection(
    LogicalCollection& collection, velocypack::Slice info) {
  return std::make_unique<RocksDBCollection>(collection, info);
}

// inventory functionality
// -----------------------

void RocksDBEngine::getDatabases(arangodb::velocypack::Builder& result) {
  LOG_TOPIC("a9cc7", TRACE, Logger::STARTUP) << "getting existing databases";

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(
      readOptions, RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Definitions)));
  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::Database);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    auto slice =
        VPackSlice(reinterpret_cast<uint8_t const*>(iter->value().data()));

    //// check format id
    TRI_ASSERT(slice.isObject());
    VPackSlice idSlice = slice.get(StaticStrings::DatabaseId);
    if (!idSlice.isString()) {
      LOG_TOPIC("099d7", ERR, arangodb::Logger::STARTUP)
          << "found invalid database declaration with non-string id: "
          << slice.toJson();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    // deleted
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "deleted",
                                                            false)) {
      TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
          basics::StringUtils::uint64(idSlice.copyString()));

      // database is deleted, skip it!
      LOG_TOPIC("43cbc", DEBUG, arangodb::Logger::STARTUP)
          << "found dropped database " << id;

      dropDatabase(id);
      continue;
    }

    // name
    VPackSlice nameSlice = slice.get("name");
    if (!nameSlice.isString()) {
      LOG_TOPIC("96ffc", ERR, arangodb::Logger::STARTUP)
          << "found invalid database declaration with non-string name: "
          << slice.toJson();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    result.add(slice);
  }
  result.close();
}

void RocksDBEngine::getCollectionInfo(TRI_vocbase_t& vocbase, DataSourceId cid,
                                      arangodb::velocypack::Builder& builder,
                                      bool includeIndexes,
                                      TRI_voc_tick_t maxTick) {
  builder.openObject();

  // read collection info from database
  RocksDBKey key;

  key.constructCollection(vocbase.id(), cid);

  rocksdb::PinnableSlice value;
  rocksdb::ReadOptions options;
  rocksdb::Status res =
      _db->Get(options,
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::Definitions),
               key.string(), &value);
  auto result = rocksutils::convertStatus(res);

  if (result.errorNumber() != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(result);
  }

  VPackSlice fullParameters = RocksDBValue::data(value);

  builder.add("parameters", fullParameters);

  if (includeIndexes) {
    // dump index information
    VPackSlice indexes = fullParameters.get("indexes");
    builder.add(VPackValue("indexes"));
    builder.openArray();

    if (indexes.isArray()) {
      for (auto const idx : VPackArrayIterator(indexes)) {
        // This is only allowed to contain user-defined indexes.
        // So we have to exclude Primary + Edge Types
        auto type = idx.get(StaticStrings::IndexType);
        TRI_ASSERT(type.isString());

        if (!type.isEqualString("primary") && !type.isEqualString("edge")) {
          builder.add(idx);
        }
      }
    }

    builder.close();
  }

  builder.close();
}

ErrorCode RocksDBEngine::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result,
    bool wasCleanShutdown, bool isUpgrade) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(
      readOptions, RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Definitions)));

  result.openArray();

  auto rSlice = rocksDBSlice(RocksDBEntryType::Collection);

  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase.id() != RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice =
        VPackSlice(reinterpret_cast<uint8_t const*>(iter->value().data()));

    if (arangodb::basics::VelocyPackHelper::getBooleanValue(
            slice, StaticStrings::DataSourceDeleted, false)) {
      continue;
    }

    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

ErrorCode RocksDBEngine::getViews(TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Builder& result) {
  auto bounds = RocksDBKeyBounds::DatabaseViews(vocbase.id());
  rocksdb::Slice upper = bounds.end();
  rocksdb::ColumnFamilyHandle* cf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Definitions);

  rocksdb::ReadOptions ro;
  ro.iterate_upper_bound = &upper;

  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(ro, cf));
  result.openArray();
  for (iter->Seek(bounds.start()); iter->Valid(); iter->Next()) {
    TRI_ASSERT(iter->key().compare(bounds.end()) < 0);
    auto slice =
        VPackSlice(reinterpret_cast<uint8_t const*>(iter->value().data()));

    LOG_TOPIC("e3bcd", TRACE, Logger::VIEWS)
        << "got view slice: " << slice.toJson();

    if (arangodb::basics::VelocyPackHelper::getBooleanValue(
            slice, StaticStrings::DataSourceDeleted, false)) {
      continue;
    }
    if (ServerState::instance()->isDBServer() &&
        arangodb::basics::VelocyPackHelper::getStringView(
            slice, StaticStrings::DataSourceType, {}) !=
            arangodb::iresearch::StaticStrings::ViewArangoSearchType) {
      continue;
    }
    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

std::string RocksDBEngine::versionFilename(TRI_voc_tick_t id) const {
  return _basePath + TRI_DIR_SEPARATOR_CHAR + "VERSION-" + std::to_string(id);
}

void RocksDBEngine::cleanupReplicationContexts() {
  if (_replicationManager != nullptr) {
    _replicationManager->dropAll();
  }
}

VPackBuilder RocksDBEngine::getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase, ErrorCode& status) {
  RocksDBKey key;

  key.constructReplicationApplierConfig(vocbase.id());

  return getReplicationApplierConfiguration(key, status);
}

VPackBuilder RocksDBEngine::getReplicationApplierConfiguration(
    ErrorCode& status) {
  RocksDBKey key;
  key.constructReplicationApplierConfig(databaseIdForGlobalApplier);
  return getReplicationApplierConfiguration(key, status);
}

VPackBuilder RocksDBEngine::getReplicationApplierConfiguration(
    RocksDBKey const& key, ErrorCode& status) {
  rocksdb::PinnableSlice value;

  auto opts = rocksdb::ReadOptions();
  auto s = _db->Get(opts,
                    RocksDBColumnFamilyManager::get(
                        RocksDBColumnFamilyManager::Family::Definitions),
                    key.string(), &value);
  if (!s.ok()) {
    status = TRI_ERROR_FILE_NOT_FOUND;
    return arangodb::velocypack::Builder();
  }

  status = TRI_ERROR_NO_ERROR;
  VPackBuilder builder;
  builder.add(RocksDBValue::data(value));
  return builder;
}

ErrorCode RocksDBEngine::removeReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase) {
  RocksDBKey key;

  key.constructReplicationApplierConfig(vocbase.id());

  return removeReplicationApplierConfiguration(key);
}

ErrorCode RocksDBEngine::removeReplicationApplierConfiguration() {
  RocksDBKey key;
  key.constructReplicationApplierConfig(databaseIdForGlobalApplier);
  return removeReplicationApplierConfiguration(key);
}

ErrorCode RocksDBEngine::removeReplicationApplierConfiguration(
    RocksDBKey const& key) {
  auto status = rocksutils::convertStatus(
      _db->Delete(rocksdb::WriteOptions(),
                  RocksDBColumnFamilyManager::get(
                      RocksDBColumnFamilyManager::Family::Definitions),
                  key.string()));
  if (!status.ok()) {
    return status.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

ErrorCode RocksDBEngine::saveReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase, velocypack::Slice slice, bool doSync) {
  RocksDBKey key;

  key.constructReplicationApplierConfig(vocbase.id());

  return saveReplicationApplierConfiguration(key, slice, doSync);
}

ErrorCode RocksDBEngine::saveReplicationApplierConfiguration(
    velocypack::Slice slice, bool doSync) {
  RocksDBKey key;
  key.constructReplicationApplierConfig(databaseIdForGlobalApplier);
  return saveReplicationApplierConfiguration(key, slice, doSync);
}

ErrorCode RocksDBEngine::saveReplicationApplierConfiguration(
    RocksDBKey const& key, velocypack::Slice slice, bool doSync) {
  auto value = RocksDBValue::ReplicationApplierConfig(slice);

  auto status = rocksutils::convertStatus(
      _db->Put(rocksdb::WriteOptions(),
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::Definitions),
               key.string(), value.string()));
  if (!status.ok()) {
    return status.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

// database, collection and index management
// -----------------------------------------

std::unique_ptr<TRI_vocbase_t> RocksDBEngine::openDatabase(
    arangodb::CreateDatabaseInfo&& info, bool isUpgrade) {
  return openExistingDatabase(std::move(info), true, isUpgrade);
}

Result RocksDBEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                                velocypack::Slice slice) {
  return writeDatabaseMarker(id, slice, RocksDBLogValue::DatabaseCreate(id));
}

Result RocksDBEngine::writeDatabaseMarker(TRI_voc_tick_t id,
                                          velocypack::Slice slice,
                                          RocksDBLogValue&& logValue) {
  RocksDBKey key;
  key.constructDatabase(id);
  auto value = RocksDBValue::Database(slice);
  rocksdb::WriteOptions wo;

  // Write marker + key into RocksDB inside one batch
  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Put(RocksDBColumnFamilyManager::get(
                RocksDBColumnFamilyManager::Family::Definitions),
            key.string(), value.string());
  rocksdb::Status res = _db->GetRootDB()->Write(wo, &batch);
  return rocksutils::convertStatus(res);
}

Result RocksDBEngine::writeCreateCollectionMarker(TRI_voc_tick_t databaseId,
                                                  DataSourceId cid,
                                                  velocypack::Slice slice,
                                                  RocksDBLogValue&& logValue) {
  rocksdb::DB* db = _db->GetRootDB();

  RocksDBKey key;
  key.constructCollection(databaseId, cid);
  auto value = RocksDBValue::Collection(slice);

  rocksdb::WriteOptions wo;
  // Write marker + key into RocksDB inside one batch
  rocksdb::WriteBatch batch;
  if (logValue.slice().size() > 0) {
    batch.PutLogData(logValue.slice());
  }
  batch.Put(RocksDBColumnFamilyManager::get(
                RocksDBColumnFamilyManager::Family::Definitions),
            key.string(), value.string());
  rocksdb::Status res = db->Write(wo, &batch);

  return rocksutils::convertStatus(res);
}

Result RocksDBEngine::prepareDropDatabase(TRI_vocbase_t& vocbase) {
  VPackBuilder builder;

  builder.openObject();
  builder.add("id", velocypack::Value(std::to_string(vocbase.id())));
  builder.add("name", velocypack::Value(vocbase.name()));
  builder.add("deleted", VPackValue(true));
  builder.close();

  auto log = RocksDBLogValue::DatabaseDrop(vocbase.id());
  return writeDatabaseMarker(vocbase.id(), builder.slice(), std::move(log));
}

Result RocksDBEngine::dropDatabase(TRI_vocbase_t& database) {
  replicationManager()->drop(database);

  return dropDatabase(database.id());
}

// current recovery state
RecoveryState RocksDBEngine::recoveryState() noexcept {
  return server().getFeature<RocksDBRecoveryManager>().recoveryState();
}

// current recovery tick
TRI_voc_tick_t RocksDBEngine::recoveryTick() noexcept {
  return server().getFeature<RocksDBRecoveryManager>().recoverySequenceNumber();
}

void RocksDBEngine::scheduleTreeRebuild(TRI_voc_tick_t database,
                                        std::string const& collection) {
  MUTEX_LOCKER(locker, _rebuildCollectionsLock);
  _rebuildCollections.emplace(std::make_pair(database, collection),
                              /*started*/ false);
}

void RocksDBEngine::processTreeRebuilds() {
  Scheduler* scheduler = arangodb::SchedulerFeature::SCHEDULER;
  if (scheduler == nullptr) {
    return;
  }

  uint64_t maxParallelRebuilds = 2;
  uint64_t iterations = 0;
  while (++iterations <= maxParallelRebuilds) {
    if (server().isStopping()) {
      // don't fire off more tree rebuilds while we are shutting down
      return;
    }

    std::pair<TRI_voc_tick_t, std::string> candidate{};

    {
      MUTEX_LOCKER(locker, _rebuildCollectionsLock);
      if (_rebuildCollections.empty() ||
          _runningRebuilds >= maxParallelRebuilds) {
        // nothing to do, or too much to do
        return;
      }

      for (auto& it : _rebuildCollections) {
        if (!it.second) {
          // set to started
          it.second = true;
          candidate = it.first;
          ++_runningRebuilds;
          break;
        }
      }
    }

    if (candidate.first == 0 || candidate.second.empty()) {
      return;
    }

    if (server().isStopping()) {
      return;
    }

    scheduler->queue(arangodb::RequestLane::CLIENT_SLOW, [this, candidate]() {
      if (!server().isStopping()) {
        VocbasePtr vocbase;
        try {
          auto& df = server().getFeature<DatabaseFeature>();
          vocbase = df.useDatabase(candidate.first);
          if (vocbase != nullptr) {
            auto collection = vocbase->lookupCollectionByUuid(candidate.second);
            if (collection != nullptr && !collection->deleted()) {
              LOG_TOPIC("b96bc", INFO, Logger::ENGINES)
                  << "starting background rebuild of revision tree for "
                     "collection "
                  << candidate.first << "/" << collection->name();
              Result res =
                  static_cast<RocksDBCollection*>(collection->getPhysical())
                      ->rebuildRevisionTree();
              if (res.ok()) {
                ++_metricsTreeRebuildsSuccess;
                LOG_TOPIC("2f997", INFO, Logger::ENGINES)
                    << "successfully rebuilt revision tree for collection "
                    << candidate.first << "/" << collection->name();
              } else {
                ++_metricsTreeRebuildsFailure;
                if (res.is(TRI_ERROR_LOCK_TIMEOUT)) {
                  LOG_TOPIC("bce3a", WARN, Logger::ENGINES)
                      << "failure during revision tree rebuilding for "
                         "collection "
                      << candidate.first << "/" << collection->name() << ": "
                      << res.errorMessage();
                } else {
                  LOG_TOPIC("a1fc2", ERR, Logger::ENGINES)
                      << "failure during revision tree rebuilding for "
                         "collection "
                      << candidate.first << "/" << collection->name() << ": "
                      << res.errorMessage();
                }
                {
                  // mark as to-be-done again
                  MUTEX_LOCKER(locker, _rebuildCollectionsLock);
                  auto it = _rebuildCollections.find(candidate);
                  if (it != _rebuildCollections.end()) {
                    (*it).second = false;
                  }
                }
                // rethrow exception
                THROW_ARANGO_EXCEPTION(res);
              }
            }
          }

          // tree rebuilding finished successfully. now remove from the list
          // to-be-rebuilt candidates
          MUTEX_LOCKER(locker, _rebuildCollectionsLock);
          _rebuildCollections.erase(candidate);

        } catch (std::exception const& ex) {
          LOG_TOPIC("13afc", WARN, Logger::ENGINES)
              << "caught exception during tree rebuilding: " << ex.what();
        } catch (...) {
          LOG_TOPIC("0bcbf", WARN, Logger::ENGINES)
              << "caught unknown exception during tree rebuilding";
        }
      }

      // always count down _runningRebuilds!
      MUTEX_LOCKER(locker, _rebuildCollectionsLock);
      TRI_ASSERT(_runningRebuilds > 0);
      --_runningRebuilds;
    });
  }
}

void RocksDBEngine::compactRange(RocksDBKeyBounds bounds) {
  {
    WRITE_LOCKER(locker, _pendingCompactionsLock);
    _pendingCompactions.push_back(std::move(bounds));
  }

  // directly kick off compactions if there is enough processing
  // capacity
  processCompactions();
}

void RocksDBEngine::processCompactions() {
  Scheduler* scheduler = arangodb::SchedulerFeature::SCHEDULER;
  if (scheduler == nullptr) {
    return;
  }

  uint64_t maxIterations = _maxParallelCompactions;
  uint64_t iterations = 0;
  while (++iterations <= maxIterations) {
    if (server().isStopping()) {
      // don't fire off more compactions while we are shutting down
      return;
    }

    RocksDBKeyBounds bounds = RocksDBKeyBounds::Empty();
    {
      WRITE_LOCKER(locker, _pendingCompactionsLock);
      if (_pendingCompactions.empty() ||
          _runningCompactions >= _maxParallelCompactions) {
        // nothing to do, or too much to do
        LOG_TOPIC("d5108", TRACE, Logger::ENGINES)
            << "checking compactions. pending: " << _pendingCompactions.size()
            << ", running: " << _runningCompactions;
        return;
      }
      // found something to do, now steal the item from the queue
      bounds = std::move(_pendingCompactions.front());
      _pendingCompactions.pop_front();

      if (server().isStopping()) {
        // if we are stopping, it is ok to not process but lose any pending
        // compactions
        return;
      }

      // set it to running already, so that concurrent callers of this method
      // will not kick off additional jobs
      ++_runningCompactions;
    }

    LOG_TOPIC("6ea1b", TRACE, Logger::ENGINES)
        << "scheduling compaction for execution";

    scheduler->queue(arangodb::RequestLane::CLIENT_SLOW, [this, bounds]() {
      if (server().isStopping()) {
        LOG_TOPIC("3d619", TRACE, Logger::ENGINES)
            << "aborting pending compaction due to server shutdown";
      } else {
        LOG_TOPIC("9485b", TRACE, Logger::ENGINES)
            << "executing compaction for range " << bounds;
        double start = TRI_microtime();
        try {
          rocksdb::CompactRangeOptions opts;
          opts.exclusive_manual_compaction = false;
          opts.allow_write_stall = true;
          opts.canceled = &::cancelCompactions;
          rocksdb::Slice b = bounds.start(), e = bounds.end();
          _db->CompactRange(opts, bounds.columnFamily(), &b, &e);
        } catch (std::exception const& ex) {
          LOG_TOPIC("a4c42", WARN, Logger::ENGINES)
              << "compaction for range " << bounds
              << " failed with error: " << ex.what();
        } catch (...) {
          // whatever happens, we need to count down _runningCompactions in all
          // cases
        }

        LOG_TOPIC("79591", TRACE, Logger::ENGINES)
            << "finished compaction for range " << bounds
            << ", took: " << Logger::FIXED(TRI_microtime() - start);
      }
      // always count down _runningCompactions!
      WRITE_LOCKER(locker, _pendingCompactionsLock);
      TRI_ASSERT(_runningCompactions > 0);
      --_runningCompactions;
    });
  }
}

void RocksDBEngine::createCollection(TRI_vocbase_t& vocbase,
                                     LogicalCollection const& collection) {
  DataSourceId const cid = collection.id();
  TRI_ASSERT(cid.isSet());

  auto builder = collection.toVelocyPackIgnore(
      {"path", "statusString"},
      LogicalDataSource::Serialization::PersistenceWithInProgress);
  TRI_UpdateTickServer(cid.id());

  Result res = writeCreateCollectionMarker(
      vocbase.id(), cid, builder.slice(),
      RocksDBLogValue::CollectionCreate(vocbase.id(), cid));

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void RocksDBEngine::prepareDropCollection(TRI_vocbase_t& /*vocbase*/,
                                          LogicalCollection& coll) {
  replicationManager()->drop(coll);
}

arangodb::Result RocksDBEngine::dropCollection(TRI_vocbase_t& vocbase,
                                               LogicalCollection& coll) {
  auto* rcoll = static_cast<RocksDBMetaCollection*>(coll.getPhysical());
  bool const prefixSameAsStart = true;
  bool const useRangeDelete = rcoll->meta().numberDocuments() >= 32 * 1024;

  auto resLock = rcoll->lockWrite();  // technically not necessary
  if (resLock != TRI_ERROR_NO_ERROR) {
    return resLock;
  }

  rocksdb::DB* db = _db->GetRootDB();

  // If we get here the collection is safe to drop.
  //
  // This uses the following workflow:
  // 1. Persist the drop.
  //   * if this fails the collection will remain!
  //   * if this succeeds the collection is gone from user point
  // 2. Drop all Documents
  //   * If this fails we give up => We have data-garbage in RocksDB, Collection
  //   is gone.
  // 3. Drop all Indexes
  //   * If this fails we give up => We have data-garbage in RocksDB, Collection
  //   is gone.
  // 4. If all succeeds we do not have data-garbage, all is gone.
  //
  // (NOTE: The above fails can only occur on full HDD or Machine dying. No
  // write conflicts possible)

  TRI_ASSERT(coll.deleted());

  // Prepare collection remove batch
  rocksdb::WriteBatch batch;
  RocksDBLogValue logValue =
      RocksDBLogValue::CollectionDrop(vocbase.id(), coll.id(), coll.guid());
  batch.PutLogData(logValue.slice());

  RocksDBKey key;
  key.constructCollection(vocbase.id(), coll.id());
  batch.Delete(RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::Definitions),
               key.string());

  rocksdb::WriteOptions wo;
  rocksdb::Status s = db->Write(wo, &batch);

  // TODO FAILURE Simulate !res.ok()
  if (!s.ok()) {
    // Persisting the drop failed. Do NOT drop collection.
    return rocksutils::convertStatus(s);
  }

  // Now Collection is gone.
  // Cleanup data-mess

  // Unregister collection metadata
  Result res = RocksDBMetadata::deleteCollectionMeta(db, rcoll->objectId());
  if (res.fail()) {
    LOG_TOPIC("2c890", ERR, Logger::ENGINES)
        << "error removing collection meta-data: "
        << res.errorMessage();  // continue regardless
  }

  // remove from map
  {
    WRITE_LOCKER(guard, _mapLock);
    _collectionMap.erase(rcoll->objectId());
  }

  // delete indexes, RocksDBIndex::drop() has its own check
  std::vector<std::shared_ptr<Index>> vecShardIndex = rcoll->getIndexes();
  TRI_ASSERT(!vecShardIndex.empty());

  for (auto& index : vecShardIndex) {
    RocksDBIndex* ridx = static_cast<RocksDBIndex*>(index.get());
    res = RocksDBMetadata::deleteIndexEstimate(db, ridx->objectId());
    if (res.fail()) {
      LOG_TOPIC("f2d51", WARN, Logger::ENGINES)
          << "could not delete index estimate: " << res.errorMessage();
    }

    auto dropRes = index->drop().errorNumber();

    if (dropRes != TRI_ERROR_NO_ERROR) {
      // We try to remove all indexed values.
      // If it does not work they cannot be accessed any more and leaked.
      // User View remains consistent.
      LOG_TOPIC("97176", ERR, Logger::ENGINES)
          << "unable to drop index: " << TRI_errno_string(dropRes);
      //      return TRI_ERROR_NO_ERROR;
    }
  }

  // delete documents
  RocksDBKeyBounds bounds =
      RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  auto result = rocksutils::removeLargeRange(db, bounds, prefixSameAsStart,
                                             useRangeDelete);

  if (result.fail()) {
    // We try to remove all documents.
    // If it does not work they cannot be accessed any more and leaked.
    // User View remains consistent.
    return TRI_ERROR_NO_ERROR;
  }

  // run compaction for data only if collection contained a considerable
  // amount of documents. otherwise don't run compaction, because it will
  // slow things down a lot, especially during tests that create/drop LOTS
  // of collections
  if (useRangeDelete) {
    rcoll->compact();
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // check if documents have been deleted
  size_t numDocs = rocksutils::countKeyRange(_db, bounds, nullptr, true);

  if (numDocs > 0) {
    std::string errorMsg(
        "deletion check in collection drop failed - not all documents "
        "have been deleted. remaining: ");
    errorMsg.append(std::to_string(numDocs));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif

  // if we get here all documents / indexes are gone.
  // We have no data garbage left.
  return Result();
}

void RocksDBEngine::changeCollection(TRI_vocbase_t& vocbase,
                                     LogicalCollection const& collection) {
  auto builder = collection.toVelocyPackIgnore(
      {"path", "statusString"},
      LogicalDataSource::Serialization::PersistenceWithInProgress);
  Result res = writeCreateCollectionMarker(
      vocbase.id(), collection.id(), builder.slice(),
      RocksDBLogValue::CollectionChange(vocbase.id(), collection.id()));

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

arangodb::Result RocksDBEngine::renameCollection(
    TRI_vocbase_t& vocbase, LogicalCollection const& collection,
    std::string const& oldName) {
  auto builder = collection.toVelocyPackIgnore(
      {"path", "statusString"},
      LogicalDataSource::Serialization::PersistenceWithInProgress);
  Result res = writeCreateCollectionMarker(
      vocbase.id(), collection.id(), builder.slice(),
      RocksDBLogValue::CollectionRename(vocbase.id(), collection.id(),
                                        oldName));

  return res;
}

Result RocksDBEngine::createView(TRI_vocbase_t& vocbase, DataSourceId id,
                                 arangodb::LogicalView const& view) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC("0bad8", DEBUG, Logger::ENGINES) << "RocksDBEngine::createView";
#endif
  rocksdb::WriteBatch batch;
  rocksdb::WriteOptions wo;

  RocksDBKey key;
  key.constructView(vocbase.id(), id);
  RocksDBLogValue logValue = RocksDBLogValue::ViewCreate(vocbase.id(), id);

  VPackBuilder props;

  props.openObject();
  view.properties(props,
                  LogicalDataSource::Serialization::PersistenceWithInProgress);
  props.close();

  RocksDBValue const value = RocksDBValue::View(props.slice());

  // Write marker + key into RocksDB inside one batch
  batch.PutLogData(logValue.slice());
  batch.Put(RocksDBColumnFamilyManager::get(
                RocksDBColumnFamilyManager::Family::Definitions),
            key.string(), value.string());

  auto res = _db->Write(wo, &batch);

  LOG_TOPIC_IF("cac6a", TRACE, Logger::VIEWS, !res.ok())
      << "could not create view: " << res.ToString();

  return rocksutils::convertStatus(res);
}

arangodb::Result RocksDBEngine::dropView(TRI_vocbase_t const& vocbase,
                                         LogicalView const& view) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC("fa6e5", DEBUG, Logger::ENGINES) << "RocksDBEngine::dropView";
#endif
  auto logValue =
      RocksDBLogValue::ViewDrop(vocbase.id(), view.id(), view.guid());

  RocksDBKey key;
  key.constructView(vocbase.id(), view.id());

  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Delete(RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::Definitions),
               key.string());

  rocksdb::WriteOptions wo;
  auto res = _db->GetRootDB()->Write(wo, &batch);
  LOG_TOPIC_IF("fcd22", TRACE, Logger::VIEWS, !res.ok())
      << "could not create view: " << res.ToString();
  return rocksutils::convertStatus(res);
}

Result RocksDBEngine::changeView(LogicalView const& view,
                                 velocypack::Slice update) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC("405da", DEBUG, Logger::ENGINES) << "RocksDBEngine::changeView";
#endif
  if (inRecovery()) {
    // nothing to do
    return {};
  }
  auto& vocbase = view.vocbase();

  RocksDBKey key;
  key.constructView(vocbase.id(), view.id());

  RocksDBLogValue log = RocksDBLogValue::ViewChange(vocbase.id(), view.id());
  RocksDBValue const value = RocksDBValue::View(update);

  rocksdb::WriteBatch batch;
  rocksdb::WriteOptions wo;  // TODO: check which options would make sense
  rocksdb::Status s;

  s = batch.PutLogData(log.slice());

  if (!s.ok()) {
    LOG_TOPIC("6d6a4", TRACE, Logger::VIEWS)
        << "failed to write change view marker " << s.ToString();
    return rocksutils::convertStatus(s);
  }

  s = batch.Put(RocksDBColumnFamilyManager::get(
                    RocksDBColumnFamilyManager::Family::Definitions),
                key.string(), value.string());

  if (!s.ok()) {
    LOG_TOPIC("ebb58", TRACE, Logger::VIEWS)
        << "failed to write change view marker " << s.ToString();
    return rocksutils::convertStatus(s);
  }
  auto res = _db->Write(wo, &batch);
  LOG_TOPIC_IF("6ee8a", TRACE, Logger::VIEWS, !res.ok())
      << "could not change view: " << res.ToString();
  return rocksutils::convertStatus(res);
}

Result RocksDBEngine::compactAll(bool changeLevel,
                                 bool compactBottomMostLevel) {
  return rocksutils::compactAll(_db->GetRootDB(), changeLevel,
                                compactBottomMostLevel, &::cancelCompactions);
}

/// @brief Add engine-specific optimizer rules
void RocksDBEngine::addOptimizerRules(aql::OptimizerRulesFeature& feature) {
  RocksDBOptimizerRules::registerResources(feature);
}

/// @brief Add engine-specific V8 functions
void RocksDBEngine::addV8Functions() {
  // there are no specific V8 functions here
  RocksDBV8Functions::registerResources(*this);
}

/// @brief Add engine-specific REST handlers
void RocksDBEngine::addRestHandlers(rest::RestHandlerFactory& handlerFactory) {
  RocksDBRestHandlers::registerResources(&handlerFactory);
}

void RocksDBEngine::addCollectionMapping(uint64_t objectId, TRI_voc_tick_t did,
                                         DataSourceId cid) {
  if (objectId != 0) {
    WRITE_LOCKER(guard, _mapLock);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto it = _collectionMap.find(objectId);
    if (it != _collectionMap.end()) {
      if (it->second.first != did || it->second.second != cid) {
        LOG_TOPIC("80e81", ERR, Logger::FIXME)
            << "trying to add objectId: " << objectId << ", did: " << did
            << ", cid: " << cid.id()
            << ", found in map: did: " << it->second.first
            << ", cid: " << it->second.second.id() << ", map contains "
            << _collectionMap.size() << " entries";
        for (auto const& it : _collectionMap) {
          LOG_TOPIC("77de9", ERR, Logger::FIXME)
              << "- objectId: " << it.first << " => (did: " << it.second.first
              << ", cid: " << it.second.second.id() << ")";
        }
      }
      TRI_ASSERT(it->second.first == did);
      TRI_ASSERT(it->second.second == cid);
    }
#endif
    _collectionMap[objectId] = std::make_pair(did, cid);
  }
}

std::vector<std::pair<TRI_voc_tick_t, DataSourceId>>
RocksDBEngine::collectionMappings() const {
  std::vector<std::pair<TRI_voc_tick_t, DataSourceId>> res;
  READ_LOCKER(guard, _mapLock);
  for (auto const& it : _collectionMap) {
    res.emplace_back(it.second.first, it.second.second);
  }
  return res;
}

void RocksDBEngine::addIndexMapping(uint64_t objectId, TRI_voc_tick_t did,
                                    DataSourceId cid, IndexId iid) {
  if (objectId != 0) {
    WRITE_LOCKER(guard, _mapLock);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto it = _indexMap.find(objectId);
    if (it != _indexMap.end()) {
      TRI_ASSERT(std::get<0>(it->second) == did);
      TRI_ASSERT(std::get<1>(it->second) == cid);
      TRI_ASSERT(std::get<2>(it->second) == iid);
    }
#endif
    _indexMap[objectId] = std::make_tuple(did, cid, iid);
  }
}

void RocksDBEngine::removeIndexMapping(uint64_t objectId) {
  if (objectId != 0) {
    WRITE_LOCKER(guard, _mapLock);
    _indexMap.erase(objectId);
  }
}

RocksDBEngine::CollectionPair RocksDBEngine::mapObjectToCollection(
    uint64_t objectId) const {
  READ_LOCKER(guard, _mapLock);
  auto it = _collectionMap.find(objectId);
  if (it == _collectionMap.end()) {
    return {0, DataSourceId::none()};
  }
  return it->second;
}

RocksDBEngine::IndexTriple RocksDBEngine::mapObjectToIndex(
    uint64_t objectId) const {
  READ_LOCKER(guard, _mapLock);
  auto it = _indexMap.find(objectId);
  if (it == _indexMap.end()) {
    return RocksDBEngine::IndexTriple(0, 0, 0);
  }
  return it->second;
}

/// @brief return a list of the currently open WAL files
std::vector<std::string> RocksDBEngine::currentWalFiles() const {
  rocksdb::VectorLogPtr files;
  std::vector<std::string> names;

  auto status = _db->GetSortedWalFiles(files);
  if (!status.ok()) {
    return names;  // TODO: error here?
  }

  for (size_t current = 0; current < files.size(); current++) {
    auto f = files[current].get();
    try {
      names.push_back(f->PathName());
    } catch (...) {
      return names;
    }
  }

  return names;
}

/// @brief flushes the RocksDB WAL.
/// the optional parameter "waitForSync" is currently only used when the
/// "flushColumnFamilies" parameter is also set to true. If
/// "flushColumnFamilies" is true, all the RocksDB column family memtables are
/// flushed, and, if "waitForSync" is set, additionally synced to disk. The only
/// call site that uses "flushColumnFamilies" currently is hot backup. The
/// function parameter name are a remainder from MMFiles times, when they made
/// more sense. This can be refactored at any point, so that flushing column
/// families becomes a separate API.
Result RocksDBEngine::flushWal(bool waitForSync, bool flushColumnFamilies) {
  Result res;

  if (_syncThread) {
    // _syncThread may be a nullptr, in case automatic syncing is turned off
    res = _syncThread->syncWal();
  } else {
    // no syncThread...
    res = RocksDBSyncThread::sync(_db->GetBaseDB());
  }

  if (res.ok() && flushColumnFamilies) {
    rocksdb::FlushOptions flushOptions;
    flushOptions.wait = waitForSync;

    for (auto cf : RocksDBColumnFamilyManager::allHandles()) {
      rocksdb::Status status = _db->GetBaseDB()->Flush(flushOptions, cf);
      if (!status.ok()) {
        res.reset(rocksutils::convertStatus(status));
        break;
      }
    }
  }

  return res;
}

void RocksDBEngine::waitForEstimatorSync(
    std::chrono::milliseconds maxWaitTime) {
  auto start = std::chrono::high_resolution_clock::now();
  auto beginSeq = _db->GetLatestSequenceNumber();

  while (std::chrono::high_resolution_clock::now() - start < maxWaitTime) {
    if (_settingsManager->earliestSeqNeeded() >= beginSeq) {
      // all synced up!
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

Result RocksDBEngine::registerRecoveryHelper(
    std::shared_ptr<RocksDBRecoveryHelper> helper) {
  try {
    _recoveryHelpers.emplace_back(helper);
  } catch (std::bad_alloc const&) {
    return {TRI_ERROR_OUT_OF_MEMORY};
  }

  return {TRI_ERROR_NO_ERROR};
}

std::vector<std::shared_ptr<RocksDBRecoveryHelper>> const&
RocksDBEngine::recoveryHelpers() {
  return _recoveryHelpers;
}

void RocksDBEngine::determineWalFilesInitial() {
  WRITE_LOCKER(lock, _walFileLock);
  // Retrieve the sorted list of all wal files with earliest file first
  rocksdb::VectorLogPtr files;
  auto status = _db->GetSortedWalFiles(files);
  if (!status.ok()) {
    LOG_TOPIC("078ee", WARN, Logger::ENGINES)
        << "could not get WAL files: " << status.ToString();
    return;
  }

  size_t archivedFiles = 0;
  for (size_t current = 0; current < files.size(); current++) {
    auto const& f = files[current].get();

    if (f->Type() != rocksdb::WalFileType::kArchivedLogFile) {
      // we are only interested in files of the archive
      continue;
    }

    ++archivedFiles;
  }
  _metricsWalSequenceLowerBound.operator=(
      _settingsManager->earliestSeqNeeded());
  _metricsArchivedWalFiles.operator=(archivedFiles);
}

void RocksDBEngine::determinePrunableWalFiles(TRI_voc_tick_t minTickExternal) {
  WRITE_LOCKER(lock, _walFileLock);
  TRI_voc_tick_t minTickToKeep =
      std::min(_useReleasedTick ? _releasedTick
                                : std::numeric_limits<TRI_voc_tick_t>::max(),
               minTickExternal);

  LOG_TOPIC("4673c", TRACE, Logger::ENGINES)
      << "determining prunable WAL files, minTickToKeep: " << minTickToKeep
      << ", minTickExternal: " << minTickExternal;

  // Retrieve the sorted list of all wal files with earliest file first
  rocksdb::VectorLogPtr files;
  auto status = _db->GetSortedWalFiles(files);
  if (!status.ok()) {
    LOG_TOPIC("078ef", WARN, Logger::ENGINES)
        << "could not get WAL files: " << status.ToString();
    return;
  }

  size_t archivedFiles = 0;
  uint64_t totalArchiveSize = 0;
  for (size_t current = 0; current < files.size(); current++) {
    auto const& f = files[current].get();

    if (f->Type() != rocksdb::WalFileType::kArchivedLogFile) {
      // we are only interested in files of the archive
      continue;
    }

    ++archivedFiles;

    // determine the size of the archive only if it there is a cap on the
    // archive size otherwise we can save the underlying file access
    if (_maxWalArchiveSizeLimit > 0) {
      totalArchiveSize += f->SizeFileBytes();
    }

    // check if there is another WAL file coming after the currently-looked-at
    // There should be at least one live WAL file after it, however, let's be
    // paranoid and do a proper check. If there is at least one WAL file
    // following, we need to take its start tick into account as well, because
    // the following file's start tick can be assumed to be the end tick of the
    // current file!
    if (f->StartSequence() < minTickToKeep && current < files.size() - 1) {
      auto const& n = files[current + 1].get();
      if (n->StartSequence() < minTickToKeep) {
        // this file will be removed because it does not contain any data we
        // still need
        auto const [it, emplaced] = _prunableWalFiles.try_emplace(
            f->PathName(), TRI_microtime() + _pruneWaitTime);
        if (emplaced) {
          LOG_TOPIC("9f7a4", DEBUG, Logger::ENGINES)
              << "RocksDB WAL file '" << f->PathName()
              << "' with start sequence " << f->StartSequence()
              << " added to prunable list because it is not needed anymore";
          TRI_ASSERT(it != _prunableWalFiles.end());
        }
      }
    }
  }

  LOG_TOPIC("01e20", TRACE, Logger::ENGINES)
      << "found " << files.size() << " WAL file(s), with " << archivedFiles
      << " files in the archive, "
      << "number of prunable files: " << _prunableWalFiles.size();

  if (_maxWalArchiveSizeLimit > 0 &&
      totalArchiveSize > _maxWalArchiveSizeLimit) {
    // size of the archive is restricted, and we overflowed the limit.

    // print current archive size
    LOG_TOPIC("8d71b", TRACE, Logger::ENGINES)
        << "total size of the RocksDB WAL file archive: " << totalArchiveSize;

    // we got more archived files than configured. time for purging some files!
    for (size_t current = 0; current < files.size(); current++) {
      auto const& f = files[current].get();

      if (f->Type() != rocksdb::WalFileType::kArchivedLogFile) {
        continue;
      }

      // force pruning
      bool doPrint = false;
      auto [it, emplaced] = _prunableWalFiles.try_emplace(f->PathName(), -1.0);
      if (emplaced) {
        doPrint = true;
      } else {
        // file already in list. now set its expiration time to the past
        // so we are sure it will get deleted

        // using an expiration time of -1.0 indicates the file is subject to
        // deletion because the archive outgrew the maximum allowed size
        if ((*it).second > 0.0) {
          doPrint = true;
        }
        (*it).second = -1.0;
      }

      if (doPrint) {
        TRI_ASSERT(totalArchiveSize > _maxWalArchiveSizeLimit);

        // never change this id without adjusting wal-archive-size-limit tests
        // in tests/js/client/server-parameters
        LOG_TOPIC("d9793", WARN, Logger::ENGINES)
            << "forcing removal of RocksDB WAL file '" << f->PathName()
            << "' with start sequence " << f->StartSequence()
            << " because of overflowing archive. configured maximum archive "
               "size is "
            << _maxWalArchiveSizeLimit
            << ", actual archive size is: " << totalArchiveSize
            << ". if these warnings persist, try to increase the value of "
            << "the startup option `--rocksdb.wal-archive-size-limit`";
      }

      TRI_ASSERT(totalArchiveSize >= f->SizeFileBytes());
      totalArchiveSize -= f->SizeFileBytes();

      if (totalArchiveSize <= _maxWalArchiveSizeLimit) {
        // got enough files to remove
        break;
      }
    }
  }

  _metricsWalSequenceLowerBound.operator=(
      _settingsManager->earliestSeqNeeded());
  _metricsArchivedWalFiles.operator=(archivedFiles);
  _metricsPrunableWalFiles.operator=(_prunableWalFiles.size());
  _metricsWalPruningActive.operator=(1);
}

RocksDBFilePurgePreventer RocksDBEngine::disallowPurging() noexcept {
  return RocksDBFilePurgePreventer(this);
}

RocksDBFilePurgeEnabler RocksDBEngine::startPurging() noexcept {
  return RocksDBFilePurgeEnabler(this);
}

void RocksDBEngine::pruneWalFiles() {
  // this struct makes sure that no other threads enter WAL tailing while we
  // are in here. If there are already other threads in WAL tailing while we
  // get here, we go on and only remove the WAL files that are really safe
  // to remove
  RocksDBFilePurgeEnabler purgeEnabler(startPurging());

  WRITE_LOCKER(lock, _walFileLock);

  // used for logging later
  size_t const initialSize = _prunableWalFiles.size();

  // go through the map of WAL files that we have already and check if they are
  // "expired"
  for (auto it = _prunableWalFiles.begin(); it != _prunableWalFiles.end();
       /* no hoisting */) {
    // check if WAL file is expired
    bool deleteFile = false;

    if ((*it).second <= 0.0) {
      // file can be deleted because we outgrew the configured max archive size,
      // but only if there are no other threads currently inside the WAL tailing
      // section
      deleteFile = purgeEnabler.canPurge();
    } else if ((*it).second < TRI_microtime()) {
      // file has expired, and it is always safe to delete it
      deleteFile = true;
    }

    if (deleteFile) {
      LOG_TOPIC("68e4a", DEBUG, Logger::ENGINES)
          << "deleting RocksDB WAL file '" << (*it).first << "'";
      rocksdb::Status s;
      if (basics::FileUtils::exists(basics::FileUtils::buildFilename(
              _dbOptions.wal_dir, (*it).first))) {
        // only attempt file deletion if the file actually exists.
        // otherwise RocksDB may complain about non-existing files and log a big
        // error message
        s = _db->DeleteFile((*it).first);
      } else {
        LOG_TOPIC("c2cc9", DEBUG, Logger::ENGINES)
            << "to-be-deleted RocksDB WAL file '" << (*it).first
            << "' does not exist. skipping deletion";
      }
      // apparently there is a case where a file was already deleted
      // but is still in _prunableWalFiles. In this case we get an invalid
      // argument response.
      if (s.ok() || s.IsInvalidArgument()) {
        it = _prunableWalFiles.erase(it);
        continue;
      } else {
        LOG_TOPIC("83162", WARN, Logger::ENGINES)
            << "attempt to prune RocksDB WAL file '" << (*it).first
            << "' failed with error: "
            << rocksutils::convertStatus(s).errorMessage();
      }
    }

    // cannot delete this file yet... must forward iterator to prevent an
    // endless loop
    ++it;
  }

  _metricsPrunableWalFiles.operator=(_prunableWalFiles.size());

  LOG_TOPIC("82a4c", TRACE, Logger::ENGINES)
      << "prune WAL files started with " << initialSize
      << " prunable WAL files, "
      << "current number of prunable WAL files: " << _prunableWalFiles.size();
}

Result RocksDBEngine::dropDatabase(TRI_voc_tick_t id) {
  using namespace rocksutils;
  arangodb::Result res;
  rocksdb::WriteOptions wo;
  rocksdb::DB* db = _db->GetRootDB();

  // remove view definitions
  res = rocksutils::removeLargeRange(db, RocksDBKeyBounds::DatabaseViews(id),
                                     true, /*rangeDel*/ false);
  if (res.fail()) {
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  size_t numDocsLeft = 0;
#endif

  // remove collections
  auto dbBounds = RocksDBKeyBounds::DatabaseCollections(id);
  iterateBounds(_db, dbBounds, [&](rocksdb::Iterator* it) {
    RocksDBKey key(it->key());
    RocksDBValue value(RocksDBEntryType::Collection, it->value());

    uint64_t const objectId = basics::VelocyPackHelper::stringUInt64(
        value.slice(), StaticStrings::ObjectId);

    auto const cnt = RocksDBMetadata::loadCollectionCount(_db, objectId);
    uint64_t const numberDocuments = cnt._added - cnt._removed;
    bool const useRangeDelete = numberDocuments >= 32 * 1024;

    // remove indexes
    VPackSlice indexes = value.slice().get("indexes");
    if (indexes.isArray()) {
      for (VPackSlice it : VPackArrayIterator(indexes)) {
        // delete index documents
        uint64_t objectId =
            basics::VelocyPackHelper::stringUInt64(it, StaticStrings::ObjectId);
        res = RocksDBMetadata::deleteIndexEstimate(db, objectId);
        if (res.fail()) {
          return;
        }

        TRI_ASSERT(it.get(StaticStrings::IndexType).isString());
        auto type = Index::type(it.get(StaticStrings::IndexType).copyString());
        bool unique = basics::VelocyPackHelper::getBooleanValue(
            it, StaticStrings::IndexUnique, false);

        RocksDBKeyBounds bounds =
            RocksDBIndex::getBounds(type, objectId, unique);
        // edge index drop fails otherwise
        bool const prefixSameAsStart = type != Index::TRI_IDX_TYPE_EDGE_INDEX;
        res = rocksutils::removeLargeRange(db, bounds, prefixSameAsStart,
                                           useRangeDelete);
        if (res.fail()) {
          return;
        }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        // check if documents have been deleted
        numDocsLeft += rocksutils::countKeyRange(
            db, bounds, /*snapshot*/ nullptr, prefixSameAsStart);
#endif
      }
    }

    // delete documents
    RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId);
    res = rocksutils::removeLargeRange(db, bounds, true, useRangeDelete);
    if (res.fail()) {
      LOG_TOPIC("6dbc6", WARN, Logger::ENGINES)
          << "error deleting collection documents: '" << res.errorMessage()
          << "'";
      return;
    }
    // delete collection meta-data
    res = RocksDBMetadata::deleteCollectionMeta(db, objectId);
    if (res.fail()) {
      LOG_TOPIC("484d0", WARN, Logger::ENGINES)
          << "error deleting collection metadata: '" << res.errorMessage()
          << "'";
      return;
    }
    // remove collection entry
    rocksdb::Status s =
        db->Delete(wo,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Definitions),
                   value.string());
    if (!s.ok()) {
      LOG_TOPIC("64b4e", WARN, Logger::ENGINES)
          << "error deleting collection definition: " << s.ToString();
      return;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // check if documents have been deleted
    numDocsLeft +=
        rocksutils::countKeyRange(db, bounds, /*snapshot*/ nullptr, true);
#endif
  });

  if (res.fail()) {
    return res;
  }

  // remove database meta-data
  RocksDBKey key;
  key.constructDatabase(id);
  rocksdb::Status s =
      db->Delete(wo,
                 RocksDBColumnFamilyManager::get(
                     RocksDBColumnFamilyManager::Family::Definitions),
                 key.string());
  if (!s.ok()) {
    LOG_TOPIC("9948c", WARN, Logger::ENGINES)
        << "error deleting database definition: " << s.ToString();
  }

  // remove VERSION file for database. it's not a problem when this fails
  // because it will simply remain there and be ignored on subsequent starts
  TRI_UnlinkFile(versionFilename(id).c_str());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (numDocsLeft > 0) {
    std::string errorMsg(
        "deletion check in drop database failed - not all documents have been "
        "deleted. remaining: ");
    errorMsg.append(std::to_string(numDocsLeft));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif

  return res;
}

bool RocksDBEngine::systemDatabaseExists() {
  velocypack::Builder builder;
  getDatabases(builder);

  for (auto const& item : velocypack::ArrayIterator(builder.slice())) {
    TRI_ASSERT(item.isObject());
    TRI_ASSERT(item.get(StaticStrings::DatabaseName).isString());
    if (item.get(StaticStrings::DatabaseName).stringView() ==
        StaticStrings::SystemDatabase) {
      return true;
    }
  }
  return false;
}

void RocksDBEngine::addSystemDatabase() {
  // create system database entry
  TRI_voc_tick_t id = TRI_NewTickServer();
  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::DatabaseId, VPackValue(std::to_string(id)));
  builder.add(StaticStrings::DatabaseName,
              VPackValue(StaticStrings::SystemDatabase));
  builder.add("deleted", VPackValue(false));
  builder.close();

  RocksDBLogValue log = RocksDBLogValue::DatabaseCreate(id);
  Result res = writeDatabaseMarker(id, builder.slice(), std::move(log));
  if (res.fail()) {
    LOG_TOPIC("8c26a", FATAL, arangodb::Logger::STARTUP)
        << "unable to write database marker: " << res.errorMessage();
    FATAL_ERROR_EXIT();
  }
}

/// @brief open an existing database. internal function
std::unique_ptr<TRI_vocbase_t> RocksDBEngine::openExistingDatabase(
    arangodb::CreateDatabaseInfo&& info, bool wasCleanShutdown,
    bool isUpgrade) {
  auto vocbase = std::make_unique<TRI_vocbase_t>(std::move(info));

  VPackBuilder builder;
  auto scanViews = [&](std::string_view type) {
    try {
      if (builder.isEmpty()) {
        auto r = getViews(*vocbase, builder);
        if (r != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(r);
        }
      }

      auto const slice = builder.slice();
      TRI_ASSERT(slice.isArray());

      for (VPackSlice it : VPackArrayIterator(slice)) {
        if (it.get(StaticStrings::DataSourceType).stringView() != type) {
          continue;
        }
        // we found a view that is still active

        TRI_ASSERT(!it.get("id").isNone());

        LogicalView::ptr view;
        auto res = LogicalView::instantiate(view, *vocbase, it, false);

        if (!res.ok()) {
          THROW_ARANGO_EXCEPTION(res);
        }

        if (!view) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              std::string("failed to instantiate view in vocbase'") +
                  vocbase->name() + "' from definition: " + it.toString());
        }

        StorageEngine::registerView(*vocbase, view);

        view->open();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("584b1", ERR, arangodb::Logger::ENGINES)
          << "error while opening database: " << ex.what();
      throw;
    } catch (...) {
      LOG_TOPIC("593fd", ERR, arangodb::Logger::ENGINES)
          << "error while opening database: unknown exception";
      throw;
    }
  };

  // scan the database path for "arangosearch" views
  scanViews(iresearch::StaticStrings::ViewArangoSearchType);

  try {
    loadReplicatedStates(*vocbase);
  } catch (std::exception const& ex) {
    LOG_TOPIC("554c1", ERR, arangodb::Logger::ENGINES)
        << "error while opening database: " << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC("5f33d", ERR, arangodb::Logger::ENGINES)
        << "error while opening database: unknown exception";
    throw;
  }

  // scan the database path for collections
  try {
    VPackBuilder builder;
    auto res = getCollectionsAndIndexes(*vocbase, builder, wasCleanShutdown,
                                        isUpgrade);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (VPackSlice it : VPackArrayIterator(slice)) {
      // we found a collection that is still active
      TRI_ASSERT(!it.get("id").isNone() || !it.get("cid").isNone());

      auto collection = vocbase->createCollectionObject(it, /*isAStub*/ false);
      TRI_ASSERT(collection != nullptr);

      auto phy = static_cast<RocksDBCollection*>(collection->getPhysical());
      TRI_ASSERT(phy != nullptr);
      Result r = phy->meta().deserializeMeta(_db, *collection);
      if (r.fail()) {
        LOG_TOPIC("4a404", ERR, arangodb::Logger::ENGINES)
            << "error while "
            << "loading metadata of collection '" << collection->name()
            << "': " << r.errorMessage();
      }

      StorageEngine::registerCollection(*vocbase, collection);
      LOG_TOPIC("39404", DEBUG, arangodb::Logger::ENGINES)
          << "added document collection '" << collection->name() << "'";
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC("8d427", ERR, arangodb::Logger::ENGINES)
        << "error while opening database: " << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC("0268e", ERR, arangodb::Logger::ENGINES)
        << "error while opening database: unknown exception";
    throw;
  }

  // scan the database path for "search-alias" views
  if (ServerState::instance()->isSingleServer()) {
    scanViews(iresearch::StaticStrings::ViewSearchAliasType);
  }

  return vocbase;
}

void RocksDBEngine::loadReplicatedStates(TRI_vocbase_t& vocbase) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(
      readOptions, RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::Definitions)));
  auto rSlice = rocksDBSlice(RocksDBEntryType::ReplicatedState);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase.id() != RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice =
        VPackSlice(reinterpret_cast<uint8_t const*>(iter->value().data()));
    if (basics::VelocyPackHelper::getBooleanValue(
            slice, StaticStrings::DataSourceDeleted, false)) {
      continue;
    }

    auto info = velocypack::deserialize<RocksDBReplicatedStateInfo>(slice);
    auto methods = std::make_unique<RocksDBLogStorageMethods>(
        info.objectId, vocbase.id(), info.stateId, _logPersistor, _db,
        RocksDBColumnFamilyManager::get(
            RocksDBColumnFamilyManager::Family::Definitions),
        RocksDBColumnFamilyManager::get(
            RocksDBColumnFamilyManager::Family::ReplicatedLogs));
    registerReplicatedState(vocbase, info.stateId, std::move(methods));
  }
}

void RocksDBEngine::scheduleFullIndexRefill(std::string const& database,
                                            std::string const& collection,
                                            IndexId iid) {
  // simply forward...
  RocksDBIndexCacheRefillFeature& f =
      server().getFeature<RocksDBIndexCacheRefillFeature>();
  f.scheduleFullIndexRefill(database, collection, iid);
}

void RocksDBEngine::syncIndexCaches() {
  RocksDBIndexCacheRefillFeature& f =
      server().getFeature<RocksDBIndexCacheRefillFeature>();
  f.waitForCatchup();
}

DECLARE_GAUGE(rocksdb_cache_active_tables, uint64_t,
              "rocksdb_cache_active_tables");
DECLARE_GAUGE(rocksdb_cache_allocated, uint64_t, "rocksdb_cache_allocated");
DECLARE_GAUGE(rocksdb_cache_hit_rate_lifetime, uint64_t,
              "rocksdb_cache_hit_rate_lifetime");
DECLARE_GAUGE(rocksdb_cache_hit_rate_recent, uint64_t,
              "rocksdb_cache_hit_rate_recent");
DECLARE_GAUGE(rocksdb_cache_limit, uint64_t, "rocksdb_cache_limit");
DECLARE_GAUGE(rocksdb_cache_unused_memory, uint64_t,
              "rocksdb_cache_unused_memory");
DECLARE_GAUGE(rocksdb_cache_unused_tables, uint64_t,
              "rocksdb_cache_unused_tables");
DECLARE_GAUGE(rocksdb_actual_delayed_write_rate, uint64_t,
              "rocksdb_actual_delayed_write_rate");
DECLARE_GAUGE(rocksdb_background_errors, uint64_t, "rocksdb_background_errors");
DECLARE_GAUGE(rocksdb_base_level, uint64_t, "rocksdb_base_level");
DECLARE_GAUGE(rocksdb_block_cache_capacity, uint64_t,
              "rocksdb_block_cache_capacity");
DECLARE_GAUGE(rocksdb_block_cache_pinned_usage, uint64_t,
              "rocksdb_block_cache_pinned_usage");
DECLARE_GAUGE(rocksdb_block_cache_usage, uint64_t, "rocksdb_block_cache_usage");
DECLARE_GAUGE(rocksdb_compaction_pending, uint64_t,
              "rocksdb_compaction_pending");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level0, uint64_t,
              "rocksdb_compression_ratio_at_level0");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level1, uint64_t,
              "rocksdb_compression_ratio_at_level1");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level2, uint64_t,
              "rocksdb_compression_ratio_at_level2");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level3, uint64_t,
              "rocksdb_compression_ratio_at_level3");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level4, uint64_t,
              "rocksdb_compression_ratio_at_level4");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level5, uint64_t,
              "rocksdb_compression_ratio_at_level5");
DECLARE_GAUGE(rocksdb_compression_ratio_at_level6, uint64_t,
              "rocksdb_compression_ratio_at_level6");
DECLARE_GAUGE(rocksdb_cur_size_active_mem_table, uint64_t,
              "rocksdb_cur_size_active_mem_table");
DECLARE_GAUGE(rocksdb_cur_size_all_mem_tables, uint64_t,
              "rocksdb_cur_size_all_mem_tables");
DECLARE_GAUGE(rocksdb_estimate_live_data_size, uint64_t,
              "rocksdb_estimate_live_data_size");
DECLARE_GAUGE(rocksdb_estimate_num_keys, uint64_t, "rocksdb_estimate_num_keys");
DECLARE_GAUGE(rocksdb_estimate_pending_compaction_bytes, uint64_t,
              "rocksdb_estimate_pending_compaction_bytes");
DECLARE_GAUGE(rocksdb_estimate_table_readers_mem, uint64_t,
              "rocksdb_estimate_table_readers_mem");
DECLARE_GAUGE(rocksdb_free_disk_space, uint64_t, "rocksdb_free_disk_space");
DECLARE_GAUGE(rocksdb_free_inodes, uint64_t, "rocksdb_free_inodes");
DECLARE_GAUGE(rocksdb_is_file_deletions_enabled, uint64_t,
              "rocksdb_is_file_deletions_enabled");
DECLARE_GAUGE(rocksdb_is_write_stopped, uint64_t, "rocksdb_is_write_stopped");
DECLARE_GAUGE(rocksdb_live_sst_files_size, uint64_t,
              "rocksdb_live_sst_files_size");
DECLARE_GAUGE(rocksdb_mem_table_flush_pending, uint64_t,
              "rocksdb_mem_table_flush_pending");
DECLARE_GAUGE(rocksdb_min_log_number_to_keep, uint64_t,
              "rocksdb_min_log_number_to_keep");
DECLARE_GAUGE(rocksdb_num_deletes_active_mem_table, uint64_t,
              "rocksdb_num_deletes_active_mem_table");
DECLARE_GAUGE(rocksdb_num_deletes_imm_mem_tables, uint64_t,
              "rocksdb_num_deletes_imm_mem_tables");
DECLARE_GAUGE(rocksdb_num_entries_active_mem_table, uint64_t,
              "rocksdb_num_entries_active_mem_table");
DECLARE_GAUGE(rocksdb_num_entries_imm_mem_tables, uint64_t,
              "rocksdb_num_entries_imm_mem_tables");
DECLARE_GAUGE(rocksdb_num_files_at_level0, uint64_t,
              "rocksdb_num_files_at_level0");
DECLARE_GAUGE(rocksdb_num_files_at_level1, uint64_t,
              "rocksdb_num_files_at_level1");
DECLARE_GAUGE(rocksdb_num_files_at_level2, uint64_t,
              "rocksdb_num_files_at_level2");
DECLARE_GAUGE(rocksdb_num_files_at_level3, uint64_t,
              "rocksdb_num_files_at_level3");
DECLARE_GAUGE(rocksdb_num_files_at_level4, uint64_t,
              "rocksdb_num_files_at_level4");
DECLARE_GAUGE(rocksdb_num_files_at_level5, uint64_t,
              "rocksdb_num_files_at_level5");
DECLARE_GAUGE(rocksdb_num_files_at_level6, uint64_t,
              "rocksdb_num_files_at_level6");
DECLARE_GAUGE(rocksdb_num_immutable_mem_table, uint64_t,
              "rocksdb_num_immutable_mem_table");
DECLARE_GAUGE(rocksdb_num_immutable_mem_table_flushed, uint64_t,
              "rocksdb_num_immutable_mem_table_flushed");
DECLARE_GAUGE(rocksdb_num_live_versions, uint64_t, "rocksdb_num_live_versions");
DECLARE_GAUGE(rocksdb_num_running_compactions, uint64_t,
              "rocksdb_num_running_compactions");
DECLARE_GAUGE(rocksdb_num_running_flushes, uint64_t,
              "rocksdb_num_running_flushes");
DECLARE_GAUGE(rocksdb_num_snapshots, uint64_t, "rocksdb_num_snapshots");
DECLARE_GAUGE(rocksdb_oldest_snapshot_time, uint64_t,
              "rocksdb_oldest_snapshot_time");
DECLARE_GAUGE(rocksdb_size_all_mem_tables, uint64_t,
              "rocksdb_size_all_mem_tables");
DECLARE_GAUGE(rocksdb_total_disk_space, uint64_t, "rocksdb_total_disk_space");
DECLARE_GAUGE(rocksdb_total_inodes, uint64_t, "rocksdb_total_inodes");
DECLARE_GAUGE(rocksdb_total_sst_files_size, uint64_t,
              "rocksdb_total_sst_files_size");
DECLARE_GAUGE(rocksdb_engine_throttle_bps, uint64_t,
              "rocksdb_engine_throttle_bps");
DECLARE_GAUGE(rocksdb_read_only, uint64_t, "rocksdb_read_only");

void RocksDBEngine::getStatistics(std::string& result) const {
  VPackBuilder stats;
  getStatistics(stats);
  VPackSlice sslice = stats.slice();
  TRI_ASSERT(sslice.isObject());
  for (auto a : VPackObjectIterator(sslice)) {
    if (a.value.isNumber()) {
      std::string name = a.key.copyString();
      std::replace(name.begin(), name.end(), '.', '_');
      std::replace(name.begin(), name.end(), '-', '_');
      if (!name.empty() && name.front() != 'r') {
        name = std::string{kEngineName}.append("_").append(name);
      }
      result += "\n# HELP " + name + " " + name + "\n# TYPE " + name +
                " gauge\n" + name + " " +
                std::to_string(a.value.getNumber<uint64_t>()) + "\n";
    }
  }
}

void RocksDBEngine::getStatistics(VPackBuilder& builder) const {
  // add int properties
  auto addInt = [&](std::string const& s) {
    std::string v;
    if (_db->GetProperty(s, &v)) {
      int64_t i = basics::StringUtils::int64(v);
      builder.add(s, VPackValue(i));
    }
  };

  // add string properties
  auto addStr = [&](std::string const& s) {
    std::string v;
    if (_db->GetProperty(s, &v)) {
      builder.add(s, VPackValue(v));
    }
  };

  // get string property from each column family and return sum;
  auto addIntAllCf = [&](std::string const& s) {
    int64_t sum = 0;
    for (auto cfh : RocksDBColumnFamilyManager::allHandles()) {
      std::string v;
      if (_db->GetProperty(cfh, s, &v)) {
        int64_t temp = basics::StringUtils::int64(v);

        // -1 returned for somethings that are valid property but no value
        if (0 < temp) {
          sum += temp;
        }
      }
    }
    builder.add(s, VPackValue(sum));
  };

  // add column family properties
  auto addCf = [&](RocksDBColumnFamilyManager::Family family) {
    std::string name = RocksDBColumnFamilyManager::name(
        family, RocksDBColumnFamilyManager::NameMode::External);
    rocksdb::ColumnFamilyHandle* c = RocksDBColumnFamilyManager::get(family);
    std::string v;
    builder.add(name, VPackValue(VPackValueType::Object));
    if (_db->GetProperty(c, rocksdb::DB::Properties::kCFStats, &v)) {
      builder.add("dbstats", VPackValue(v));
    }

    // re-add this line to count all keys in the column family (slow!!!)
    // builder.add("keys", VPackValue(rocksutils::countKeys(_db, c)));

    // estimate size on disk and in memtables
    uint64_t out = 0;
    rocksdb::Range r(
        rocksdb::Slice("\x00\x00\x00\x00\x00\x00\x00\x00", 8),
        rocksdb::Slice(
            "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff",
            16));

    rocksdb::SizeApproximationOptions options{.include_memtables = true,
                                              .include_files = true};
    _db->GetApproximateSizes(options, c, &r, 1, &out);

    builder.add("memory", VPackValue(out));
    builder.close();
  };

  builder.openObject();
  for (int i = 0; i < _optionsProvider.getOptions().num_levels; ++i) {
    addIntAllCf(rocksdb::DB::Properties::kNumFilesAtLevelPrefix +
                std::to_string(i));
    // ratio needs new calculation with all cf, not a simple add operation
    addIntAllCf(rocksdb::DB::Properties::kCompressionRatioAtLevelPrefix +
                std::to_string(i));
  }
  // caution:  you must read rocksdb/db/interal_stats.cc carefully to
  //           determine if a property is for whole database or one column
  //           family
  addIntAllCf(rocksdb::DB::Properties::kNumImmutableMemTable);
  addIntAllCf(rocksdb::DB::Properties::kNumImmutableMemTableFlushed);
  addIntAllCf(rocksdb::DB::Properties::kMemTableFlushPending);
  addIntAllCf(rocksdb::DB::Properties::kCompactionPending);
  addInt(rocksdb::DB::Properties::kBackgroundErrors);
  addIntAllCf(rocksdb::DB::Properties::kCurSizeActiveMemTable);
  addIntAllCf(rocksdb::DB::Properties::kCurSizeAllMemTables);
  addIntAllCf(rocksdb::DB::Properties::kSizeAllMemTables);
  addIntAllCf(rocksdb::DB::Properties::kNumEntriesActiveMemTable);
  addIntAllCf(rocksdb::DB::Properties::kNumEntriesImmMemTables);
  addIntAllCf(rocksdb::DB::Properties::kNumDeletesActiveMemTable);
  addIntAllCf(rocksdb::DB::Properties::kNumDeletesImmMemTables);
  addIntAllCf(rocksdb::DB::Properties::kEstimateNumKeys);
  addIntAllCf(rocksdb::DB::Properties::kEstimateTableReadersMem);
  addInt(rocksdb::DB::Properties::kNumSnapshots);
  addInt(rocksdb::DB::Properties::kOldestSnapshotTime);
  addIntAllCf(rocksdb::DB::Properties::kNumLiveVersions);
  addInt(rocksdb::DB::Properties::kMinLogNumberToKeep);
  addIntAllCf(rocksdb::DB::Properties::kEstimateLiveDataSize);
  addIntAllCf(rocksdb::DB::Properties::kLiveSstFilesSize);
  addStr(rocksdb::DB::Properties::kDBStats);
  addStr(rocksdb::DB::Properties::kSSTables);
  addInt(rocksdb::DB::Properties::kNumRunningCompactions);
  addInt(rocksdb::DB::Properties::kNumRunningFlushes);
  addInt(rocksdb::DB::Properties::kIsFileDeletionsEnabled);
  addIntAllCf(rocksdb::DB::Properties::kEstimatePendingCompactionBytes);
  addInt(rocksdb::DB::Properties::kBaseLevel);
  addInt(rocksdb::DB::Properties::kBlockCacheCapacity);
  addInt(rocksdb::DB::Properties::kBlockCacheUsage);
  addInt(rocksdb::DB::Properties::kBlockCachePinnedUsage);

  addIntAllCf(rocksdb::DB::Properties::kTotalSstFilesSize);
  addInt(rocksdb::DB::Properties::kActualDelayedWriteRate);
  addInt(rocksdb::DB::Properties::kIsWriteStopped);

  if (_dbOptions.statistics) {
    for (auto const& stat : rocksdb::TickersNameMap) {
      builder.add(
          stat.second,
          VPackValue(_dbOptions.statistics->getTickerCount(stat.first)));
    }

    uint64_t walWrite, flushWrite, compactionWrite, userWrite;
    walWrite = _dbOptions.statistics->getTickerCount(rocksdb::WAL_FILE_BYTES);
    flushWrite =
        _dbOptions.statistics->getTickerCount(rocksdb::FLUSH_WRITE_BYTES);
    compactionWrite =
        _dbOptions.statistics->getTickerCount(rocksdb::COMPACT_WRITE_BYTES);
    userWrite = _dbOptions.statistics->getTickerCount(rocksdb::BYTES_WRITTEN);
    builder.add(
        "rocksdbengine.write.amplification.x100",
        VPackValue((0 != userWrite)
                       ? ((walWrite + flushWrite + compactionWrite) * 100) /
                             userWrite
                       : 100));
  }

  cache::Manager* manager =
      server().getFeature<CacheManagerFeature>().manager();
  if (manager != nullptr) {
    // cache turned on
    cache::Manager::MemoryStats stats = manager->memoryStats();
    auto rates = manager->globalHitRates();
    builder.add("cache.limit", VPackValue(stats.globalLimit));
    builder.add("cache.allocated", VPackValue(stats.globalAllocation));
    builder.add("cache.active-tables", VPackValue(stats.activeTables));
    builder.add("cache.unused-memory", VPackValue(stats.spareAllocation));
    builder.add("cache.unused-tables", VPackValue(stats.spareTables));
    // handle NaN
    builder.add("cache.hit-rate-lifetime",
                VPackValue(rates.first >= 0.0 ? rates.first : 0.0));
    builder.add("cache.hit-rate-recent",
                VPackValue(rates.second >= 0.0 ? rates.second : 0.0));
  } else {
    // cache turned off
    builder.add("cache.limit", VPackValue(0));
    builder.add("cache.allocated", VPackValue(0));
    builder.add("cache.active-tables", VPackValue(0));
    builder.add("cache.unused-memory", VPackValue(0));
    builder.add("cache.unused-tables", VPackValue(0));
    // handle NaN
    builder.add("cache.hit-rate-lifetime", VPackValue(0));
    builder.add("cache.hit-rate-recent", VPackValue(0));
  }

  // print column family statistics
  //  warning: output format limits numbers to 3 digits of precision or less.
  builder.add("columnFamilies", VPackValue(VPackValueType::Object));
  addCf(RocksDBColumnFamilyManager::Family::Definitions);
  addCf(RocksDBColumnFamilyManager::Family::Documents);
  addCf(RocksDBColumnFamilyManager::Family::PrimaryIndex);
  addCf(RocksDBColumnFamilyManager::Family::EdgeIndex);
  addCf(RocksDBColumnFamilyManager::Family::VPackIndex);
  addCf(RocksDBColumnFamilyManager::Family::GeoIndex);
  addCf(RocksDBColumnFamilyManager::Family::FulltextIndex);
  addCf(RocksDBColumnFamilyManager::Family::ZkdIndex);
  addCf(RocksDBColumnFamilyManager::Family::ReplicatedLogs);
  builder.close();

  if (_throttleListener) {
    builder.add("rocksdb_engine.throttle.bps",
                VPackValue(_throttleListener->getThrottle()));
  }

  {
    // total disk space in database directory
    uint64_t totalSpace = 0;
    // free disk space in database directory
    uint64_t freeSpace = 0;
    Result res = TRI_GetDiskSpaceInfo(_basePath, totalSpace, freeSpace);
    if (res.ok()) {
      builder.add("rocksdb.free-disk-space", VPackValue(freeSpace));
      builder.add("rocksdb.total-disk-space", VPackValue(totalSpace));
    } else {
      builder.add("rocksdb.free-disk-space", VPackValue(VPackValueType::Null));
      builder.add("rocksdb.total-disk-space", VPackValue(VPackValueType::Null));
    }
  }

  {
    // total inodes for database directory
    uint64_t totalINodes = 0;
    // free inodes for database directory
    uint64_t freeINodes = 0;
    Result res = TRI_GetINodesInfo(_basePath, totalINodes, freeINodes);
    if (res.ok()) {
      builder.add("rocksdb.free-inodes", VPackValue(freeINodes));
      builder.add("rocksdb.total-inodes", VPackValue(totalINodes));
    } else {
      builder.add("rocksdb.free-inodes", VPackValue(VPackValueType::Null));
      builder.add("rocksdb.total-inodes", VPackValue(VPackValueType::Null));
    }
  }

  if (_errorListener) {
    builder.add("rocksdb.read-only",
                VPackValue(_errorListener->called() ? 1 : 0));
  }

  auto sequenceNumber = _db->GetLatestSequenceNumber();
  builder.add("rocksdb.wal-sequence", VPackValue(sequenceNumber));

  builder.close();
}

Result RocksDBEngine::handleSyncKeys(DatabaseInitialSyncer& syncer,
                                     LogicalCollection& col,
                                     std::string const& keysId) {
  return handleSyncKeysRocksDB(syncer, &col, keysId);
}

Result RocksDBEngine::createLoggerState(TRI_vocbase_t* vocbase,
                                        VPackBuilder& builder) {
  builder.openObject();  // Base
  rocksdb::SequenceNumber lastTick = _db->GetLatestSequenceNumber();

  // "state" part
  builder.add("state", VPackValue(VPackValueType::Object));  // open

  // always hard-coded to true
  builder.add("running", VPackValue(true));

  builder.add("lastLogTick", VPackValue(std::to_string(lastTick)));

  // not used anymore in 3.8:
  builder.add("lastUncommittedLogTick", VPackValue(std::to_string(lastTick)));

  // not used anymore in 3.8:
  builder.add("totalEvents", VPackValue(lastTick));

  builder.add("time", VPackValue(utilities::timeString()));
  builder.close();

  // "server" part
  builder.add("server", VPackValue(VPackValueType::Object));  // open
  builder.add("version", VPackValue(ARANGODB_VERSION));
  builder.add("serverId",
              VPackValue(std::to_string(ServerIdFeature::getId().id())));
  builder.add("engine", VPackValue(kEngineName));  // "rocksdb"
  builder.close();

  // "clients" part
  builder.add("clients", VPackValue(VPackValueType::Array));  // open
  if (vocbase != nullptr) {
    vocbase->replicationClients().toVelocyPack(builder);
  }
  builder.close();  // clients
  builder.close();  // base

  return {};
}

Result RocksDBEngine::createTickRanges(VPackBuilder& builder) {
  rocksdb::VectorLogPtr walFiles;
  rocksdb::Status s = _db->GetSortedWalFiles(walFiles);

  Result res = rocksutils::convertStatus(s);
  if (res.fail()) {
    return res;
  }

  builder.openArray();
  for (auto lfile = walFiles.begin(); lfile != walFiles.end(); ++lfile) {
    auto& logfile = *lfile;
    builder.openObject();
    // filename and state are already of type string
    builder.add("datafile", VPackValue(logfile->PathName()));
    if (logfile->Type() == rocksdb::WalFileType::kAliveLogFile) {
      builder.add("status", VPackValue("open"));
    } else if (logfile->Type() == rocksdb::WalFileType::kArchivedLogFile) {
      builder.add("status", VPackValue("collected"));
    }
    rocksdb::SequenceNumber min = logfile->StartSequence();
    builder.add("tickMin", VPackValue(std::to_string(min)));
    rocksdb::SequenceNumber max;
    if (std::next(lfile) != walFiles.end()) {
      max = (*std::next(lfile))->StartSequence();
    } else {
      max = _db->GetLatestSequenceNumber();
    }
    builder.add("tickMax", VPackValue(std::to_string(max)));
    builder.close();
  }
  builder.close();

  return {};
}

Result RocksDBEngine::firstTick(uint64_t& tick) {
  rocksdb::VectorLogPtr walFiles;
  rocksdb::Status s = _db->GetSortedWalFiles(walFiles);

  Result res;
  if (!s.ok()) {
    res = rocksutils::convertStatus(s);
  } else {
    // read minium possible tick
    if (!walFiles.empty()) {
      tick = walFiles[0]->StartSequence();
    }
  }
  return res;
}

Result RocksDBEngine::lastLogger(TRI_vocbase_t& vocbase, uint64_t tickStart,
                                 uint64_t tickEnd, VPackBuilder& builder) {
  bool includeSystem = true;
  size_t chunkSize = 32 * 1024 * 1024;  // TODO: determine good default value?

  builder.openArray();
  RocksDBReplicationResult rep =
      rocksutils::tailWal(&vocbase, tickStart, tickEnd, chunkSize,
                          includeSystem, DataSourceId::none(), builder);
  builder.close();

  return std::move(rep).result();
}

WalAccess const* RocksDBEngine::walAccess() const {
  TRI_ASSERT(_walAccess);
  return _walAccess.get();
}

/// @brief get compression supported by RocksDB
std::string RocksDBEngine::getCompressionSupport() const {
  std::string result;

  for (auto const& type : rocksdb::GetSupportedCompressions()) {
    std::string out;
    rocksdb::GetStringFromCompressionType(&out, type);

    if (out.empty()) {
      continue;
    }
    if (!result.empty()) {
      result.append(", ");
    }
    result.append(out);
  }
  return result;
}

// management methods for synchronizing with external persistent stores
TRI_voc_tick_t RocksDBEngine::currentTick() const {
  return _db->GetLatestSequenceNumber();
}

TRI_voc_tick_t RocksDBEngine::releasedTick() const {
  READ_LOCKER(lock, _walFileLock);
  return _releasedTick;
}

void RocksDBEngine::releaseTick(TRI_voc_tick_t tick) {
  WRITE_LOCKER(lock, _walFileLock);
  if (tick > _releasedTick) {
    _releasedTick = tick;
  }
}

HealthData RocksDBEngine::healthCheck() {
  auto now = std::chrono::steady_clock::now();

  // the following checks are executed under a mutex so that different
  // threads can potentially call in here without messing up any data.
  // in addition, serializing access to this function avoids stampedes
  // with multiple threads trying to calculate the free disk space
  // capacity at the same time, which could be expensive.
  MUTEX_LOCKER(guard, _healthMutex);

  TRI_IF_FAILURE("RocksDBEngine::healthCheck") {
    _healthData.res.reset(TRI_ERROR_DEBUG, "peng! 💥");
    return _healthData;
  }

  bool lastCheckLongAgo =
      (_healthData.lastCheckTimestamp.time_since_epoch().count() == 0) ||
      ((now - _healthData.lastCheckTimestamp) >= std::chrono::seconds(30));
  if (lastCheckLongAgo) {
    _healthData.lastCheckTimestamp = now;
  }

  // only log about once every 24 hours, to reduce log spam
  bool lastLogMessageLongAgo =
      (_lastHealthLogMessageTimestamp.time_since_epoch().count() == 0) ||
      ((now - _lastHealthLogMessageTimestamp) >= std::chrono::hours(24));

  _healthData.backgroundError = hasBackgroundError();

  if (_healthData.backgroundError) {
    // go into failed state
    _healthData.res.reset(
        TRI_ERROR_FAILED,
        "storage engine reports background error. please check the logs "
        "for the error reason and take action");
  } else if (_lastHealthCheckSuccessful) {
    _healthData.res.reset();
  }

  if (lastCheckLongAgo || !_lastHealthCheckSuccessful) {
    // check the amount of free disk space. this may be expensive to do, so
    // we only execute the check every once in a while, or when the last check
    // failed too (so that we don't report success only because we skipped the
    // checks)
    //
    // total disk space in database directory
    uint64_t totalSpace = 0;
    // free disk space in database directory
    uint64_t freeSpace = 0;

    if (TRI_GetDiskSpaceInfo(_basePath, totalSpace, freeSpace).ok() &&
        totalSpace >= 1024 * 1024) {
      // only carry out the following if we get a disk size of at least 1MB
      // back. everything else seems to be very unreasonable and not
      // trustworthy.
      double diskFreePercentage = double(freeSpace) / double(totalSpace);
      _healthData.freeDiskSpaceBytes = freeSpace;
      _healthData.freeDiskSpacePercent = diskFreePercentage;

      if (_healthData.res.ok() &&
          ((_requiredDiskFreePercentage > 0.0 &&
            diskFreePercentage < _requiredDiskFreePercentage) ||
           (_requiredDiskFreeBytes > 0 &&
            freeSpace < _requiredDiskFreeBytes))) {
        std::stringstream ss;
        ss << "free disk space capacity has reached critical level, "
           << "bytes free: " << freeSpace
           << ", % free: " << std::setprecision(1) << std::fixed
           << (diskFreePercentage * 100.0);
        // go into failed state
        _healthData.res.reset(TRI_ERROR_FAILED, ss.str());
      } else if (diskFreePercentage < 0.05 || freeSpace < 256 * 1024 * 1024) {
        // warnings about disk space only every 15 minutes
        bool lastLogWarningLongAgo =
            (now - _lastHealthLogWarningTimestamp >= std::chrono::minutes(15));
        if (lastLogWarningLongAgo) {
          LOG_TOPIC("54e7f", WARN, Logger::ENGINES)
              << "free disk space capacity is low, "
              << "bytes free: " << freeSpace
              << ", % free: " << std::setprecision(1) << std::fixed
              << (diskFreePercentage * 100.0);
          _lastHealthLogWarningTimestamp = now;
        }
        // don't go into failed state (yet)
      }
    }
  }

  _lastHealthCheckSuccessful = _healthData.res.ok();

  if (_healthData.res.fail() && lastLogMessageLongAgo) {
    LOG_TOPIC("ead1f", ERR, Logger::ENGINES) << _healthData.res.errorMessage();

    // update timestamp of last log message
    _lastHealthLogMessageTimestamp = now;
  }

  return _healthData;
}

void RocksDBEngine::waitForCompactionJobsToFinish() {
  // wait for started compaction jobs to finish
  int iterations = 0;

  do {
    size_t numRunning;
    {
      READ_LOCKER(locker, _pendingCompactionsLock);
      numRunning = _runningCompactions;
    }
    if (numRunning == 0) {
      return;
    }

    // print this only every few seconds
    if (iterations++ % 200 == 0) {
      LOG_TOPIC("9cbfd", INFO, Logger::ENGINES)
          << "waiting for " << numRunning << " compaction job(s) to finish...";
    }
    // unfortunately there is not much we can do except waiting for
    // RocksDB's compaction job(s) to finish.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // wait up to 2 minutes, and then give up
  } while (iterations <= 2400);

  LOG_TOPIC("c537b", WARN, Logger::ENGINES)
      << "giving up waiting for pending compaction job(s)";
}

bool RocksDBEngine::checkExistingDB(
    std::vector<rocksdb::ColumnFamilyDescriptor> const& cfFamilies) {
  bool dbExisted = false;

  rocksdb::Options testOptions;
  testOptions.create_if_missing = false;
  testOptions.create_missing_column_families = false;
  testOptions.avoid_flush_during_recovery = true;
  testOptions.avoid_flush_during_shutdown = true;
  testOptions.env = _dbOptions.env;

  std::vector<std::string> existingColumnFamilies;
  rocksdb::Status status = rocksdb::DB::ListColumnFamilies(
      testOptions, _path, &existingColumnFamilies);
  if (!status.ok()) {
    // check if we have found the database directory or not
    Result res = rocksutils::convertStatus(status);
    if (res.errorNumber() != TRI_ERROR_ARANGO_IO_ERROR) {
      // not an I/O error. so we better report the error and abort here
      LOG_TOPIC("74b7f", FATAL, arangodb::Logger::STARTUP)
          << "unable to initialize RocksDB engine: " << res.errorMessage();
      FATAL_ERROR_EXIT();
    }
  }

  if (status.ok()) {
    dbExisted = true;
    // we were able to open the database.
    // now check which column families are present in the db
    std::string names;
    for (auto const& it : existingColumnFamilies) {
      if (!names.empty()) {
        names.append(", ");
      }
      names.append(it);
    }

    LOG_TOPIC("528b8", DEBUG, arangodb::Logger::STARTUP)
        << "found existing column families: " << names;
    auto const replicatedLogsName = RocksDBColumnFamilyManager::name(
        RocksDBColumnFamilyManager::Family::ReplicatedLogs);
    auto const zkdIndexName = RocksDBColumnFamilyManager::name(
        RocksDBColumnFamilyManager::Family::ZkdIndex);

    for (auto const& it : cfFamilies) {
      auto it2 = std::find(existingColumnFamilies.begin(),
                           existingColumnFamilies.end(), it.name);
      if (it2 == existingColumnFamilies.end()) {
        if (it.name == replicatedLogsName || it.name == zkdIndexName) {
          LOG_TOPIC("293c3", INFO, Logger::STARTUP)
              << "column family " << it.name
              << " is missing and will be created.";
          continue;
        }

        LOG_TOPIC("d9df8", FATAL, arangodb::Logger::STARTUP)
            << "column family '" << it.name << "' is missing in database"
            << ". if you are upgrading from an earlier alpha or beta version "
               "of ArangoDB, it is required to restart with a new database "
               "directory and "
               "re-import data";
        FATAL_ERROR_EXIT();
      }
    }

    if (existingColumnFamilies.size() <
        RocksDBColumnFamilyManager::minNumberOfColumnFamilies) {
      LOG_TOPIC("e99ec", FATAL, arangodb::Logger::STARTUP)
          << "unexpected number of column families found in database ("
          << existingColumnFamilies.size() << "). "
          << "expecting at least "
          << RocksDBColumnFamilyManager::minNumberOfColumnFamilies
          << ". if you are upgrading from an earlier alpha or beta version "
             "of ArangoDB, it is required to restart with a new database "
             "directory and "
             "re-import data";
      FATAL_ERROR_EXIT();
    }
  }

  return dbExisted;
}

Result RocksDBEngine::dropReplicatedState(
    TRI_vocbase_t& vocbase,
    std::unique_ptr<replication2::replicated_state::IStorageEngineMethods>&
        ptr) {
  // make sure that all pending async operations have completed
  auto methods = dynamic_cast<RocksDBLogStorageMethods*>(ptr.get());
  ADB_PROD_ASSERT(methods != nullptr);
  methods->ctx.waitForCompletion();

  // drop everything
  if (auto res = methods->drop(); res.fail()) {
    return res;
  }

  // do a compaction for the log range
  std::ignore = methods->compact();
  ptr.reset();
  return {};
}

auto RocksDBEngine::createReplicatedState(
    TRI_vocbase_t& vocbase, arangodb::replication2::LogId id,
    replication2::replicated_state::PersistedStateInfo const& info)
    -> ResultT<std::unique_ptr<
        replication2::replicated_state::IStorageEngineMethods>> {
  auto objectId = TRI_NewTickServer();
  auto methods = std::make_unique<RocksDBLogStorageMethods>(
      objectId, vocbase.id(), id, _logPersistor, _db,
      RocksDBColumnFamilyManager::get(
          RocksDBColumnFamilyManager::Family::Definitions),
      RocksDBColumnFamilyManager::get(
          RocksDBColumnFamilyManager::Family::ReplicatedLogs));
  methods->updateMetadata(info);
  return {std::move(methods)};
}

std::shared_ptr<StorageSnapshot> RocksDBEngine::currentSnapshot() {
  if (ADB_LIKELY(_db)) {
    return std::make_shared<RocksDBSnapshot>(*_db);
  } else {
    return nullptr;
  }
}

}  // namespace arangodb
