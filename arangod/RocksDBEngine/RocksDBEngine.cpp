////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Rest/Version.h"
#include "RocksDBEngine/RocksDBAqlFunctions.h"
#include "RocksDBEngine/RocksDBBackgroundThread.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBIncrementalSync.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBIndexFactory.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "RocksDBEngine/RocksDBRestHandlers.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionContextData.h"
#include "RocksDBEngine/RocksDBTransactionManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBV8Functions.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBView.h"
#include "Transaction/Options.h"
#include "VocBase/replication-applier.h"
#include "VocBase/ticks.h"

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>
#include <rocksdb/transaction_log.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

std::string const RocksDBEngine::EngineName("rocksdb");
std::string const RocksDBEngine::FeatureName("RocksDBEngine");

rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_documents(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_primary(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_edge(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_geo(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_fulltext(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_other(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_index(nullptr);
rocksdb::ColumnFamilyHandle* RocksDBColumnFamily::_uniqueIndex(nullptr);
std::vector<rocksdb::ColumnFamilyHandle*> RocksDBColumnFamily::_allHandles;

// create the storage engine
RocksDBEngine::RocksDBEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, EngineName, FeatureName, new RocksDBIndexFactory()),
      _db(nullptr),
      _vpackCmp(new RocksDBComparator()),
      _maxTransactionSize(transaction::Options::defaultMaxTransactionSize),
      _intermediateCommitSize(transaction::Options::defaultIntermediateCommitSize),
      _intermediateCommitCount(transaction::Options::defaultIntermediateCommitCount),
      _pruneWaitTime(10.0) {
  // inherits order from StorageEngine but requires "RocksDBOption" that is used
  // to configure this engine and the MMFiles PersistentIndexFeature
  startsAfter("RocksDBOption");
}

RocksDBEngine::~RocksDBEngine() { delete _db; }

// inherited from ApplicationFeature
// ---------------------------------

// add the storage engine's specifc options to the global list of options
void RocksDBEngine::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("rocksdb", "RocksDB engine specific configuration");

  // control transaction size for RocksDB engine
  options->addOption("--rocksdb.max-transaction-size",
                     "transaction size limit (in bytes)",
                     new UInt64Parameter(&_maxTransactionSize));

  options->addOption(
      "--rocksdb.intermediate-commit-size",
      "an intermediate commit will be performed automatically when a transaction "
      "has accumulated operations of this size (in bytes)",
      new UInt64Parameter(&_intermediateCommitSize));

  options->addOption(
      "--rocksdb.intermediate-commit-count",
      "an intermediate commit will be performed automatically when this number of "
      "operations is reached in a transaction",
      new UInt64Parameter(&_intermediateCommitCount));

  options->addOption("--rocksdb.wal-file-timeout",
                     "timeout after which unused WAL files are deleted",
                     new DoubleParameter(&_pruneWaitTime));
}

// validate the storage engine's specific options
void RocksDBEngine::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  transaction::Options::setLimits(_maxTransactionSize, _intermediateCommitSize, _intermediateCommitCount);
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
}

void RocksDBEngine::start() {
  // it is already decided that rocksdb is used
  if (!isEnabled()) {
    return;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::ENGINES) << "rocksdb version " << rest::Version::getRocksDBVersion() << ", supported compression types: " << getCompressionSupport();

  // set the database sub-directory for RocksDB
  auto* databasePathFeature =
      ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _path = databasePathFeature->subdirectoryName("engine-rocksdb");

  LOG_TOPIC(TRACE, arangodb::Logger::STARTUP) << "initializing rocksdb, path: "
                                              << _path;

  rocksdb::TransactionDBOptions transactionOptions;
  // number of locks per column_family
  transactionOptions.num_stripes = TRI_numberProcessors();

  // options imported set by RocksDBOptionFeature
  auto const* opts =
      ApplicationServer::getFeature<arangodb::RocksDBOptionFeature>(
          "RocksDBOption");

  _options.write_buffer_size = static_cast<size_t>(opts->_writeBufferSize);
  _options.max_write_buffer_number =
      static_cast<int>(opts->_maxWriteBufferNumber);
  _options.delayed_write_rate = opts->_delayedWriteRate;
  _options.min_write_buffer_number_to_merge =
      static_cast<int>(opts->_minWriteBufferNumberToMerge);
  _options.num_levels = static_cast<int>(opts->_numLevels);
  _options.max_bytes_for_level_base = opts->_maxBytesForLevelBase;
  _options.max_bytes_for_level_multiplier =
      static_cast<int>(opts->_maxBytesForLevelMultiplier);
  _options.verify_checksums_in_compaction = opts->_verifyChecksumsInCompaction;
  _options.optimize_filters_for_hits = opts->_optimizeFiltersForHits;
  _options.use_direct_reads = opts->_useDirectReads;
  _options.use_direct_writes = opts->_useDirectWrites;
  if (opts->_skipCorrupted) {
    _options.wal_recovery_mode =
        rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;
  } else {
    _options.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
  }

  _options.base_background_compactions =
      static_cast<int>(opts->_baseBackgroundCompactions);
  _options.max_background_compactions =
      static_cast<int>(opts->_maxBackgroundCompactions);
  _options.max_background_flushes = static_cast<int>(opts->_maxFlushes);
  _options.use_fsync = opts->_useFSync;

  // only compress levels >= 2
  _options.compression_per_level.resize(_options.num_levels);
  for (int level = 0; level < _options.num_levels; ++level) {
    _options.compression_per_level[level] =
        (((uint64_t) level >= opts->_numUncompressedLevels) ? rocksdb::kSnappyCompression
                                                 : rocksdb::kNoCompression);
  }

  // TODO: try out the effects of these options 
  // Number of files to trigger level-0 compaction. A value <0 means that
  // level-0 compaction will not be triggered by number of files at all.
  //
  // Default: 4
  // _options.level0_file_num_compaction_trigger = -1;

  // Soft limit on number of level-0 files. We start slowing down writes at this
  // point. A value <0 means that no writing slow down will be triggered by
  // number of files in level-0.
  // _options.level0_slowdown_writes_trigger = -1;

  // Maximum number of level-0 files.  We stop writes at this point.
  // _options.level0_stop_writes_trigger = 256;

  _options.recycle_log_file_num = static_cast<size_t>(opts->_recycleLogFileNum);
  _options.compaction_readahead_size =
      static_cast<size_t>(opts->_compactionReadaheadSize);

  _options.env->SetBackgroundThreads((int)opts->_numThreadsHigh,
                                     rocksdb::Env::Priority::HIGH);
  _options.env->SetBackgroundThreads((int)opts->_numThreadsLow,
                                     rocksdb::Env::Priority::LOW);

  // intentionally set the RocksDB logger to warning because it will
  // log lots of things otherwise
  _options.info_log_level = rocksdb::InfoLogLevel::ERROR_LEVEL;
  auto logger = std::make_shared<RocksDBLogger>(_options.info_log_level);
  _options.info_log = logger;
  logger->disable();

  // _options.statistics = rocksdb::CreateDBStatistics();
  // _options.stats_dump_period_sec = 1;

  rocksdb::BlockBasedTableOptions table_options;
  if (opts->_blockCacheSize > 0) {
    auto cache = rocksdb::NewLRUCache(opts->_blockCacheSize,
                                      (int)opts->_blockCacheShardBits);
    table_options.block_cache = cache;
  } else {
    table_options.no_block_cache = true;
  }
  table_options.block_size = opts->_tableBlockSize;
  table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, true));
  _options.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(table_options));

  _options.create_if_missing = true;
  _options.create_missing_column_families = true;
  _options.max_open_files = -1;
  // WAL_ttl_seconds needs to be bigger than the sync interval of the count
  // manager. Should be several times bigger counter_sync_seconds
  _options.WAL_ttl_seconds = 60 * 60 * 24 * 30;  // we manage WAL file deletion
                                                 // ourselves, don't let RocksDB
                                                 // garbage collect them
  _options.WAL_size_limit_MB = 0;
  double counter_sync_seconds = 2.5;

  _options.prefix_extractor.reset(new RocksDBPrefixExtractor());
  _options.memtable_prefix_bloom_size_ratio = 0.2;  // TODO: pick better value?
  // TODO: enable memtable_insert_with_hint_prefix_extractor?
  _options.bloom_locality = 1;

  // create column families
  std::vector<rocksdb::ColumnFamilyDescriptor> columFamilies;
  rocksdb::ColumnFamilyOptions cfOptions1(_options);
  columFamilies.emplace_back(rocksdb::kDefaultColumnFamilyName, cfOptions1);// 0
  columFamilies.emplace_back("Documents", cfOptions1);// 1
  columFamilies.emplace_back("PrimaryIndex", cfOptions1);// 2
  columFamilies.emplace_back("EdgeIndex", cfOptions1);// 3
  columFamilies.emplace_back("GeoIndex", cfOptions1);// 4
  columFamilies.emplace_back("FulltextIndex", cfOptions1);// 5
  rocksdb::ColumnFamilyOptions cfOptions2(_options);
  cfOptions2.comparator = _vpackCmp.get();
  columFamilies.emplace_back("IndexValue", cfOptions2); // 6
  columFamilies.emplace_back("UniqueIndexValue", cfOptions2);// 7
  // DO NOT FORGET TO DESTROY THE CFs ON CLOSE
 
  std::vector<rocksdb::ColumnFamilyHandle*> cfHandles;
  size_t const numberOfColumnFamilies = RocksDBColumnFamily::numberOfColumnFamilies;
  {
    rocksdb::Options testOptions; 
    testOptions.create_if_missing = false;
    testOptions.create_missing_column_families = false;
    std::vector<std::string> existingColumnFamilies;
    rocksdb::Status status = rocksdb::DB::ListColumnFamilies(testOptions, _path, &existingColumnFamilies);
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
      // we were able to open the database.
      // now check which column families are present in the db
      std::string names;
      for (auto const& it : existingColumnFamilies) {
        if (!names.empty()) {
          names.append(", ");
        }
        names.append(it);
      }
      
      LOG_TOPIC(DEBUG, arangodb::Logger::STARTUP) << "found existing column families: " << names;

      if (existingColumnFamilies.size() < numberOfColumnFamilies) {
        LOG_TOPIC(FATAL, arangodb::Logger::STARTUP) 
            << "unexpected number of column families found in database (" << cfHandles.size() << "). " 
            << "expecting at least " << numberOfColumnFamilies
            << ". if you are upgrading from an alpha version of ArangoDB 3.2, " 
            << "it is required to restart with a new database directory and re-import data";
        FATAL_ERROR_EXIT();
      }
    }
  }

  rocksdb::Status status = rocksdb::TransactionDB::Open(
      _options, transactionOptions, _path, columFamilies, &cfHandles, &_db);

  if (!status.ok()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB engine: " << status.ToString();
    FATAL_ERROR_EXIT();
  }
  if (columFamilies.size() != cfHandles.size()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to initialize RocksDB column families";
    FATAL_ERROR_EXIT();
  }

  if (cfHandles.size() < numberOfColumnFamilies) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP) 
        << "unexpected number of column families found in database. " 
        << "got " << cfHandles.size() << ", expecting at least " << numberOfColumnFamilies;
    FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(cfHandles.size() >= numberOfColumnFamilies);

  // set our column families
  RocksDBColumnFamily::_other = cfHandles[0];
  RocksDBColumnFamily::_documents = cfHandles[1];
  RocksDBColumnFamily::_primary = cfHandles[2];
  RocksDBColumnFamily::_edge = cfHandles[3];
  RocksDBColumnFamily::_geo = cfHandles[4];
  RocksDBColumnFamily::_fulltext = cfHandles[5];
  RocksDBColumnFamily::_index = cfHandles[6];
  RocksDBColumnFamily::_uniqueIndex = cfHandles[7];
  RocksDBColumnFamily::_allHandles = cfHandles;
  TRI_ASSERT(RocksDBColumnFamily::_other->GetID() == 0);

  // only enable logger after RocksDB start
  logger->enable();

  TRI_ASSERT(_db != nullptr);
  _counterManager.reset(new RocksDBCounterManager(_db));
  _replicationManager.reset(new RocksDBReplicationManager());

  _backgroundThread.reset(
      new RocksDBBackgroundThread(this, counter_sync_seconds));
  if (!_backgroundThread->start()) {
    LOG_TOPIC(ERR, Logger::ENGINES)
        << "could not start rocksdb counter manager";
    TRI_ASSERT(false);
  }

  if (!systemDatabaseExists()) {
    addSystemDatabase();
  }
}

