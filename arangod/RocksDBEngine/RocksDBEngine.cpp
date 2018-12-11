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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine.h"
#include "ApplicationFeatures/RocksDBOptionFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBLogger.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/build.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Manager.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RocksDBEngine/RocksDBBackgroundThread.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBIncrementalSync.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBOptimizerRules.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "RocksDBEngine/RocksDBRestHandlers.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "RocksDBEngine/RocksDBThrottle.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionContextData.h"
#include "RocksDBEngine/RocksDBTransactionManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBUpgrade.h"
#include "RocksDBEngine/RocksDBV8Functions.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBWalAccess.h"
#include "Transaction/Context.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/ticks.h"
#include "VocBase/LogicalView.h"

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/statistics.h>
#include <rocksdb/table.h>
#include <rocksdb/transaction_log.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

std::string const RocksDBEngine::EngineName("rocksdb");
std::string const RocksDBEngine::FeatureName("RocksDBEngine");

// static variables for all existing column families
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_definitions(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_documents(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_primary(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_edge(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_vpack(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_geo(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_fulltext(nullptr);
std::vector<rocksdb::ColumnFamilyHandle*> RocksDBColumnFamily::_allHandles;

// minimum value for --rocksdb.sync-interval (in ms)
// a value of 0 however means turning off the syncing altogether!
static constexpr uint64_t minSyncInterval = 5;

static constexpr uint64_t databaseIdForGlobalApplier = 0;

// handles for recovery helpers
std::vector<std::shared_ptr<RocksDBRecoveryHelper>>
    RocksDBEngine::_recoveryHelpers;

// create the storage engine
RocksDBEngine::RocksDBEngine(application_features::ApplicationServer& server)
    : StorageEngine(
        server,
        EngineName,
        FeatureName,
        std::unique_ptr<IndexFactory>(new RocksDBIndexFactory())
      ),
      _db(nullptr),
      _vpackCmp(new RocksDBVPackComparator()),
      _walAccess(new RocksDBWalAccess()),
      _maxTransactionSize(transaction::Options::defaultMaxTransactionSize),
      _intermediateCommitSize(
          transaction::Options::defaultIntermediateCommitSize),
      _intermediateCommitCount(
          transaction::Options::defaultIntermediateCommitCount),
      _pruneWaitTime(10.0),
      _pruneWaitTimeInitial(180.0),
      _releasedTick(0),
#ifdef _WIN32
      // background syncing is not supported on Windows
      _syncInterval(0),
#else
      _syncInterval(100),
#endif
      _useThrottle(true),
      _debugLogging(false) {

  startsAfter("BasicsPhase");
  // inherits order from StorageEngine but requires "RocksDBOption" that is used
  // to configure this engine and the MMFiles PersistentIndexFeature
  startsAfter("RocksDBOption");

  server.addFeature(new RocksDBRecoveryManager(server));
}

RocksDBEngine::~RocksDBEngine() { shutdownRocksDBInstance(); }

/// shuts down the RocksDB instance. this is called from unprepare
/// and the dtor
void RocksDBEngine::shutdownRocksDBInstance() noexcept {
  if (_db == nullptr) {
    return;
  }

  // turn off RocksDBThrottle, and release our pointers to it
  if (nullptr != _listener.get()) {
    _listener->StopThread();
  }  // if

  for (rocksdb::ColumnFamilyHandle* h : RocksDBColumnFamily::_allHandles) {
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
      LOG_TOPIC(WARN, Logger::ENGINES)
          << "could not sync RocksDB WAL: " << res.errorMessage();
    }

    rocksdb::Status status = _db->Close();

    if (!status.ok()) {
      Result res = rocksutils::convertStatus(status);
      LOG_TOPIC(ERR, Logger::ENGINES)
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
  options->addSection("rocksdb", "RocksDB engine specific configuration");

  // control transaction size for RocksDB engine
  options->addOption("--rocksdb.max-transaction-size",
                     "transaction size limit (in bytes)",
                     new UInt64Parameter(&_maxTransactionSize));

  options->addOption("--rocksdb.intermediate-commit-size",
                     "an intermediate commit will be performed automatically "
                     "when a transaction "
                     "has accumulated operations of this size (in bytes)",
                     new UInt64Parameter(&_intermediateCommitSize));

  options->addOption("--rocksdb.intermediate-commit-count",
                     "an intermediate commit will be performed automatically "
                     "when this number of "
                     "operations is reached in a transaction",
                     new UInt64Parameter(&_intermediateCommitCount));

  options->addOption("--rocksdb.sync-interval",
                     "interval for automatic, non-requested disk syncs (in milliseconds, use 0 to turn automatic syncing off)",
                     new UInt64Parameter(&_syncInterval));

  options->addOption("--rocksdb.wal-file-timeout",
                     "timeout after which unused WAL files are deleted",
                     new DoubleParameter(&_pruneWaitTime));

  options->addOption("--rocksdb.wal-file-timeout-initial",
                     "initial timeout after which unused WAL files deletion kicks in after server start",
                     new DoubleParameter(&_pruneWaitTimeInitial),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--rocksdb.throttle",
                     "enable write-throttling",
                     new BooleanParameter(&_useThrottle));

  options->addOption("--rocksdb.debug-logging",
                     "true to enable rocksdb debug logging",
                     new BooleanParameter(&_debugLogging),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

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

  if (_syncInterval > 0 && _syncInterval < minSyncInterval) {
    // _syncInterval = 0 means turned off!
    LOG_TOPIC(FATAL, arangodb::Logger::CONFIG)
        << "invalid value for --rocksdb.sync-interval. Please use a value "
        << "of at least " << minSyncInterval;
    FATAL_ERROR_EXIT();
  }

#ifdef _WIN32
  if (_syncInterval > 0) {
    LOG_TOPIC(WARN, arangodb::Logger::CONFIG)
        << "automatic syncing of RocksDB WAL via background thread is not "
        << " supported on this platform";
  }
#endif
  
  if (_pruneWaitTimeInitial < 10) {
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
    << "consider increasing the value for --rocksdb.wal-file-timeout-initial. "
    << "Replication clients might have trouble to get in sync";
  }
}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void RocksDBEngine::prepare() {
  // get base path from DatabaseServerFeature
  auto databasePathFeature =
      application_features::ApplicationServer::getFeature<DatabasePathFeature>(
          "DatabasePath");
  _basePath = databasePathFeature->directory();

  TRI_ASSERT(!_basePath.empty());

#ifdef USE_ENTERPRISE
  prepareEnterprise();
#endif
}

void RocksDBEngine::start() {
  // it is already decided that rocksdb is used
  if (!isEnabled()) {
    return;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
      << "rocksdb version " << rest::Version::getRocksDBVersion()
      << ", supported compression types: " << getCompressionSupport();

  // set the database sub-directory for RocksDB
  auto* databasePathFeature =
      ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _path = databasePathFeature->subdirectoryName("engine-rocksdb");

  if (!basics::FileUtils::isDirectory(_path)) {
    std::string systemErrorStr;
    long errorNo;

    int res = TRI_CreateRecursiveDirectory(_path.c_str(), errorNo,
                                           systemErrorStr);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "created RocksDB data directory '" << _path << "'";
    } else {
      LOG_TOPIC(FATAL, arangodb::Logger::ENGINES) << "unable to create RocksDB data directory '" << _path << "': " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }

  // options imported set by RocksDBOptionFeature
  auto const* opts =
  ApplicationServer::getFeature<arangodb::RocksDBOptionFeature>(
                                                                "RocksDBOption");

  rocksdb::TransactionDBOptions transactionOptions;
  // number of locks per column_family
  transactionOptions.num_stripes = TRI_numberProcessors();
  transactionOptions.transaction_lock_timeout = opts->_transactionLockTimeout;

  _options.enable_pipelined_write = opts->_enablePipelinedWrite;
  _options.write_buffer_size = static_cast<size_t>(opts->_writeBufferSize);
  _options.max_write_buffer_number =
      static_cast<int>(opts->_maxWriteBufferNumber);
  _options.delayed_write_rate = opts->_delayedWriteRate;
  _options.min_write_buffer_number_to_merge =
      static_cast<int>(opts->_minWriteBufferNumberToMerge);
  _options.num_levels = static_cast<int>(opts->_numLevels);
  _options.level_compaction_dynamic_level_bytes = opts->_dynamicLevelBytes;
  _options.max_bytes_for_level_base = opts->_maxBytesForLevelBase;
  _options.max_bytes_for_level_multiplier =
      static_cast<int>(opts->_maxBytesForLevelMultiplier);
  _options.optimize_filters_for_hits = opts->_optimizeFiltersForHits;
  _options.use_direct_reads = opts->_useDirectReads;
  _options.use_direct_io_for_flush_and_compaction =
      opts->_useDirectIoForFlushAndCompaction;
  // limit the total size of WAL files. This forces the flush of memtables of
  // column families still backed by WAL files. If we would not do this, WAL
  // files may linger around forever and will not get removed
  _options.max_total_wal_size = opts->_maxTotalWalSize;

  if (opts->_walDirectory.empty()) {
    _options.wal_dir = basics::FileUtils::buildFilename(_path, "journals");
  } else {
    _options.wal_dir = opts->_walDirectory;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
      << "initializing RocksDB, path: '" << _path << "', WAL directory '"
      << _options.wal_dir << "'";

  if (opts->_skipCorrupted) {
    _options.wal_recovery_mode =
        rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;
  } else {
    _options.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
  }

  _options.max_background_jobs = static_cast<int>(opts->_maxBackgroundJobs);
  _options.max_subcompactions = static_cast<int>(opts->_maxSubcompactions);
  _options.use_fsync = opts->_useFSync;

  // only compress levels >= 2
  _options.compression_per_level.resize(_options.num_levels);
  for (int level = 0; level < _options.num_levels; ++level) {
    _options.compression_per_level[level] =
        (((uint64_t)level >= opts->_numUncompressedLevels)
             ? rocksdb::kSnappyCompression
             : rocksdb::kNoCompression);
  }

  // Number of files to trigger level-0 compaction. A value <0 means that
  // level-0 compaction will not be triggered by number of files at all.
  // Default: 4
  _options.level0_file_num_compaction_trigger =
      static_cast<int>(opts->_level0CompactionTrigger);

  // Soft limit on number of level-0 files. We start slowing down writes at this
  // point. A value <0 means that no writing slow down will be triggered by
  // number of files in level-0.
  _options.level0_slowdown_writes_trigger =
      static_cast<int>(opts->_level0SlowdownTrigger);

  // Maximum number of level-0 files.  We stop writes at this point.
  _options.level0_stop_writes_trigger =
      static_cast<int>(opts->_level0StopTrigger);

  _options.recycle_log_file_num = static_cast<size_t>(opts->_recycleLogFileNum);
  _options.compaction_readahead_size =
      static_cast<size_t>(opts->_compactionReadaheadSize);

#ifdef USE_ENTERPRISE
  configureEnterpriseRocksDBOptions(_options);
  startEnterprise();
#endif

  _options.env->SetBackgroundThreads(static_cast<int>(opts->_numThreadsHigh),
                                     rocksdb::Env::Priority::HIGH);
  _options.env->SetBackgroundThreads(static_cast<int>(opts->_numThreadsLow),
                                     rocksdb::Env::Priority::LOW);

  // intentionally set the RocksDB logger to warning because it will
  // log lots of things otherwise
  if (_debugLogging) {
    _options.info_log_level = rocksdb::InfoLogLevel::DEBUG_LEVEL;
  } else {
    if (!opts->_useFileLogging) {
      // if we don't use file logging but log into ArangoDB's logfile,
      // we only want real errors
      _options.info_log_level = rocksdb::InfoLogLevel::ERROR_LEVEL;
    }
  }

  std::shared_ptr<RocksDBLogger> logger;

  if (!opts->_useFileLogging) {
    // if option "--rocksdb.use-file-logging" is set to false, we will use
    // our own logger that logs to ArangoDB's logfile
    logger = std::make_shared<RocksDBLogger>(_options.info_log_level);
    _options.info_log = logger;

    if (!_debugLogging) {
      logger->disable();
    } // if
  }

  if (opts->_enableStatistics) {
    _options.statistics = rocksdb::CreateDBStatistics();
    // _options.stats_dump_period_sec = 1;
  }

  rocksdb::BlockBasedTableOptions tableOptions;
  if (opts->_blockCacheSize > 0) {
    tableOptions.block_cache = rocksdb::NewLRUCache(
        opts->_blockCacheSize, 
        static_cast<int>(opts->_blockCacheShardBits), 
        /*strict_capacity_limit*/ opts->_enforceBlockCacheSizeLimit
    );
    // tableOptions.cache_index_and_filter_blocks =
    // opts->_compactionReadaheadSize > 0;
  } else {
    tableOptions.no_block_cache = true;
  }
  tableOptions.block_size = opts->_tableBlockSize;
  tableOptions.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, true));
  // use slightly space-optimized format version 3
  tableOptions.format_version = 3;
  tableOptions.block_align = opts->_blockAlignDataBlocks;

  _options.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(tableOptions));

  _options.create_if_missing = true;
  _options.create_missing_column_families = true;
  _options.max_open_files = -1;

  // WAL_ttl_seconds needs to be bigger than the sync interval of the count
  // manager. Should be several times bigger counter_sync_seconds
  _options.WAL_ttl_seconds = 60 * 60 * 24 * 30;  // we manage WAL file deletion
  // ourselves, don't let RocksDB
  // garbage collect them
  _options.WAL_size_limit_MB = 0;
  _options.memtable_prefix_bloom_size_ratio = 0.2;  // TODO: pick better value?
  // TODO: enable memtable_insert_with_hint_prefix_extractor?
  _options.bloom_locality = 1;

  if (_useThrottle) {
    _listener.reset(new RocksDBThrottle);
    _options.listeners.push_back(_listener);
  }
  
  if (opts->_totalWriteBufferSize > 0) {
    _options.db_write_buffer_size = opts->_totalWriteBufferSize;
  }

  // this is cfFamilies.size() + 2 ... but _option needs to be set before
  //  building cfFamilies
  _options.max_write_buffer_number = 7 + 2;

  // cf options for definitons (dbs, collections, views, ...)
  rocksdb::ColumnFamilyOptions definitionsCF(_options);

  // cf options with fixed 8 byte object id prefix for documents
  rocksdb::ColumnFamilyOptions fixedPrefCF(_options);
  fixedPrefCF.prefix_extractor = std::shared_ptr<rocksdb::SliceTransform const>(
      rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));

  // construct column family options with prefix containing indexed value
  rocksdb::ColumnFamilyOptions dynamicPrefCF(_options);
  dynamicPrefCF.prefix_extractor = std::make_shared<RocksDBPrefixExtractor>();
  // also use hash-search based SST file format
  rocksdb::BlockBasedTableOptions tblo(tableOptions);
  tblo.index_type = rocksdb::BlockBasedTableOptions::IndexType::kHashSearch;
  dynamicPrefCF.table_factory = std::shared_ptr<rocksdb::TableFactory>(
      rocksdb::NewBlockBasedTableFactory(tblo));

  // velocypack based index variants with custom comparator
  rocksdb::ColumnFamilyOptions vpackFixedPrefCF(fixedPrefCF);
  rocksdb::BlockBasedTableOptions tblo2(tableOptions);
  tblo2.filter_policy.reset();  // intentionally no bloom filter here
  vpackFixedPrefCF.table_factory = std::shared_ptr<rocksdb::TableFactory>(
      rocksdb::NewBlockBasedTableFactory(tblo2));
  vpackFixedPrefCF.comparator = _vpackCmp.get();

  // create column families
  std::vector<rocksdb::ColumnFamilyDescriptor> cfFamilies;
  // no prefix families for default column family (Has to be there)
  cfFamilies.emplace_back(rocksdb::kDefaultColumnFamilyName,
                          definitionsCF);                   // 0
  cfFamilies.emplace_back("Documents", fixedPrefCF);        // 1
  cfFamilies.emplace_back("PrimaryIndex", fixedPrefCF);     // 2
  cfFamilies.emplace_back("EdgeIndex", dynamicPrefCF);      // 3
  cfFamilies.emplace_back("VPackIndex", vpackFixedPrefCF);  // 4
  cfFamilies.emplace_back("GeoIndex", fixedPrefCF);         // 5
  cfFamilies.emplace_back("FulltextIndex", fixedPrefCF);    // 6
  // DO NOT FORGET TO DESTROY THE CFs ON CLOSE
  //  Update max_write_buffer_number above if you change number of families used

  std::vector<rocksdb::ColumnFamilyHandle*> cfHandles;
  size_t const numberOfColumnFamilies =
      RocksDBColumnFamily::minNumberOfColumnFamilies;
  bool dbExisted = false;
  {
    rocksdb::Options testOptions;
    testOptions.create_if_missing = false;
    testOptions.create_missing_column_families = false;
    testOptions.env = _options.env;
    std::vector<std::string> existingColumnFamilies;
    rocksdb::Status status = rocksdb::DB::ListColumnFamilies(
        testOptions, _path, &existingColumnFamilies);
    if (!status.ok()) {
      // check if we have found the database directory or not
      Result res = rocksutils::convertStatus(status);
      if (res.errorNumber() != TRI_ERROR_ARANGO_IO_ERROR) {
        // not an I/O error. so we better report the error and abort here
        LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
            << "unable to initialize RocksDB engine: " << status.ToString();
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

      LOG_TOPIC(DEBUG, arangodb::Logger::STARTUP)
          << "found existing column families: " << names;

      for (auto const& it : cfFamilies) {
        auto it2 = std::find(existingColumnFamilies.begin(), existingColumnFamilies.end(), it.name);

        if (it2 == existingColumnFamilies.end()) {
          LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
              << "column family '" << it.name << "' is missing in database"
              << ". if you are upgrading from an earlier alpha or beta version of ArangoDB 3.2, "
              << "it is required to restart with a new database directory and "
                 "re-import data";
          FATAL_ERROR_EXIT();
        }

      }

      if (existingColumnFamilies.size() < numberOfColumnFamilies) {
        LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
            << "unexpected number of column families found in database ("
            << cfHandles.size() << "). "
            << "expecting at least " << numberOfColumnFamilies
            << ". if you are upgrading from an earlier alpha or beta version of ArangoDB 3.2, "
            << "it is required to restart with a new database directory and "
               "re-import data";
        FATAL_ERROR_EXIT();
      }
    }
  }
  

  rocksdb::Status status = rocksdb::TransactionDB::Open(
      _options, transactionOptions, _path, cfFamilies, &cfHandles, &_db);

  if (!status.ok()) {
    std::string error;
    if (status.IsIOError()) {
      error = "; Maybe your filesystem doesn't provide required features? (Cifs? NFS?)";
    }

    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB engine: " << status.ToString() << error;
    FATAL_ERROR_EXIT();
  }
  if (cfFamilies.size() != cfHandles.size()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB column families";
    FATAL_ERROR_EXIT();
  }
  if (cfHandles.size() < numberOfColumnFamilies) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unexpected number of column families found in database. "
        << "got " << cfHandles.size() << ", expecting at least "
        << numberOfColumnFamilies;
    FATAL_ERROR_EXIT();
  }

  // give throttle access to families
  if (_useThrottle) {
    _listener->SetFamilies(cfHandles);
  }

  // set our column families
  RocksDBColumnFamily::_definitions = cfHandles[0];
  RocksDBColumnFamily::_documents = cfHandles[1];
  RocksDBColumnFamily::_primary = cfHandles[2];
  RocksDBColumnFamily::_edge = cfHandles[3];
  RocksDBColumnFamily::_vpack = cfHandles[4];
  RocksDBColumnFamily::_geo = cfHandles[5];
  RocksDBColumnFamily::_fulltext = cfHandles[6];
  RocksDBColumnFamily::_allHandles = cfHandles;
  TRI_ASSERT(RocksDBColumnFamily::_definitions->GetID() == 0);

  // will crash the process if version does not match
  arangodb::rocksdbStartupVersionCheck(_db, dbExisted);

  // only enable logger after RocksDB start
  if (logger != nullptr) {
    logger->enable();
  }

  if (_syncInterval > 0) {
    _syncThread.reset(
        new RocksDBSyncThread(this, std::chrono::milliseconds(_syncInterval)));
    if (!_syncThread->start()) {
      LOG_TOPIC(FATAL, Logger::ENGINES) << "could not start rocksdb sync thread";
      FATAL_ERROR_EXIT();
    }
  }

  TRI_ASSERT(_db != nullptr);
  _settingsManager.reset(new RocksDBSettingsManager(_db));
  _replicationManager.reset(new RocksDBReplicationManager());

  _settingsManager->retrieveInitialValues();

  double const counterSyncSeconds = 2.5;
  _backgroundThread.reset(
      new RocksDBBackgroundThread(this, counterSyncSeconds));
  if (!_backgroundThread->start()) {
    LOG_TOPIC(FATAL, Logger::ENGINES)
        << "could not start rocksdb counter manager";
    FATAL_ERROR_EXIT();
  }

  if (!systemDatabaseExists()) {
    addSystemDatabase();
  }
}

void RocksDBEngine::beginShutdown() {
  if (!isEnabled()) {
    return;
  }

  // block the creation of new replication contexts
  if (_replicationManager != nullptr) {
    _replicationManager->beginShutdown();
  }
}

void RocksDBEngine::stop() {
  if (!isEnabled()) {
    return;
  }

  // in case we missed the beginShutdown somehow, call it again
  replicationManager()->beginShutdown();

  replicationManager()->dropAll();

  if (_backgroundThread) {
    // stop the press
    _backgroundThread->beginShutdown();

    if (_settingsManager) {
      _settingsManager->sync(true);
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
}

void RocksDBEngine::unprepare() {
  if (!isEnabled()) {
    return;
  }

  shutdownRocksDBInstance();
}

std::unique_ptr<TransactionManager> RocksDBEngine::createTransactionManager() {
  return std::unique_ptr<TransactionManager>(new RocksDBTransactionManager());
}

std::unique_ptr<transaction::ContextData> RocksDBEngine::createTransactionContextData() {
  return std::unique_ptr<transaction::ContextData>(
    new RocksDBTransactionContextData()
  );
}

std::unique_ptr<TransactionState> RocksDBEngine::createTransactionState(
    TRI_vocbase_t& vocbase,
    transaction::Options const& options
) {
  return std::unique_ptr<TransactionState>(
    new RocksDBTransactionState(vocbase, TRI_NewTickServer(), options)
  );
}

std::unique_ptr<TransactionCollection> RocksDBEngine::createTransactionCollection(
    TransactionState& state,
    TRI_voc_cid_t cid,
    AccessMode::Type accessType,
    int nestingLevel
) {
  return std::unique_ptr<TransactionCollection>(
    new RocksDBTransactionCollection(&state, cid, accessType, nestingLevel)
  );
}

void RocksDBEngine::addParametersForNewCollection(VPackBuilder& builder,
                                                  VPackSlice info) {
  if (!info.hasKey("objectId")) {
    builder.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
  }
  if (!info.hasKey("cacheEnabled") || !info.get("cacheEnabled").isBool()) {
    builder.add("cacheEnabled", VPackValue(false));
  }
}

// create storage-engine specific collection
std::unique_ptr<PhysicalCollection> RocksDBEngine::createPhysicalCollection(
    LogicalCollection& collection,
    velocypack::Slice const& info
) {
  return std::unique_ptr<PhysicalCollection>(
    new RocksDBCollection(collection, info)
  );
}

// inventory functionality
// -----------------------

void RocksDBEngine::getDatabases(arangodb::velocypack::Builder& result) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "getting existing databases";

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, RocksDBColumnFamily::definitions()));
  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::Database);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    auto slice = VPackSlice(iter->value().data());

    //// check format id
    VPackSlice idSlice = slice.get("id");
    if (!idSlice.isString()) {
      LOG_TOPIC(ERR, arangodb::Logger::STARTUP)
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
      LOG_TOPIC(DEBUG, arangodb::Logger::STARTUP) << "found dropped database "
                                                  << id;

      dropDatabase(id);
      continue;
    }

    // name
    VPackSlice nameSlice = slice.get("name");
    if (!nameSlice.isString()) {
      LOG_TOPIC(ERR, arangodb::Logger::STARTUP)
          << "found invalid database declaration with non-string name: "
          << slice.toJson();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    result.add(slice);
  }
  result.close();
}

void RocksDBEngine::getCollectionInfo(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    arangodb::velocypack::Builder& builder,
    bool includeIndexes,
    TRI_voc_tick_t maxTick
) {
  builder.openObject();

  // read collection info from database
  RocksDBKey key;

  key.constructCollection(vocbase.id(), cid);

  rocksdb::PinnableSlice value;
  rocksdb::ReadOptions options;
  rocksdb::Status res = _db->Get(options, RocksDBColumnFamily::definitions(),
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

int RocksDBEngine::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result,
    bool wasCleanShutdown,
    bool isUpgrade
) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, RocksDBColumnFamily::definitions()));

  result.openArray();

  auto rSlice = rocksDBSlice(RocksDBEntryType::Collection);

  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase.id() != RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice = VPackSlice(iter->value().data());

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(
          slice, StaticStrings::DataSourceDeleted, false
        )
       ) {
      continue;
    }

    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::getViews(
    TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result
) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, RocksDBColumnFamily::definitions()));

  result.openArray();

  auto bounds = RocksDBKeyBounds::DatabaseViews(vocbase.id());

  for (iter->Seek(bounds.start());
       iter->Valid() && iter->key().compare(bounds.end()) < 0; iter->Next()) {
    auto slice = VPackSlice(iter->value().data());

    LOG_TOPIC(TRACE, Logger::VIEWS) << "got view slice: " << slice.toJson();

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(
          slice, StaticStrings::DataSourceDeleted, false
        )
       ) {
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

VPackBuilder RocksDBEngine::getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase,
    int& status
) {
  RocksDBKey key;

  key.constructReplicationApplierConfig(vocbase.id());

  return getReplicationApplierConfiguration(key, status);
}

