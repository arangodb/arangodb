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

#include "RocksDBFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Indexes/RocksDBKeyComparator.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"
  
#include <rocksdb/db.h>
#include <rocksdb/convenience.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

static RocksDBFeature* Instance = nullptr;

RocksDBFeature::RocksDBFeature(
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "RocksDB"),
      _db(nullptr), _comparator(nullptr), _path(), _active(true),
      _writeBufferSize(0), _maxWriteBufferNumber(2), 
      _delayedWriteRate(2 * 1024 * 1024), _minWriteBufferNumberToMerge(1),
      _numLevels(4), _maxBytesForLevelBase(256 * 1024 * 1024),
      _maxBytesForLevelMultiplier(10), _verifyChecksumsInCompaction(true),
      _optimizeFiltersForHits(true), _baseBackgroundCompactions(1),
      _maxBackgroundCompactions(1), _maxLogFileSize(0),
      _keepLogFileNum(1000), _logFileTimeToRoll(0), _compactionReadaheadSize(0) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("LogfileManager");
  startsAfter("DatabasePath");
}

RocksDBFeature::~RocksDBFeature() {
  try {
    delete _db;
  } catch (...) {
  }
  try {
    delete _comparator;
  } catch (...) {
  }
}

void RocksDBFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "Configure the RocksDB engine");

  options->addOption(
      "--rocksdb.enabled",
      "Whether or not the RocksDB engine is enabled", 
      new BooleanParameter(&_active));
  
  options->addOption(
      "--rocksdb.write-buffer-size",
      "amount of data to build up in memory before converting to a sorted on-disk file (0 = disabled)",
      new UInt64Parameter(&_writeBufferSize));
  
  options->addOption(
      "--rocksdb.max-write-buffer-number",
      "maximum number of write buffers that built up in memory",
      new UInt64Parameter(&_maxWriteBufferNumber));
  
  options->addHiddenOption(
      "--rocksdb.delayed_write_rate",
      "limited write rate to DB (in bytes per second) if we are writing to the last "
      "mem table allowed and we allow more than 3 mem tables",
      new UInt64Parameter(&_delayedWriteRate)); 

  options->addOption(
      "--rocksdb.min-write-buffer-number-to-merge",
      "minimum number of write buffers that will be merged together before writing "
      "to storage",
      new UInt64Parameter(&_minWriteBufferNumberToMerge)); 

  options->addOption(
      "--rocksdb.num-levels",
      "number of levels for the database",
      new UInt64Parameter(&_numLevels)); 

  options->addHiddenOption(
      "--rocksdb.max-bytes-for-level-base",
      "control maximum total data size for a level",
      new UInt64Parameter(&_maxBytesForLevelBase));
  
  options->addOption(
      "--rocksdb.max-bytes-for-level-multiplier",
      "control maximum total data size for a level",
      new UInt64Parameter(&_maxBytesForLevelMultiplier));

  options->addOption(
      "--rocksdb.verify-checksums-in-compation",
      "if true, compaction will verify checksum on every read that happens "
      "as part of compaction",
      new BooleanParameter(&_verifyChecksumsInCompaction));

  options->addOption(
      "--rocksdb.optimize-filters-for-hits",
      "this flag specifies that the implementation should optimize the filters "
      "mainly for cases where keys are found rather than also optimize for keys "
      "missed. This would be used in cases where the application knows that "
      "there are very few misses or the performance in the case of misses is not "
      "important",
      new BooleanParameter(&_optimizeFiltersForHits));
  
  options->addOption(
      "--rocksdb.base-background-compactions",
      "suggested number of concurrent background compaction jobs",
      new UInt64Parameter(&_baseBackgroundCompactions));
  
  options->addOption(
      "--rocksdb.max-background-compactions",
      "maximum number of concurrent background compaction jobs",
      new UInt64Parameter(&_maxBackgroundCompactions));
  
  options->addOption(
      "--rocksdb.max-log-file-size",
      "specify the maximal size of the info log file",
      new UInt64Parameter(&_maxLogFileSize));

  options->addOption(
      "--rocksdb.keep-log-file-num",
      "maximal info log files to be kept",
      new UInt64Parameter(&_keepLogFileNum));
  
  options->addOption(
      "--rocksdb.log-file-time-to-roll",
      "time for the info log file to roll (in seconds). "
      "If specified with non-zero value, log file will be rolled "
      "if it has been active longer than `log_file_time_to_roll`",
      new UInt64Parameter(&_logFileTimeToRoll));
  
  options->addOption(
      "--rocksdb.compaction-read-ahead-size",
      "if non-zero, we perform bigger reads when doing compaction. If you're "
      "running RocksDB on spinning disks, you should set this to at least 2MB. "
      "that way RocksDB's compaction is doing sequential instead of random reads.",
      new UInt64Parameter(&_compactionReadaheadSize));
}

void RocksDBFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_active) {
    forceDisable();
  } else {
    if (_writeBufferSize > 0 && _writeBufferSize < 1024 * 1024) {
      LOG(FATAL) << "invalid value for '--rocksdb.write-buffer-size'";
      FATAL_ERROR_EXIT();
    }
    if (_maxBytesForLevelMultiplier == 0) {
      LOG(FATAL) << "invalid value for '--rocksdb.max-bytes-for-level-multiplier'";
      FATAL_ERROR_EXIT();
    }
    if (_numLevels < 1 || _numLevels > 20) {
      LOG(FATAL) << "invalid value for '--rocksdb.num-levels'";
      FATAL_ERROR_EXIT();
    }
    if (_baseBackgroundCompactions < 1 || _baseBackgroundCompactions > 64) {
      LOG(FATAL) << "invalid value for '--rocksdb.base-background-compactions'";
      FATAL_ERROR_EXIT();
    }
    if (_maxBackgroundCompactions < _baseBackgroundCompactions) {
      _maxBackgroundCompactions = _baseBackgroundCompactions;
    }
  }
}

void RocksDBFeature::start() {
  Instance = this;

  if (!isEnabled()) {
    return;
  }

  // set the database sub-directory for RocksDB
  auto database = ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _path = database->subdirectoryName("rocksdb");
  
  LOG(TRACE) << "initializing rocksdb, path: " << _path;
  
  _comparator = new RocksDBKeyComparator();
  
  rocksdb::BlockBasedTableOptions tableOptions;
  tableOptions.cache_index_and_filter_blocks = true;
  tableOptions.filter_policy.reset(rocksdb::NewBloomFilterPolicy(12, false));
  
  // TODO: using the prefix extractor will lead to the comparator being
  // called with just the key prefix (which the comparator currently cannot handle)
  // _options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(RocksDBIndex::minimalPrefixSize()));
  // _options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(tableOptions));
  
  _options.create_if_missing = true;
  _options.max_open_files = -1;
  _options.comparator = _comparator;

  _options.write_buffer_size = static_cast<size_t>(_writeBufferSize);
  _options.max_write_buffer_number = static_cast<int>(_maxWriteBufferNumber);
  _options.delayed_write_rate = _delayedWriteRate;
  _options.min_write_buffer_number_to_merge = static_cast<int>(_minWriteBufferNumberToMerge);
  _options.num_levels = static_cast<int>(_numLevels);
  _options.max_bytes_for_level_base = _maxBytesForLevelBase;
  _options.max_bytes_for_level_multiplier = static_cast<int>(_maxBytesForLevelMultiplier);
  _options.verify_checksums_in_compaction = _verifyChecksumsInCompaction;
  _options.optimize_filters_for_hits = _optimizeFiltersForHits;
  
  _options.base_background_compactions = static_cast<int>(_baseBackgroundCompactions);
  _options.max_background_compactions = static_cast<int>(_maxBackgroundCompactions);
  
  _options.max_log_file_size = static_cast<size_t>(_maxLogFileSize);
  _options.keep_log_file_num = static_cast<size_t>(_keepLogFileNum);
  _options.log_file_time_to_roll = static_cast<size_t>(_logFileTimeToRoll);
  _options.compaction_readahead_size = static_cast<size_t>(_compactionReadaheadSize);
 
  if (_options.base_background_compactions > 1 || _options.max_background_compactions > 1) {
    _options.env->SetBackgroundThreads(
      (std::max)(_options.base_background_compactions, _options.max_background_compactions),
      rocksdb::Env::Priority::LOW);
  }

  //options.block_cache = rocksdb::NewLRUCache(100 * 1048576); // 100MB uncompressed cache
  //options.block_cache_compressed = rocksdb::NewLRUCache(100 * 1048576); // 100MB compressed cache

  rocksdb::Status status = rocksdb::OptimisticTransactionDB::Open(_options, _path, &_db);
  
  if (! status.ok()) {
    LOG(FATAL) << "unable to initialize RocksDB: " << status.ToString();
    FATAL_ERROR_EXIT();
  }
}

void RocksDBFeature::unprepare() {
  if (!isEnabled()) {
    return;
  }

  LOG(TRACE) << "shutting down RocksDB";

  // flush
  rocksdb::FlushOptions options;
  options.wait = true;
  rocksdb::Status status = _db->GetBaseDB()->Flush(options);
  
  if (! status.ok()) {
    LOG(ERR) << "error flushing data to RocksDB: " << status.ToString();
  }

  syncWal();
}
  
RocksDBFeature* RocksDBFeature::instance() {
  return Instance;
}