void RocksDBEngine::stop() {
  if (!isEnabled()) {
    return;
  }
  replicationManager()->dropAll();

  if (_backgroundThread) {
    // stop the press
    _backgroundThread->beginShutdown();

    if (_counterManager) {
      _counterManager->sync(true);
    }

    // wait until background thread stops
    while (_backgroundThread->isRunning()) {
      usleep(10000);
    }
    _backgroundThread.reset();
  }
}

void RocksDBEngine::unprepare() {
  if (!isEnabled()) {
    return;
  }

  if (_db) {
    for (rocksdb::ColumnFamilyHandle* h : RocksDBColumnFamily::_allHandles) {
      _db->DestroyColumnFamilyHandle(h);
    }

    // now prune all obsolete WAL files
    determinePrunableWalFiles(0);
    pruneWalFiles();

    delete _db;
    _db = nullptr;
  }
}

TransactionManager* RocksDBEngine::createTransactionManager() {
  return new RocksDBTransactionManager();
}

transaction::ContextData* RocksDBEngine::createTransactionContextData() {
  return new RocksDBTransactionContextData();
}

TransactionState* RocksDBEngine::createTransactionState(
    TRI_vocbase_t* vocbase, transaction::Options const& options) {
  return new RocksDBTransactionState(vocbase, options);
}