VPackBuilder RocksDBEngine::getReplicationApplierConfiguration(int& status) {
  RocksDBKey key;
  key.constructReplicationApplierConfig(databaseIdForGlobalApplier);
  return getReplicationApplierConfiguration(key, status);
}

VPackBuilder RocksDBEngine::getReplicationApplierConfiguration(
    RocksDBKey const& key, int& status) {
  rocksdb::PinnableSlice value;

  auto db = rocksutils::globalRocksDB();
  auto opts = rocksdb::ReadOptions();
  auto s =
      db->Get(opts, RocksDBColumnFamily::definitions(), key.string(), &value);
  if (!s.ok()) {
    status = TRI_ERROR_FILE_NOT_FOUND;
    return arangodb::velocypack::Builder();
  }

  status = TRI_ERROR_NO_ERROR;
  VPackBuilder builder;
  builder.add(RocksDBValue::data(value));
  return builder;
}

int RocksDBEngine::removeReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase
) {
  RocksDBKey key;

  key.constructReplicationApplierConfig(vocbase.id());

  return removeReplicationApplierConfiguration(key);
}

int RocksDBEngine::removeReplicationApplierConfiguration() {
  RocksDBKey key;
  key.constructReplicationApplierConfig(databaseIdForGlobalApplier);
  return removeReplicationApplierConfiguration(key);
}