int RocksDBFeature::syncWal() {
#ifndef _WIN32
  // SyncWAL() always reports a "not implemented" error on Windows
  if (Instance == nullptr || !Instance->isEnabled()) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG(TRACE) << "syncing RocksDB WAL";

  rocksdb::Status status = Instance->db()->GetBaseDB()->SyncWAL();

  if (! status.ok()) {
    LOG(ERR) << "error syncing RocksDB WAL: " << status.ToString();
    return TRI_ERROR_INTERNAL;
  }
#endif
  return TRI_ERROR_NO_ERROR;
}

int RocksDBFeature::dropDatabase(TRI_voc_tick_t databaseId) {
  if (Instance == nullptr) {
    return TRI_ERROR_INTERNAL;
  }
  return Instance->dropPrefix(RocksDBIndex::buildPrefix(databaseId));
}

int RocksDBFeature::dropCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
  if (Instance == nullptr) {
    return TRI_ERROR_INTERNAL;
  }
  return Instance->dropPrefix(RocksDBIndex::buildPrefix(databaseId, collectionId));
}

int RocksDBFeature::dropIndex(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, TRI_idx_iid_t indexId) {
  if (Instance == nullptr) {
    return TRI_ERROR_INTERNAL;
  }
  return Instance->dropPrefix(RocksDBIndex::buildPrefix(databaseId, collectionId, indexId));
}

int RocksDBFeature::dropPrefix(std::string const& prefix) {
  if (!isEnabled()) {
    return TRI_ERROR_NO_ERROR;
  }
  
  TRI_ASSERT(Instance != nullptr);

  try {
    VPackBuilder builder;

    // create lower and upper bound for deletion
    builder.openArray();
    builder.add(VPackSlice::minKeySlice());
    builder.close();  

    std::string l;
    l.reserve(prefix.size() + builder.slice().byteSize());
    l.append(prefix);
    // extend the prefix to at least 24 bytes
    while (l.size() < RocksDBIndex::keyPrefixSize()) {
      uint64_t value = 0;
      l.append(reinterpret_cast<char const*>(&value), sizeof(uint64_t));
    }
    l.append(builder.slice().startAs<char const>(), builder.slice().byteSize());
    
    builder.clear();
    builder.openArray();
    builder.add(VPackSlice::maxKeySlice());
    builder.close();  
    
    std::string u;
    u.reserve(prefix.size() + builder.slice().byteSize());
    u.append(prefix);
    // extend the prefix to at least 24 bytes
    while (u.size() < RocksDBIndex::keyPrefixSize()) {
      uint64_t value = UINT64_MAX;
      u.append(reinterpret_cast<char const*>(&value), sizeof(uint64_t));
    }
    u.append(builder.slice().startAs<char const>(), builder.slice().byteSize());
 
#if 0 
    for (size_t i = 0; i < prefix.size(); i += sizeof(TRI_idx_iid_t)) {
      char const* x = prefix.c_str() + i;
      LOG(TRACE) << "prefix part: " << std::to_string(*reinterpret_cast<uint64_t const*>(x));
    }
#endif

    // delete files in range lower..upper
    LOG(TRACE) << "dropping range: " << VPackSlice(l.c_str() + prefix.size()).toJson() << " - " << VPackSlice(u.c_str() + prefix.size()).toJson();

    rocksdb::Slice lower(l.c_str(), l.size());
    rocksdb::Slice upper(u.c_str(), u.size());

    {
      rocksdb::Status status = rocksdb::DeleteFilesInRange(_db->GetBaseDB(), _db->GetBaseDB()->DefaultColumnFamily(), &lower, &upper);

      if (!status.ok()) {
        // if file deletion failed, we will still iterate over the remaining keys, so we
        // don't need to abort and raise an error here
        LOG(WARN) << "RocksDB file deletion failed";
      }
    }
    
    // go on and delete the remaining keys (delete files in range does not necessarily
    // find them all, just complete files)
    
    auto comparator = RocksDBFeature::instance()->comparator();
    rocksdb::DB* db = _db->GetBaseDB();

    rocksdb::WriteBatch batch;
    
    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));

    it->Seek(lower);
    while (it->Valid()) {
      int res = comparator->Compare(it->key(), upper);

      if (res >= 0) {
        break;
      }
      
      batch.Delete(it->key());

      it->Next();
    }
    
    // now apply deletion batch
    rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);

    if (!status.ok()) {
      LOG(WARN) << "RocksDB key deletion failed: " << status.ToString();
      return TRI_ERROR_INTERNAL;
    }

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    LOG(ERR) << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(ERR) << "caught unknown exception during RocksDB key prefix deletion";
    return TRI_ERROR_INTERNAL;
  }
}

