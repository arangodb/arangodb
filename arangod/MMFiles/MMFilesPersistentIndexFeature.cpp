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

#include "ApplicationFeatures/RocksDBOptionFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "MMFiles/MMFilesPersistentIndexKeyComparator.h"
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

static MMFilesPersistentIndexFeature* Instance = nullptr;

MMFilesPersistentIndexFeature::MMFilesPersistentIndexFeature(
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "MMFilesPersistentIndex"),
      _db(nullptr), _comparator(nullptr), _path()
{
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("RocksDBOption");
  startsBefore("Database");
  onlyEnabledWith("MMFilesEngine");
}

MMFilesPersistentIndexFeature::~MMFilesPersistentIndexFeature() {
  try {
    delete _db;
  } catch (...) {
  }
  try {
    delete _comparator;
  } catch (...) {
  }
}

void MMFilesPersistentIndexFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
}

void MMFilesPersistentIndexFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
}

void MMFilesPersistentIndexFeature::start() {
  Instance = this;

  if (!isEnabled()) {
    return;
  }

  auto* opts = ApplicationServer::getFeature<arangodb::RocksDBOptionFeature>("RocksDBOption");

  // set the database sub-directory for RocksDB
  auto database = ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _path = database->subdirectoryName("rocksdb");

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "initializing rocksdb for persistent indexes, path: " << _path;

  _comparator = new MMFilesPersistentIndexKeyComparator();

  rocksdb::BlockBasedTableOptions tableOptions;
  tableOptions.cache_index_and_filter_blocks = true;
  tableOptions.filter_policy.reset(rocksdb::NewBloomFilterPolicy(12, false));

  // TODO: using the prefix extractor will lead to the comparator being
  // called with just the key prefix (which the comparator currently cannot handle)
  // _options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(MMFilesPersistentIndex::minimalPrefixSize()));
  // _options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(tableOptions));

  _options.create_if_missing = true;
  _options.max_open_files = -1;
  _options.comparator = _comparator;

  _options.write_buffer_size = static_cast<size_t>(opts->_writeBufferSize);
  _options.max_write_buffer_number = static_cast<int>(opts->_maxWriteBufferNumber);
  _options.delayed_write_rate = opts->_delayedWriteRate;
  _options.min_write_buffer_number_to_merge = static_cast<int>(opts->_minWriteBufferNumberToMerge);
  _options.num_levels = static_cast<int>(opts->_numLevels);
  _options.max_bytes_for_level_base = opts->_maxBytesForLevelBase;
  _options.max_bytes_for_level_multiplier = static_cast<int>(opts->_maxBytesForLevelMultiplier);
  _options.optimize_filters_for_hits = opts->_optimizeFiltersForHits;

  _options.max_background_jobs = static_cast<int>(opts->_maxBackgroundJobs);
  _options.compaction_readahead_size = static_cast<size_t>(opts->_compactionReadaheadSize);
  if (_options.max_background_jobs > 1) {
    _options.env->SetBackgroundThreads(std::max(1, _options.max_background_jobs),
      rocksdb::Env::Priority::LOW);
  }

  //options.block_cache = rocksdb::NewLRUCache(100 * 1048576); // 100MB uncompressed cache
  //options.block_cache_compressed = rocksdb::NewLRUCache(100 * 1048576); // 100MB compressed cache

  rocksdb::Status status = rocksdb::OptimisticTransactionDB::Open(_options, _path, &_db);

  if (! status.ok()) {
    std::string error;
    if (status.IsIOError()) {
      error = "; Maybe your filesystem doesn't provide required features? (Cifs? NFS?)";
    }
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "unable to initialize RocksDB engine for persistent indexes: " << status.ToString() << error;
    FATAL_ERROR_EXIT();
  }
}