TransactionCollection* RocksDBEngine::createTransactionCollection(
    TransactionState* state, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int nestingLevel) {
  return new RocksDBTransactionCollection(state, cid, accessType, nestingLevel);
}

void RocksDBEngine::addParametersForNewCollection(VPackBuilder& builder,
                                                  VPackSlice info) {
  if (!info.hasKey("objectId")) {
    builder.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
  }
}

void RocksDBEngine::addParametersForNewIndex(VPackBuilder& builder,
                                             VPackSlice info) {
  if (!info.hasKey("objectId")) {
    builder.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
  }
}

// create storage-engine specific collection
PhysicalCollection* RocksDBEngine::createPhysicalCollection(
    LogicalCollection* collection, VPackSlice const& info) {
  return new RocksDBCollection(collection, info);
}

// create storage-engine specific view
PhysicalView* RocksDBEngine::createPhysicalView(LogicalView* view,
                                                VPackSlice const& info) {
  return new RocksDBView(view, info);
}

// inventory functionality
// -----------------------

void RocksDBEngine::getDatabases(arangodb::velocypack::Builder& result) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "getting existing databases";

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions,
                                                           RocksDBColumnFamily::other()));
  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::Database);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    auto slice = VPackSlice(iter->value().data());

    //// check format
    // id
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