int RocksDBEngine::removeReplicationApplierConfiguration(
    RocksDBKey const& key) {
  auto status = rocksutils::globalRocksDBRemove(
      RocksDBColumnFamily::definitions(), key.string());
  if (!status.ok()) {
    return status.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::saveReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase,
    velocypack::Slice slice,
    bool doSync
) {
  RocksDBKey key;

  key.constructReplicationApplierConfig(vocbase.id());

  return saveReplicationApplierConfiguration(key, slice, doSync);
}

int RocksDBEngine::saveReplicationApplierConfiguration(
    arangodb::velocypack::Slice slice, bool doSync) {
  RocksDBKey key;
  key.constructReplicationApplierConfig(databaseIdForGlobalApplier);
  return saveReplicationApplierConfiguration(key, slice, doSync);
}

int RocksDBEngine::saveReplicationApplierConfiguration(
    RocksDBKey const& key, arangodb::velocypack::Slice slice, bool doSync) {
  auto value = RocksDBValue::ReplicationApplierConfig(slice);

  auto status = rocksutils::globalRocksDBPut(RocksDBColumnFamily::definitions(),
                                             key.string(), value.string());
  if (!status.ok()) {
    return status.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

// database, collection and index management
// -----------------------------------------

std::unique_ptr<TRI_vocbase_t> RocksDBEngine::openDatabase(
    arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  VPackSlice idSlice = args.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
      basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = args.get("name").copyString();

  status = TRI_ERROR_NO_ERROR;

  return openExistingDatabase(id, name, true, isUpgrade);
}

std::unique_ptr<TRI_vocbase_t> RocksDBEngine::createDatabase(
    TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) {
  status = TRI_ERROR_NO_ERROR;

  return std::make_unique<TRI_vocbase_t>(
    TRI_VOCBASE_TYPE_NORMAL, id, args.get("name").copyString()
  );
}

int RocksDBEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                             VPackSlice const& slice) {
  Result res =
      writeDatabaseMarker(id, slice, RocksDBLogValue::DatabaseCreate(id));
  return res.errorNumber();
}

Result RocksDBEngine::writeDatabaseMarker(TRI_voc_tick_t id,
                                          VPackSlice const& slice,
                                          RocksDBLogValue&& logValue) {
  RocksDBKey key;
  key.constructDatabase(id);
  auto value = RocksDBValue::Database(slice);
  rocksdb::WriteOptions wo;

  // Write marker + key into RocksDB inside one batch
  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Put(RocksDBColumnFamily::definitions(), key.string(), value.string());
  rocksdb::Status res = _db->GetRootDB()->Write(wo, &batch);
  return rocksutils::convertStatus(res);
}

int RocksDBEngine::writeCreateCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t cid,
                                               VPackSlice const& slice,
                                               RocksDBLogValue&& logValue) {
  
  rocksdb::DB* db = _db->GetRootDB();
  
  RocksDBKey key;
  key.constructCollection(databaseId, cid);
  auto value = RocksDBValue::Collection(slice);
  
  rocksdb::WriteOptions wo;

  // Write marker + key into RocksDB inside one batch
  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Put(RocksDBColumnFamily::definitions(), key.string(), value.string());
  rocksdb::Status res = db->Write(wo, &batch);
  
  auto result = rocksutils::convertStatus(res);
  return result.errorNumber();
}

void RocksDBEngine::prepareDropDatabase(
    TRI_vocbase_t& vocbase,
    bool useWriteMarker,
    int& status
) {
  VPackBuilder builder;

  builder.openObject();
  builder.add("id", velocypack::Value(std::to_string(vocbase.id())));
  builder.add("name", velocypack::Value(vocbase.name()));
  builder.add("deleted", VPackValue(true));
  builder.close();

  auto log = RocksDBLogValue::DatabaseDrop(vocbase.id());
  auto res = writeDatabaseMarker(vocbase.id(), builder.slice(), std::move(log));

  status = res.errorNumber();
}

Result RocksDBEngine::dropDatabase(TRI_vocbase_t& database) {
  replicationManager()->drop(&database);

  return dropDatabase(database.id());
}

void RocksDBEngine::waitUntilDeletion(TRI_voc_tick_t /* id */, bool /* force */,
                                      int& status) {
  // can delete databases instantly
  status = TRI_ERROR_NO_ERROR;
}

// wal in recovery
bool RocksDBEngine::inRecovery() {
  return RocksDBRecoveryManager::instance()->inRecovery();
}

void RocksDBEngine::recoveryDone(TRI_vocbase_t& vocbase) {
}

std::string RocksDBEngine::createCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    LogicalCollection const& collection
) {
  auto builder = collection.toVelocyPackIgnore(
    {"path", "statusString"}, /*translate cid*/ true, /*for persistence*/ true
  );

  TRI_ASSERT(cid != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(cid));

  int res = writeCreateCollectionMarker(
    vocbase.id(),
    cid,
    builder.slice(),
    RocksDBLogValue::CollectionCreate(vocbase.id(), cid)
  );

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* rcoll = toRocksDBCollection(collection.getPhysical());

  TRI_ASSERT(rcoll->numberDocuments() == 0);
#endif

  return std::string();  // no need to return a path
}