void MMFilesPersistentIndexFeature::unprepare() {
  if (!isEnabled()) {
    return;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "shutting down RocksDB for persistent indexes";

  // flush
  rocksdb::FlushOptions options;
  options.wait = true;
  rocksdb::Status status = _db->GetBaseDB()->Flush(options);

  if (! status.ok()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error flushing data to RocksDB for persistent indexes: " << status.ToString();
  }

  syncWal();
}

MMFilesPersistentIndexFeature* MMFilesPersistentIndexFeature::instance() {
  return Instance;
}

int MMFilesPersistentIndexFeature::syncWal() {
#ifndef _WIN32
  // SyncWAL() always reports a "not implemented" error on Windows
  if (Instance == nullptr || !Instance->isEnabled()) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "syncing RocksDB WAL for persistent indexes";

  rocksdb::Status status = Instance->db()->GetBaseDB()->SyncWAL();

  if (!status.ok()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error syncing RocksDB WAL for persistent indexes: " << status.ToString();
    return TRI_ERROR_INTERNAL;
  }
#endif
  return TRI_ERROR_NO_ERROR;
}

int MMFilesPersistentIndexFeature::dropDatabase(TRI_voc_tick_t databaseId) {
  if (Instance == nullptr) {
    return TRI_ERROR_INTERNAL;
  }
  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "dropping RocksDB database: " << databaseId;
  return Instance->dropPrefix(MMFilesPersistentIndex::buildPrefix(databaseId));
}

int MMFilesPersistentIndexFeature::dropCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
  if (Instance == nullptr) {
    return TRI_ERROR_INTERNAL;
  }
  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "dropping RocksDB database: " << databaseId << ", collection: " << collectionId;
  return Instance->dropPrefix(MMFilesPersistentIndex::buildPrefix(databaseId, collectionId));
}

int MMFilesPersistentIndexFeature::dropIndex(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, TRI_idx_iid_t indexId) {
  if (Instance == nullptr) {
    return TRI_ERROR_INTERNAL;
  }
  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "dropping RocksDB database: " << databaseId << ", collection: " << collectionId << ", index: " << indexId;
  return Instance->dropPrefix(MMFilesPersistentIndex::buildPrefix(databaseId, collectionId, indexId));
}

int MMFilesPersistentIndexFeature::dropPrefix(std::string const& prefix) {
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
    while (l.size() < MMFilesPersistentIndex::keyPrefixSize()) {
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
    while (u.size() < MMFilesPersistentIndex::keyPrefixSize()) {
      uint64_t value = UINT64_MAX;
      u.append(reinterpret_cast<char const*>(&value), sizeof(uint64_t));
    }
    u.append(builder.slice().startAs<char const>(), builder.slice().byteSize());

#if 0
    for (size_t i = 0; i < prefix.size(); i += sizeof(TRI_idx_iid_t)) {
      char const* x = prefix.c_str() + i;
      size_t o;
      char* q = TRI_EncodeHexString(x, 8, &o);
      if (q != nullptr) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "RocksDB prefix part: " << q;
        TRI_FreeString(TRI_CORE_MEM_ZONE, q);
      }
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "dropping RocksDB range: " << VPackSlice(l.c_str() + MMFilesPersistentIndex::keyPrefixSize()).toJson() << " - " << VPackSlice(u.c_str() + MMFilesPersistentIndex::keyPrefixSize()).toJson();
#endif

    // delete files in range lower..upper
    rocksdb::Slice lower(l.c_str(), l.size());
    rocksdb::Slice upper(u.c_str(), u.size());

    {
      rocksdb::Status status = rocksdb::DeleteFilesInRange(_db->GetBaseDB(), _db->GetBaseDB()->DefaultColumnFamily(), &lower, &upper);

      if (!status.ok()) {
        // if file deletion failed, we will still iterate over the remaining keys, so we
        // don't need to abort and raise an error here
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "RocksDB file deletion failed";
      }
    }

    // go on and delete the remaining keys (delete files in range does not necessarily
    // find them all, just complete files)

    auto comparator = MMFilesPersistentIndexFeature::instance()->comparator();
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
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "RocksDB key deletion failed: " << status.ToString();
      return TRI_ERROR_INTERNAL;
    }

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception during RocksDB key prefix deletion";
    return TRI_ERROR_INTERNAL;
  }
}