void RocksDBEngine::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
                                      arangodb::velocypack::Builder& builder,
                                      bool includeIndexes,
                                      TRI_voc_tick_t maxTick) {
  builder.openObject();

  // read collection info from database
  auto key = RocksDBKey::Collection(vocbase->id(), cid);
  auto value = RocksDBValue::Empty(RocksDBEntryType::Collection);
  rocksdb::ReadOptions options;
  rocksdb::Status res = _db->Get(options, key.string(), value.buffer());
  auto result = rocksutils::convertStatus(res);

  if (result.errorNumber() != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(result.errorNumber());
  }

  VPackSlice fullParameters = VPackSlice(value.buffer()->data());

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
        VPackSlice type = idx.get("type");
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
    TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result,
    bool wasCleanShutdown, bool isUpgrade) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions,
                                                           RocksDBColumnFamily::other()));

  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::Collection);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase->id() != RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice = VPackSlice(iter->value().data());

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(slice, "deleted",
                                                             false)) {
      continue;
    }
    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::getViews(TRI_vocbase_t* vocbase,
                            arangodb::velocypack::Builder& result) {
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions,
                                                           RocksDBColumnFamily::other()));

  result.openArray();
  auto rSlice = rocksDBSlice(RocksDBEntryType::View);
  for (iter->Seek(rSlice); iter->Valid() && iter->key().starts_with(rSlice);
       iter->Next()) {
    if (vocbase->id() != !RocksDBKey::databaseId(iter->key())) {
      continue;
    }

    auto slice = VPackSlice(iter->value().data());

    LOG_TOPIC(TRACE, Logger::FIXME) << "got view slice: " << slice.toJson();

    if (arangodb::basics::VelocyPackHelper::readBooleanValue(slice, "deleted",
                                                             false)) {
      continue;
    }
    result.add(slice);
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

std::string RocksDBEngine::databasePath(TRI_vocbase_t const* vocbase) const {
  return _basePath;
}

std::string RocksDBEngine::versionFilename(TRI_voc_tick_t id) const {
  return _basePath + TRI_DIR_SEPARATOR_CHAR + "VERSION-" + std::to_string(id);
}

std::string RocksDBEngine::collectionPath(TRI_vocbase_t const* vocbase,
                                          TRI_voc_cid_t id) const {
  return std::string();  // no path to be returned here
}

void RocksDBEngine::waitForSync(TRI_voc_tick_t tick) {
  // intentionally empty, not useful for this type of engine
}

std::shared_ptr<arangodb::velocypack::Builder>
RocksDBEngine::getReplicationApplierConfiguration(TRI_vocbase_t* vocbase,
                                                  int& status) {
  auto key = RocksDBKey::ReplicationApplierConfig(vocbase->id());
  auto value = RocksDBValue::Empty(RocksDBEntryType::ReplicationApplierConfig);

  auto db = rocksutils::globalRocksDB();
  auto opts = rocksdb::ReadOptions();
  auto s = db->Get(opts, key.string(), value.buffer());
  if (!s.ok()) {
    status = TRI_ERROR_FILE_NOT_FOUND;
    return std::shared_ptr<arangodb::velocypack::Builder>();
  }

  auto builder = std::make_shared<VPackBuilder>();
  builder->add(RocksDBValue::data(value));

  status = TRI_ERROR_NO_ERROR;
  return builder;
}

int RocksDBEngine::removeReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase) {
  auto key = RocksDBKey::ReplicationApplierConfig(vocbase->id());

  auto status = rocksutils::globalRocksDBRemove(key.string());
  if (!status.ok()) {
    return status.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

int RocksDBEngine::saveReplicationApplierConfiguration(
    TRI_vocbase_t* vocbase, arangodb::velocypack::Slice slice, bool doSync) {
  auto key = RocksDBKey::ReplicationApplierConfig(vocbase->id());
  auto value = RocksDBValue::ReplicationApplierConfig(slice);

  auto status = rocksutils::globalRocksDBPut(key.string(), value.string());
  if (!status.ok()) {
    return status.errorNumber();
  }

  return TRI_ERROR_NO_ERROR;
}

// database, collection and index management
// -----------------------------------------

TRI_vocbase_t* RocksDBEngine::openDatabase(
    arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  VPackSlice idSlice = args.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
      basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = args.get("name").copyString();

  status = TRI_ERROR_NO_ERROR;

  return openExistingDatabase(id, name, true, isUpgrade);
}

TRI_vocbase_t* RocksDBEngine::createDatabase(
    TRI_voc_tick_t id, arangodb::velocypack::Slice const& args, int& status) {
  status = TRI_ERROR_NO_ERROR;
  auto vocbase = std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id,
                                                 args.get("name").copyString());
  return vocbase.release();
}

int RocksDBEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                             VPackSlice const& slice) {
  auto key = RocksDBKey::Database(id);
  auto value = RocksDBValue::Database(slice);
  rocksdb::WriteOptions options;  // TODO: check which options would make sense

  rocksdb::Status res = _db->Put(options, RocksDBColumnFamily::other(),
                                 key.string(), value.string());
  auto result = rocksutils::convertStatus(res);
  return result.errorNumber();
}