arangodb::Result RocksDBEngine::persistCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection const& collection
) {
  return {};
}

arangodb::Result RocksDBEngine::dropCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection& collection
) {
  auto* coll = toRocksDBCollection(collection);
  bool const prefixSameAsStart = true;
  bool const useRangeDelete = coll->numberDocuments() >= 32 * 1024;

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

  TRI_ASSERT(collection.status() == TRI_VOC_COL_STATUS_DELETED);

  // Prepare collection remove batch
  rocksdb::WriteBatch batch;
  RocksDBLogValue logValue = RocksDBLogValue::CollectionDrop(
    vocbase.id(), collection.id(), StringRef(collection.guid())
  );
  batch.PutLogData(logValue.slice());

  RocksDBKey key;
  key.constructCollection(vocbase.id(), collection.id());
  batch.Delete(RocksDBColumnFamily::definitions(), key.string());

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
  Result res = RocksDBCollectionMeta::deleteCollectionMeta(db, coll->objectId());
  if (res.fail()) {
    LOG_TOPIC(ERR, Logger::ENGINES) << "error removing collection meta-data: "
      << res.errorMessage(); // continue regardless
  }
  
  // remove from map
  {
    WRITE_LOCKER(guard, _mapLock);
    _collectionMap.erase(coll->objectId());
  }
  
  // delete indexes, RocksDBIndex::drop() has its own check
  std::vector<std::shared_ptr<Index>> vecShardIndex = coll->getIndexes();
  TRI_ASSERT(!vecShardIndex.empty());

  for (auto& index : vecShardIndex) {
    RocksDBIndex* ridx = static_cast<RocksDBIndex*>(index.get());
    res = RocksDBCollectionMeta::deleteIndexEstimate(db, ridx->objectId());
    if (res.fail()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "could not delete index estimate: "
      << res.errorMessage();
    }

    auto dropRes = index->drop().errorNumber();

    if (dropRes != TRI_ERROR_NO_ERROR) {
      // We try to remove all indexed values.
      // If it does not work they cannot be accessed any more and leaked.
      // User View remains consistent.
      LOG_TOPIC(ERR, Logger::ENGINES) << "unable to drop index: "
      << TRI_errno_string(dropRes);
//      return TRI_ERROR_NO_ERROR;
    }
  }

  // delete documents
  RocksDBKeyBounds bounds =
      RocksDBKeyBounds::CollectionDocuments(coll->objectId());
  auto result = rocksutils::removeLargeRange(db, bounds, prefixSameAsStart, useRangeDelete);

  if (result.fail()) {
    // We try to remove all documents.
    // If it does not work they cannot be accessed any more and leaked.
    // User View remains consistent.
    return TRI_ERROR_NO_ERROR;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // check if documents have been deleted
  size_t numDocs =
      rocksutils::countKeyRange(rocksutils::globalRocksDB(), bounds, true);

  if (numDocs > 0) {
    std::string errorMsg(
        "deletion check in collection drop failed - not all documents in the "
        "index have been deleted. remaining: ");
    errorMsg.append(std::to_string(numDocs));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif

  // run compaction for data only if collection contained a considerable
  // amount of documents. otherwise don't run compaction, because it will
  // slow things down a lot, especially during tests that create/drop LOTS
  // of collections
  if (useRangeDelete) {
    coll->compact();
  }

  // if we get here all documents / indexes are gone.
  // We have no data garbage left.
  return TRI_ERROR_NO_ERROR;
}

void RocksDBEngine::destroyCollection(
    TRI_vocbase_t& /*vocbase*/,
    LogicalCollection& /*collection*/
) {
  // not required
}

void RocksDBEngine::changeCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    LogicalCollection const& collection,
    bool doSync
) {
  auto builder = collection.toVelocyPackIgnore(
    {"path", "statusString"}, /*translate cid*/ true, /*for persistence*/ true
  );
  int res = writeCreateCollectionMarker(
    vocbase.id(),
    id,
    builder.slice(),
    RocksDBLogValue::CollectionChange(vocbase.id(), id)
  );

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

arangodb::Result RocksDBEngine::renameCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection const& collection,
    std::string const& oldName
) {
  auto builder =
    collection.toVelocyPackIgnore({"path", "statusString"}, true, true);
  int res = writeCreateCollectionMarker(
    vocbase.id(),
    collection.id(),
    builder.slice(),
    RocksDBLogValue::CollectionRename(
      vocbase.id(), collection.id(), StringRef(oldName)
    )
  );

  return arangodb::Result(res);
}

void RocksDBEngine::unloadCollection(
    TRI_vocbase_t& /*vocbase*/,
    LogicalCollection& collection
) {
  collection.setStatus(TRI_VOC_COL_STATUS_UNLOADED);
}

Result RocksDBEngine::createView(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalView const& view
) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "RocksDBEngine::createView";
#endif
  rocksdb::WriteBatch batch;
  rocksdb::WriteOptions wo;

  RocksDBKey key;
  key.constructView(vocbase.id(), id);
  RocksDBLogValue logValue = RocksDBLogValue::ViewCreate(vocbase.id(), id);

  VPackBuilder props;

  props.openObject();
    view.properties(props, true, true);
  props.close();

  RocksDBValue const value = RocksDBValue::View(props.slice());

  // Write marker + key into RocksDB inside one batch
  batch.PutLogData(logValue.slice());
  batch.Put(RocksDBColumnFamily::definitions(), key.string(), value.string());

  auto res = _db->Write(wo, &batch);

  LOG_TOPIC_IF(TRACE, Logger::VIEWS, !res.ok())
      << "could not create view: " << res.ToString();

  return rocksutils::convertStatus(res);
}