int RocksDBEngine::writeCreateCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t cid,
                                               VPackSlice const& slice,
                                               RocksDBLogValue&& logValue) {
  auto key = RocksDBKey::Collection(databaseId, cid);
  auto value = RocksDBValue::Collection(slice);
  rocksdb::WriteOptions options;  // TODO: check which options would make sense

  // Write marker + key into RocksDB inside one batch
  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Put(key.string(), value.string());
  rocksdb::Status res = _db->Write(options, &batch);

  auto result = rocksutils::convertStatus(res);
  return result.errorNumber();
}

void RocksDBEngine::prepareDropDatabase(TRI_vocbase_t* vocbase,
                                        bool useWriteMarker, int& status) {
  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(std::to_string(vocbase->id())));
  builder.add("name", VPackValue(vocbase->name()));
  builder.add("deleted", VPackValue(true));
  builder.close();

  status = writeCreateDatabaseMarker(vocbase->id(), builder.slice());
}

Result RocksDBEngine::dropDatabase(TRI_vocbase_t* database) {
  replicationManager()->drop(database);
  return dropDatabase(database->id());
}

void RocksDBEngine::waitUntilDeletion(TRI_voc_tick_t /* id */, bool /* force */,
                                      int& status) {
  // can delete databases instantly
  status = TRI_ERROR_NO_ERROR;
}

// wal in recovery
bool RocksDBEngine::inRecovery() {
  // recovery is handled outside of this engine
  return false;
}

void RocksDBEngine::recoveryDone(TRI_vocbase_t* vocbase) {
  // nothing to do here
}

std::string RocksDBEngine::createCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
    arangodb::LogicalCollection const* parameters) {
  VPackBuilder builder = parameters->toVelocyPackIgnore(
      {"path", "statusString"}, /*translate cid*/ true,
      /*for persistence*/ true);

  // should cause counter to be added to the manager
  // in case the collection is created for the first time
  VPackSlice objectId = builder.slice().get("objectId");
  if (objectId.isInteger()) {
    RocksDBCounterManager::CounterAdjustment adj;
    _counterManager->updateCounter(objectId.getUInt(), adj);
  }

  int res = writeCreateCollectionMarker(
      vocbase->id(), cid, builder.slice(),
      RocksDBLogValue::CollectionCreate(vocbase->id(), cid));

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return std::string();  // no need to return a path
}

arangodb::Result RocksDBEngine::persistCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection) {
  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(vocbase != nullptr);
  arangodb::Result result;
  if (inRecovery()) {
    // Nothing to do. In recovery we do not write markers.
    return result;
  }
  VPackBuilder builder =
      collection->toVelocyPackIgnore({"path", "statusString"}, true, true);
  VPackSlice const slice = builder.slice();

  auto cid = collection->cid();
  TRI_ASSERT(cid != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(cid));

  int res = writeCreateCollectionMarker(
      vocbase->id(), cid, slice,
      RocksDBLogValue::CollectionCreate(vocbase->id(), cid));
  result.reset(res);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (result.ok()) {
    RocksDBCollection* rcoll = toRocksDBCollection(collection->getPhysical());
    TRI_ASSERT(rcoll->numberDocuments() == 0);
  }
#endif
  return result;
}

arangodb::Result RocksDBEngine::dropCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  RocksDBCollection* coll = toRocksDBCollection(collection->getPhysical());
  uint64_t const numberDocuments = coll->numberDocuments();

  rocksdb::WriteOptions options;  // TODO: check which options would make sense

  // If we get here the collection is save to drop.
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

  TRI_ASSERT(collection->status() == TRI_VOC_COL_STATUS_DELETED);

  // Prepare collection remove batch
  RocksDBLogValue logValue =
      RocksDBLogValue::CollectionDrop(vocbase->id(), collection->cid());
  rocksdb::WriteBatch batch;
  batch.PutLogData(logValue.slice());
  batch.Delete(
      RocksDBKey::Collection(vocbase->id(), collection->cid()).string());
  rocksdb::Status res = _db->Write(options, &batch);

  // TODO FAILURE Simulate !res.ok()
  if (!res.ok()) {
    // Persisting the drop failed. Do NOT drop collection.
    return rocksutils::convertStatus(res);
  }

  // Now Collection is gone.
  // Cleanup data-mess

  // Unregister counter
  _counterManager->removeCounter(coll->objectId());

  // remove from map
  {
    WRITE_LOCKER(guard, _collectionMapLock);
    _collectionMap.erase(collection->cid());
  }

  // delete documents
  RocksDBKeyBounds bounds =
      RocksDBKeyBounds::CollectionDocuments(coll->objectId());
  arangodb::Result result = rocksutils::removeLargeRange(_db, bounds);

  // TODO FAILURE Simulate result.fail()
  if (result.fail()) {
    // We try to remove all documents.
    // If it does not work they cannot be accessed any more and leaked.
    // User View remains consistent.
    return TRI_ERROR_NO_ERROR;
  }

  // delete indexes
  std::vector<std::shared_ptr<Index>> vecShardIndex = coll->getIndexes();
  for (auto& index : vecShardIndex) {
    int dropRes = index->drop();
    // TODO FAILURE Simulate dropRes != TRI_ERROR_NO_ERROR
    if (dropRes != TRI_ERROR_NO_ERROR) {
      // We try to remove all indexed values.
      // If it does not work they cannot be accessed any more and leaked.
      // User View remains consistent.
      return TRI_ERROR_NO_ERROR;
    }
  }

  // run compaction for data only if collection contained a considerable
  // amount of documents. otherwise don't run compaction, because it will
  // slow things down a lot, especially during tests that create/drop LOTS
  // of collections
  if (numberDocuments >= 16384) {
    coll->compact();
  }

  // if we get here all documents / indexes are gone.
  // We have no data garbage left.
  return TRI_ERROR_NO_ERROR;
}

void RocksDBEngine::destroyCollection(TRI_vocbase_t* vocbase,
                                      arangodb::LogicalCollection*) {
  // not required
}

void RocksDBEngine::changeCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters, bool doSync) {
  VPackBuilder builder = parameters->toVelocyPackIgnore(
      {"path", "statusString"}, /*translate cid*/ true,
      /*for persistence*/ true);

  int res = writeCreateCollectionMarker(
      vocbase->id(), id, builder.slice(),
      RocksDBLogValue::CollectionChange(vocbase->id(), id));

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

arangodb::Result RocksDBEngine::renameCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection,
    std::string const& oldName) {
  VPackBuilder builder =
      collection->toVelocyPackIgnore({"path", "statusString"}, true, true);
  int res = writeCreateCollectionMarker(
      vocbase->id(), collection->cid(), builder.slice(),
      RocksDBLogValue::CollectionRename(vocbase->id(), collection->cid(),
                                        StringRef(collection->name())));
  return arangodb::Result(res);
}

void RocksDBEngine::createIndex(TRI_vocbase_t* vocbase,
                                TRI_voc_cid_t collectionId,
                                TRI_idx_iid_t indexId,
                                arangodb::velocypack::Slice const& data) {}

void RocksDBEngine::unloadCollection(TRI_vocbase_t* vocbase,
                                     arangodb::LogicalCollection* collection) {
  // TODO: does anything else have to happen?
  collection->setStatus(TRI_VOC_COL_STATUS_UNLOADED);
}

void RocksDBEngine::createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalView const*) {
  auto key = RocksDBKey::View(vocbase->id(), id);
  auto value = RocksDBValue::View(VPackSlice::emptyObjectSlice());

  auto status = rocksutils::globalRocksDBPut(key.string(), value.string());
  if (!status.ok()) {
    THROW_ARANGO_EXCEPTION(status.errorNumber());
  }
}

arangodb::Result RocksDBEngine::persistView(
    TRI_vocbase_t* vocbase, arangodb::LogicalView const* logical) {
  auto physical = static_cast<RocksDBView*>(logical->getPhysical());
  return physical->persistProperties();
}

arangodb::Result RocksDBEngine::dropView(TRI_vocbase_t* vocbase,
                                         arangodb::LogicalView*) {
  // nothing to do here
  return {TRI_ERROR_NO_ERROR};
}

void RocksDBEngine::destroyView(TRI_vocbase_t* vocbase,
                                arangodb::LogicalView*) {
  // nothing to do here
}

void RocksDBEngine::changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalView const*, bool doSync) {
  // nothing to do here
}

void RocksDBEngine::signalCleanup(TRI_vocbase_t*) {
  // nothing to do here
}

int RocksDBEngine::shutdownDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief Add engine-specific AQL functions.
void RocksDBEngine::addAqlFunctions() {
  RocksDBAqlFunctions::registerResources();
}