arangodb::Result RocksDBEngine::dropView(
    TRI_vocbase_t& vocbase,
    LogicalView& view
) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "RocksDBEngine::dropView";
#endif
  VPackBuilder builder;

  builder.openObject();
    view.properties(builder, true, true);
  builder.close();

  auto logValue =
    RocksDBLogValue::ViewDrop(vocbase.id(), view.id(),
                              StringRef(view.guid()));
  RocksDBKey key;
  key.constructView(vocbase.id(), view.id());

  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Delete(RocksDBColumnFamily::definitions(), key.string());

  rocksdb::WriteOptions wo;
  auto res = _db->GetRootDB()->Write(wo, &batch);
  LOG_TOPIC_IF(TRACE, Logger::VIEWS, !res.ok())
      << "could not create view: " << res.ToString();
  return rocksutils::convertStatus(res);
}

void RocksDBEngine::destroyView(
    TRI_vocbase_t& /*vocbase*/,
    LogicalView& /*view*/
) noexcept {
  // nothing to do here
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "RocksDBEngine::destroyView";
#endif
}

Result RocksDBEngine::changeView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    bool /*doSync*/
) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "RocksDBEngine::changeView";
#endif
  if (inRecovery()) {
    // nothing to do
    return {};
  }

  RocksDBKey key;

  key.constructView(vocbase.id(), view.id());

  VPackBuilder infoBuilder;

  infoBuilder.openObject();
    view.properties(infoBuilder, true, true);
  infoBuilder.close();

  RocksDBLogValue log = RocksDBLogValue::ViewChange(vocbase.id(), view.id());
  RocksDBValue const value = RocksDBValue::View(infoBuilder.slice());

  rocksdb::WriteBatch batch;
  rocksdb::WriteOptions wo;  // TODO: check which options would make sense
  rocksdb::Status s;

  s = batch.PutLogData(log.slice());

  if (!s.ok()) {
    LOG_TOPIC(TRACE, Logger::VIEWS)
        << "failed to write change view marker " << s.ToString();
    return rocksutils::convertStatus(s);
  }

  s = batch.Put(RocksDBColumnFamily::definitions(),
            key.string(), value.string());

  if (!s.ok()) {
    LOG_TOPIC(TRACE, Logger::VIEWS)
        << "failed to write change view marker " << s.ToString();
    return rocksutils::convertStatus(s);
  }
  auto db = rocksutils::globalRocksDB();
  auto res = db->Write(wo, &batch);
  LOG_TOPIC_IF(TRACE, Logger::VIEWS, !res.ok())
      << "could not change view: " << res.ToString();
  return rocksutils::convertStatus(res);
}