/// @brief Add engine-specific optimizer rules
void RocksDBEngine::addOptimizerRules() {
  // there are no specific optimizer rules here
  // TODO: add geo index optimization once there is the geo index
}

/// @brief Add engine-specific V8 functions
void RocksDBEngine::addV8Functions() {
  // there are no specific V8 functions here
  RocksDBV8Functions::registerResources();
}

/// @brief Add engine-specific REST handlers
void RocksDBEngine::addRestHandlers(rest::RestHandlerFactory* handlerFactory) {
  RocksDBRestHandlers::registerResources(handlerFactory);
}

void RocksDBEngine::addCollectionMapping(uint64_t objectId, TRI_voc_tick_t did,
                                         TRI_voc_cid_t cid) {
  if (objectId == 0) {
    return;
  }

  WRITE_LOCKER(guard, _collectionMapLock);
  _collectionMap[objectId] = std::make_pair(did, cid);
}

std::pair<TRI_voc_tick_t, TRI_voc_cid_t> RocksDBEngine::mapObjectToCollection(
    uint64_t objectId) const {
  READ_LOCKER(guard, _collectionMapLock);
  auto it = _collectionMap.find(objectId);
  if (it == _collectionMap.end()) {
    return {0, 0};
  }
  return it->second;
}

arangodb::Result RocksDBEngine::syncWal() {
#ifdef _WIN32
  // SyncWAL always reports "not implemented" on Windows
  return arangodb::Result();
#else
  rocksdb::Status status = _db->GetBaseDB()->SyncWAL();
  if (!status.ok()) {
    return rocksutils::convertStatus(status);
  }
  return arangodb::Result();
#endif
}

Result RocksDBEngine::createLoggerState(TRI_vocbase_t* vocbase,
                                        VPackBuilder& builder) {
  syncWal();

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

      builder.add("lastServedTick",
                  VPackValue(std::to_string(std::get<2>(it))));

      builder.close();
    }
  }
  builder.close();  // clients

  builder.close();  // base

  return Result{};
}

void RocksDBEngine::determinePrunableWalFiles(TRI_voc_tick_t minTickToKeep) {
  rocksdb::VectorLogPtr files;

  auto status = _db->GetSortedWalFiles(files);
  if (!status.ok()) {
    return;  // TODO: error here?
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
          _prunableWalFiles.emplace(f->PathName(),
                                    TRI_microtime() + _pruneWaitTime);
        }
      }
    }
  }
}

void RocksDBEngine::pruneWalFiles() {
  // go through the map of WAL files that we have already and check if they are
  // "expired"
  for (auto it = _prunableWalFiles.begin(); it != _prunableWalFiles.end();
       /* no hoisting */) {
    // check if WAL file is expired
    if ((*it).second < TRI_microtime()) {
      auto s = _db->DeleteFile((*it).first);
      if (s.ok()) {
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
  Result res;
  rocksdb::WriteOptions options;  // TODO: check which options would make sense

  // remove views
  for (auto const& val : viewKVPairs(id)) {
    res = globalRocksDBRemove(val.first.string(), options);
    if (res.fail()) {
      return res;
    }
  }

  // remove collections
  for (auto const& val : collectionKVPairs(id)) {
    // remove indexes
    VPackSlice indexes = val.second.slice().get("indexes");
    if (indexes.isArray()) {
      for (auto const& it : VPackArrayIterator(indexes)) {
        // delete index documents
        uint64_t objectId =
            basics::VelocyPackHelper::stringUInt64(it, "objectId");
        RocksDBKeyBounds bounds = RocksDBKeyBounds::IndexEntries(objectId);
        res = rocksutils::removeLargeRange(_db, bounds);
        if (res.fail()) {
          return res;
        }
        // delete index
        res = globalRocksDBRemove(val.first.string(), options);
        if (res.fail()) {
          return res;
        }
      }
    }

    uint64_t objectId =
        basics::VelocyPackHelper::stringUInt64(val.second.slice(), "objectId");
    // delete documents
    RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId);
    res = rocksutils::removeLargeRange(_db, bounds);
    if (res.fail()) {
      return res;
    }
    // delete Collection
    _counterManager->removeCounter(objectId);
    res = globalRocksDBRemove(val.first.string(), options);
    if (res.fail()) {
      return res;
    }
  }

  // TODO
  // How to unregister Vocbase?
  // Cleanup thread does it in MMFiles

  auto key = RocksDBKey::Database(id);
  res = rocksutils::globalRocksDBRemove(key.string(), options);

  // remove VERSION file for database. it's not a problem when this fails
  // because it will simply remain there and be ignored on subsequent starts
  TRI_UnlinkFile(versionFilename(id).c_str());

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

  int res = writeCreateDatabaseMarker(id, builder.slice());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "unable to write database marker: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }
}

/// @brief open an existing database. internal function
TRI_vocbase_t* RocksDBEngine::openExistingDatabase(TRI_voc_tick_t id,
                                                   std::string const& name,
                                                   bool wasCleanShutdown,
                                                   bool isUpgrade) {
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);

  // scan the database path for views
  try {
    VPackBuilder builder;
    int res = getViews(vocbase.get(), builder);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    ViewTypesFeature* viewTypesFeature =
        application_features::ApplicationServer::getFeature<ViewTypesFeature>(
            "ViewTypes");

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a view that is still active

      std::string type = it.get("type").copyString();
      // will throw if type is invalid
      ViewCreator& creator = viewTypesFeature->creator(type);

      TRI_ASSERT(!it.get("id").isNone());

      std::shared_ptr<LogicalView> view =
          std::make_shared<arangodb::LogicalView>(vocbase.get(), it);

      StorageEngine::registerView(vocbase.get(), view);

      auto physical = static_cast<RocksDBView*>(view->getPhysical());
      TRI_ASSERT(physical != nullptr);

      view->spawnImplementation(creator, it, false);
      view->getImplementation()->open();
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while opening database: "
                                            << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "error while opening database: unknown exception";
    throw;
  }

  // scan the database path for collections
  try {
    VPackBuilder builder;
    int res = getCollectionsAndIndexes(vocbase.get(), builder, wasCleanShutdown,
                                       isUpgrade);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a collection that is still active
      TRI_ASSERT(!it.get("id").isNone() || !it.get("cid").isNone());
      auto uniqCol =
          std::make_unique<arangodb::LogicalCollection>(vocbase.get(), it);
      auto collection = uniqCol.get();
      TRI_ASSERT(collection != nullptr);
      StorageEngine::registerCollection(vocbase.get(), uniqCol.get());
      // The vocbase has taken over control
      uniqCol.release();

      auto physical =
          static_cast<RocksDBCollection*>(collection->getPhysical());
      TRI_ASSERT(physical != nullptr);

      physical->deserializeIndexEstimates(counterManager());
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "added document collection '"
                                                << collection->name() << "'";
    }

    return vocbase.release();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while opening database: "
                                            << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "error while opening database: unknown exception";
    throw;
  }
}

RocksDBCounterManager* RocksDBEngine::counterManager() const {
  TRI_ASSERT(_counterManager);
  return _counterManager.get();
}

RocksDBReplicationManager* RocksDBEngine::replicationManager() const {
  TRI_ASSERT(_replicationManager);
  return _replicationManager.get();
}

void RocksDBEngine::getStatistics(VPackBuilder& builder) const {
  builder.openObject();
  // add int properties
  auto c1 = [&](std::string const& s) {
    std::string v;
    if (_db->GetProperty(s, &v)) {
      builder.add(s, VPackValue(v));
    }
  };
  c1(rocksdb::DB::Properties::kNumImmutableMemTable);
  c1(rocksdb::DB::Properties::kMemTableFlushPending);
  c1(rocksdb::DB::Properties::kCompactionPending);
  c1(rocksdb::DB::Properties::kBackgroundErrors);
  c1(rocksdb::DB::Properties::kCurSizeActiveMemTable);
  c1(rocksdb::DB::Properties::kCurSizeAllMemTables);
  c1(rocksdb::DB::Properties::kSizeAllMemTables);
  c1(rocksdb::DB::Properties::kNumEntriesImmMemTables);
  c1(rocksdb::DB::Properties::kNumSnapshots);
  c1(rocksdb::DB::Properties::kDBStats);
  c1(rocksdb::DB::Properties::kCFStats);
  c1(rocksdb::DB::Properties::kSSTables);
  c1(rocksdb::DB::Properties::kNumRunningCompactions);
  c1(rocksdb::DB::Properties::kNumRunningFlushes);
  c1(rocksdb::DB::Properties::kIsFileDeletionsEnabled);
  c1(rocksdb::DB::Properties::kBaseLevel);
  c1(rocksdb::DB::Properties::kTotalSstFilesSize);

  builder.close();
}

int RocksDBEngine::handleSyncKeys(arangodb::InitialSyncer& syncer,
                                  arangodb::LogicalCollection* col,
                                  std::string const& keysId,
                                  std::string const& cid,
                                  std::string const& collectionName,
                                  TRI_voc_tick_t maxTick,
                                  std::string& errorMsg) {
  return handleSyncKeysRocksDB(syncer, col, keysId, cid, collectionName,
                               maxTick, errorMsg);
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
      builder.add("state", VPackValue("open"));
    } else if (logfile->Type() == rocksdb::WalFileType::kArchivedLogFile) {
      builder.add("state", VPackValue("collected"));
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
    TRI_vocbase_t* vocbase,
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
      vocbase, tickStart, tickEnd, chunkSize, includeSystem, 0, *builder);
  builder->close();
  builderSPtr = std::move(builder);

  return rep;
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

}  // namespace arangodb