void RocksDBEngine::signalCleanup(TRI_vocbase_t& vocbase) {
  // nothing to do here
}

int RocksDBEngine::shutdownDatabase(TRI_vocbase_t& vocbase) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief Add engine-specific optimizer rules
void RocksDBEngine::addOptimizerRules() {
  RocksDBOptimizerRules::registerResources();
}

/// @brief Add engine-specific V8 functions
void RocksDBEngine::addV8Functions() {
  // there are no specific V8 functions here
  RocksDBV8Functions::registerResources();
}

/// @brief Add engine-specific REST handlers
void RocksDBEngine::addRestHandlers(rest::RestHandlerFactory& handlerFactory) {
  RocksDBRestHandlers::registerResources(&handlerFactory);
}

void RocksDBEngine::addCollectionMapping(uint64_t objectId, TRI_voc_tick_t did,
                                         TRI_voc_cid_t cid) {
  if (objectId != 0) {
    WRITE_LOCKER(guard, _mapLock);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto it = _collectionMap.find(objectId);
    if (it != _collectionMap.end()) {
      TRI_ASSERT(it->second.first == did);
      TRI_ASSERT(it->second.second == cid);
    }
#endif
    _collectionMap[objectId] = std::make_pair(did, cid);
  }
}
  
std::vector<std::pair<TRI_voc_tick_t, TRI_voc_cid_t>> RocksDBEngine::collectionMappings() const {
  std::vector<std::pair<TRI_voc_tick_t, TRI_voc_cid_t>> res;
  READ_LOCKER(guard, _mapLock);
  for (auto const& it : _collectionMap) {
    res.emplace_back(it.second.first, it.second.second);
  }
  return res;
}

void RocksDBEngine::addIndexMapping(uint64_t objectId, TRI_voc_tick_t did,
                                    TRI_voc_cid_t cid, TRI_idx_iid_t iid) {
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
    return {0, 0};
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

Result RocksDBEngine::flushWal(bool waitForSync, bool waitForCollector,
                               bool /*writeShutdownFile*/) {
  if (_syncThread) {
    // _syncThread may be a nullptr, in case automatic syncing is turned off
    _syncThread->syncWal();
  }

  if (waitForCollector) {
    rocksdb::FlushOptions flushOptions;
    flushOptions.wait = waitForSync;

    for (auto cf : RocksDBColumnFamily::_allHandles) {
      rocksdb::Status status = _db->GetBaseDB()->Flush(flushOptions, cf);
      if (!status.ok()) {
        return rocksutils::convertStatus(status);
      }
    }
  }
  return Result();
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

void RocksDBEngine::determinePrunableWalFiles(TRI_voc_tick_t minTickExternal) {
  WRITE_LOCKER(lock, _walFileLock);
  rocksdb::VectorLogPtr files;

  TRI_voc_tick_t minTickToKeep = std::min(_releasedTick, minTickExternal);

  auto status = _db->GetSortedWalFiles(files);
  if (!status.ok()) {
    LOG_TOPIC(INFO, Logger::ENGINES) << "could not get WAL files "
      << status.ToString();
    return;
  }

  size_t lastLess = files.size();
  for (size_t current = 0; current < files.size(); current++) {
    auto f = files[current].get();
    if (f->StartSequence() < minTickToKeep) {
      lastLess = current;
    } else {
      break;
    }
  }

  // insert all candidate files into the map of deletable files
  if (lastLess > 0 && lastLess < files.size()) {
    for (size_t current = 0; current < lastLess; current++) {
      auto const& f = files[current].get();
      if (f->Type() == rocksdb::WalFileType::kArchivedLogFile) {
        if (_prunableWalFiles.find(f->PathName()) == _prunableWalFiles.end()) {
          LOG_TOPIC(DEBUG, Logger::ENGINES)
              << "RocksDB WAL file '" << f->PathName()
              << "' with start sequence " << f->StartSequence()
              << " added to prunable list";
          _prunableWalFiles.emplace(f->PathName(),
                                    TRI_microtime() + _pruneWaitTime);
        }
      }
    }
  }
}

void RocksDBEngine::pruneWalFiles() {
  WRITE_LOCKER(lock, _walFileLock);

  // go through the map of WAL files that we have already and check if they are
  // "expired"
  for (auto it = _prunableWalFiles.begin(); it != _prunableWalFiles.end();
       /* no hoisting */) {
    // check if WAL file is expired
    if ((*it).second < TRI_microtime()) {
      LOG_TOPIC(DEBUG, Logger::ENGINES)
        << "deleting RocksDB WAL file '" << (*it).first << "'";
      auto s = _db->DeleteFile((*it).first);
      // apparently there is a case where a file was already deleted
      // but is still in _prunableWalFiles. In this case we get an invalid
      // argument response.
      if (s.ok() || s.IsInvalidArgument()) {
        it = _prunableWalFiles.erase(it);
        continue;
      }
    }
    // cannot delete this file yet... must forward iterator to prevent an
    // endless loop
    ++it;
  }
}

Result RocksDBEngine::dropDatabase(TRI_voc_tick_t id) {
  using namespace rocksutils;
  arangodb::Result res;
  rocksdb::WriteOptions wo;
  rocksdb::DB* db = _db->GetRootDB();

  // remove view definitions
  res = rocksutils::removeLargeRange(db, RocksDBKeyBounds::DatabaseViews(id),
                                     true, /*rangeDel*/false);
  if (res.fail()) {
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  size_t numDocsLeft = 0;
#endif

  // remove collections
  auto dbBounds = RocksDBKeyBounds::DatabaseCollections(id);
  iterateBounds(dbBounds, [&](rocksdb::Iterator* it) {
    RocksDBKey key(it->key());
    RocksDBValue value(RocksDBEntryType::Collection, it->value());

    uint64_t const objectId =
    basics::VelocyPackHelper::stringUInt64(value.slice(), "objectId");
    
    auto const cnt = RocksDBCollectionMeta::loadCollectionCount(_db, objectId);
    uint64_t const numberDocuments = cnt._added - cnt._removed;
    bool const useRangeDelete = numberDocuments >= 32 * 1024;

    // remove indexes
    VPackSlice indexes = value.slice().get("indexes");
    if (indexes.isArray()) {
      for (auto const& it : VPackArrayIterator(indexes)) {
        // delete index documents
        uint64_t objectId =
            basics::VelocyPackHelper::stringUInt64(it, "objectId");
        res = RocksDBCollectionMeta::deleteIndexEstimate(db, objectId);
        if (res.fail()) {
          return;
        }
        
        TRI_ASSERT(it.get(StaticStrings::IndexType).isString());
        auto type = Index::type(it.get(StaticStrings::IndexType).copyString());
        bool unique = basics::VelocyPackHelper::getBooleanValue(
          it, StaticStrings::IndexUnique, false
        );

        RocksDBKeyBounds bounds =
            RocksDBIndex::getBounds(type, objectId, unique);
        // edge index drop fails otherwise
        bool const prefixSameAsStart = type != Index::TRI_IDX_TYPE_EDGE_INDEX;
        res = rocksutils::removeLargeRange(db, bounds, prefixSameAsStart, useRangeDelete);
        if (res.fail()) {
          return;
        }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        // check if documents have been deleted
        numDocsLeft += rocksutils::countKeyRange(db, bounds, prefixSameAsStart);
#endif
      }
    }

    // delete documents
    RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId);
    res = rocksutils::removeLargeRange(db, bounds, true, useRangeDelete);
    if (res.fail()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "error deleting collection documents: '"
        << res.errorMessage() << "'";
      return;
    }
    // delete collection meta-data
    res = RocksDBCollectionMeta::deleteCollectionMeta(db, objectId);
    if (res.fail()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "error deleting collection metadata: '"
      << res.errorMessage() << "'";
      return;
    }
    // remove collection entry
    rocksdb::Status s = db->Delete(wo, RocksDBColumnFamily::definitions(), value.string());
    if (!s.ok()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "error deleting collection definition: " << s.ToString();
      return;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // check if documents have been deleted
    numDocsLeft +=
        rocksutils::countKeyRange(db, bounds, true);
#endif
  });

  if (res.fail()) {
    return res;
  }

  // remove database meta-data
  RocksDBKey key;
  key.constructDatabase(id);
  rocksdb::Status s = db->Delete(wo, RocksDBColumnFamily::definitions(), key.string());
  if (!s.ok()) {
    LOG_TOPIC(WARN, Logger::ENGINES) << "error deleting database definition: " << s.ToString();
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
    if (item.get("name").copyString() == StaticStrings::SystemDatabase) {
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
  builder.add("id", VPackValue(std::to_string(id)));
  builder.add("name", VPackValue(StaticStrings::SystemDatabase));
  builder.add("deleted", VPackValue(false));
  builder.close();

  RocksDBLogValue log = RocksDBLogValue::DatabaseCreate(id);
  Result res = writeDatabaseMarker(id, builder.slice(), std::move(log));
  if (res.fail()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to write database marker: " << res.errorMessage();
    FATAL_ERROR_EXIT();
  }
}

/// @brief open an existing database. internal function
std::unique_ptr<TRI_vocbase_t> RocksDBEngine::openExistingDatabase(
    TRI_voc_tick_t id,
    std::string const& name,
    bool wasCleanShutdown,
    bool isUpgrade
) {
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);

  // scan the database path for views
  try {
    VPackBuilder builder;
    int res = getViews(*vocbase, builder);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice const slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a view that is still active

      TRI_ASSERT(!it.get("id").isNone());

      LogicalView::ptr view;
      auto res = LogicalView::instantiate(view, *vocbase, it);

      if (!res.ok() || !view) {
        auto const message = "failed to instantiate view '" + name + "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, message);
      }

      StorageEngine::registerView(*vocbase, view);

      view->open();
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "error while opening database: " << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "error while opening database: unknown exception";
    throw;
  }

  // scan the database path for collections
  try {
    VPackBuilder builder;
    int res =
      getCollectionsAndIndexes(*vocbase, builder, wasCleanShutdown, isUpgrade);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (VPackSlice it : VPackArrayIterator(slice)) {
      // we found a collection that is still active
      TRI_ASSERT(!it.get("id").isNone() || !it.get("cid").isNone());
      auto uniqCol =
        std::make_shared<arangodb::LogicalCollection>(*vocbase, it, false);
      auto collection = uniqCol.get();
      TRI_ASSERT(collection != nullptr);

      auto phy = static_cast<RocksDBCollection*>(collection->getPhysical());
      TRI_ASSERT(phy != nullptr);
      phy->meta().deserializeMeta(_db, *collection);
      
      StorageEngine::registerCollection(*vocbase, uniqCol);
      LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
          << "added document collection '" << collection->name() << "'";
    }

    return vocbase;
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "error while opening database: " << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "error while opening database: unknown exception";
    throw;
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

  // add column family properties
  auto addCf = [&](std::string const& name, rocksdb::ColumnFamilyHandle* c) {
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

    _db->GetApproximateSizes(
        c, &r, 1, &out,
        static_cast<uint8_t>(
            rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
            rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));

    builder.add("memory", VPackValue(out));
    builder.close();
  };

  builder.openObject();
  for (int i = 0; i < _options.num_levels; ++i) {
    addInt(rocksdb::DB::Properties::kNumFilesAtLevelPrefix + std::to_string(i));
    addInt(rocksdb::DB::Properties::kCompressionRatioAtLevelPrefix + std::to_string(i));
  }
  addInt(rocksdb::DB::Properties::kNumImmutableMemTable);
  addInt(rocksdb::DB::Properties::kNumImmutableMemTableFlushed);
  addInt(rocksdb::DB::Properties::kMemTableFlushPending);
  addInt(rocksdb::DB::Properties::kCompactionPending);
  addInt(rocksdb::DB::Properties::kBackgroundErrors);
  addInt(rocksdb::DB::Properties::kCurSizeActiveMemTable);
  addInt(rocksdb::DB::Properties::kCurSizeAllMemTables);
  addInt(rocksdb::DB::Properties::kSizeAllMemTables);
  addInt(rocksdb::DB::Properties::kNumEntriesActiveMemTable);
  addInt(rocksdb::DB::Properties::kNumEntriesImmMemTables);
  addInt(rocksdb::DB::Properties::kNumDeletesActiveMemTable);
  addInt(rocksdb::DB::Properties::kNumDeletesImmMemTables);
  addInt(rocksdb::DB::Properties::kEstimateNumKeys);
  addInt(rocksdb::DB::Properties::kEstimateTableReadersMem);
  addInt(rocksdb::DB::Properties::kNumSnapshots);
  addInt(rocksdb::DB::Properties::kOldestSnapshotTime);
  addInt(rocksdb::DB::Properties::kNumLiveVersions);
  addInt(rocksdb::DB::Properties::kMinLogNumberToKeep);
  addInt(rocksdb::DB::Properties::kEstimateLiveDataSize);
  addInt(rocksdb::DB::Properties::kLiveSstFilesSize);
  addStr(rocksdb::DB::Properties::kDBStats);
  addStr(rocksdb::DB::Properties::kSSTables);
  addInt(rocksdb::DB::Properties::kNumRunningCompactions);
  addInt(rocksdb::DB::Properties::kNumRunningFlushes);
  addInt(rocksdb::DB::Properties::kIsFileDeletionsEnabled);
  addInt(rocksdb::DB::Properties::kEstimatePendingCompactionBytes);
  addInt(rocksdb::DB::Properties::kBaseLevel);
  addInt(rocksdb::DB::Properties::kBlockCacheCapacity);
  addInt(rocksdb::DB::Properties::kBlockCacheUsage);
  addInt(rocksdb::DB::Properties::kBlockCachePinnedUsage);
  addInt(rocksdb::DB::Properties::kTotalSstFilesSize);
  addInt(rocksdb::DB::Properties::kActualDelayedWriteRate);
  addInt(rocksdb::DB::Properties::kIsWriteStopped);

  if (_options.statistics) {
    for (auto const& stat : rocksdb::TickersNameMap) {
      builder.add(stat.second,
                  VPackValue(_options.statistics->getTickerCount(stat.first)));
    }
  }

  cache::Manager* manager = CacheManagerFeature::MANAGER;
  if (manager != nullptr) {
    // cache turned on
    auto rates = manager->globalHitRates();
    builder.add("cache.limit", VPackValue(manager->globalLimit()));
    builder.add("cache.allocated", VPackValue(manager->globalAllocation()));
    // handle NaN
    builder.add("cache.hit-rate-lifetime", VPackValue(rates.first >= 0.0 ? rates.first : 0.0));
    builder.add("cache.hit-rate-recent", VPackValue(rates.second >= 0.0 ? rates.second : 0.0));
  } else {
    // cache turned off
    builder.add("cache.limit", VPackValue(0));
    builder.add("cache.allocated", VPackValue(0));
    // handle NaN
    builder.add("cache.hit-rate-lifetime", VPackValue(0));
    builder.add("cache.hit-rate-recent", VPackValue(0));
  }

  // print column family statistics
  builder.add("columnFamilies", VPackValue(VPackValueType::Object));
  addCf("definitions", RocksDBColumnFamily::definitions());
  addCf("documents", RocksDBColumnFamily::documents());
  addCf("primary", RocksDBColumnFamily::primary());
  addCf("edge", RocksDBColumnFamily::edge());
  addCf("vpack", RocksDBColumnFamily::vpack());
  addCf("geo", RocksDBColumnFamily::geo());
  addCf("fulltext", RocksDBColumnFamily::fulltext());
  builder.close();

  builder.close();
}

Result RocksDBEngine::handleSyncKeys(
    DatabaseInitialSyncer& syncer,
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
  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(lastTick)));
  builder.add("lastUncommittedLogTick", VPackValue(std::to_string(lastTick)));
  builder.add("totalEvents",
              VPackValue(lastTick));  // s.numEvents + s.numEventsSync
  builder.add("time", VPackValue(utilities::timeString()));
  builder.close();

  // "server" part
  builder.add("server", VPackValue(VPackValueType::Object));  // open
  builder.add("version", VPackValue(ARANGODB_VERSION));
  builder.add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
  builder.add("engine", VPackValue(EngineName)); // "rocksdb"
  builder.close();

  // "clients" part
  builder.add("clients", VPackValue(VPackValueType::Array));  // open
  if (vocbase != nullptr) {                                   // add clients
    auto allClients = vocbase->getReplicationClients();
    for (auto& it : allClients) {
      // One client
      builder.add(VPackValue(VPackValueType::Object));
      builder.add("serverId", VPackValue(std::to_string(std::get<0>(it))));

      char buffer[21];
      TRI_GetTimeStampReplication(std::get<1>(it), &buffer[0], sizeof(buffer));
      builder.add("time", VPackValue(buffer));

      TRI_GetTimeStampReplication(std::get<2>(it), &buffer[0], sizeof(buffer));
      builder.add("expires", VPackValue(buffer));

      builder.add("lastServedTick",
                  VPackValue(std::to_string(std::get<3>(it))));

      builder.close();
    }
  }
  builder.close();  // clients

  builder.close();  // base

  return Result{};
}

Result RocksDBEngine::createTickRanges(VPackBuilder& builder) {
  rocksdb::TransactionDB* tdb = rocksutils::globalRocksDB();
  rocksdb::VectorLogPtr walFiles;
  rocksdb::Status s = tdb->GetSortedWalFiles(walFiles);
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
      max = tdb->GetLatestSequenceNumber();
    }
    builder.add("tickMax", VPackValue(std::to_string(max)));
    builder.close();
  }
  builder.close();
  return Result{};
}

Result RocksDBEngine::firstTick(uint64_t& tick) {
  Result res{};
  rocksdb::TransactionDB* tdb = rocksutils::globalRocksDB();
  rocksdb::VectorLogPtr walFiles;
  rocksdb::Status s = tdb->GetSortedWalFiles(walFiles);

  if (!s.ok()) {
    res = rocksutils::convertStatus(s);
    return res;
  }
  // read minium possible tick
  if (!walFiles.empty()) {
    tick = walFiles[0]->StartSequence();
  }
  return res;
}

Result RocksDBEngine::lastLogger(
    TRI_vocbase_t& vocbase,
    std::shared_ptr<transaction::Context> transactionContext,
    uint64_t tickStart, uint64_t tickEnd,
    std::shared_ptr<VPackBuilder>& builderSPtr) {
  bool includeSystem = true;
  size_t chunkSize = 32 * 1024 * 1024;  // TODO: determine good default value?

  // construct vocbase with proper handler
  auto builder =
      std::make_unique<VPackBuilder>(transactionContext->getVPackOptions());

  builder->openArray();

  RocksDBReplicationResult rep = rocksutils::tailWal(
    &vocbase, tickStart, tickEnd, chunkSize, includeSystem, 0, *builder
  );

  builder->close();
  builderSPtr = std::move(builder);

  return std::move(rep);
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
  return static_cast<TRI_voc_tick_t>(_db->GetLatestSequenceNumber());
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

bool RocksDBEngine::canUseRangeDeleteInWal() const {
  return ServerState::instance()->isSingleServer();
}

}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
