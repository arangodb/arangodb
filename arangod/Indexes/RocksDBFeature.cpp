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
#include "RestServer/DatabaseFeature.h"
  
#include <rocksdb/db.h>
#include <rocksdb/convenience.h>
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
      _db(nullptr), _comparator(nullptr), _path(), _active(true) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("LogfileManager");
}

RocksDBFeature::~RocksDBFeature() {
  delete _db;
  delete _comparator;
}

void RocksDBFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "Configure the RocksDB engine");

  options->addOption(
      "--rocksdb.enabled",
      "Whether or not the RocksDB engine is enabled", 
      new BooleanParameter(&_active));
}

void RocksDBFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_active) {
    forceDisable();
  }
}

void RocksDBFeature::start() {
  Instance = this;

  if (!isEnabled()) {
    return;
  }

  // set the database path
  DatabaseFeature* database = ApplicationServer::getFeature<DatabaseFeature>("Database");

  _path = arangodb::basics::FileUtils::buildFilename(database->directory(), "rocksdb");
  
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
  
  //options.block_cache = rocksdb::NewLRUCache(100 * 1048576); // 100MB uncompressed cache
  //options.block_cache_compressed = rocksdb::NewLRUCache(100 * 1048576); // 100MB compressed cache
  //options.compression = rocksdb::kLZ4Compression;
  //options.write_buffer_size = 32 << 20;
  //options.max_write_buffer_number = 2;
  //options.min_write_buffer_number_to_merge = 1;
  //options.disableDataSync = 1;
  //options.bytes_per_sync = 2 << 20;

  //options.env->SetBackgroundThreads(num_threads, Env::Priority::HIGH);
  //options.env->SetBackgroundThreads(num_threads, Env::Priority::LOW);

  rocksdb::Status status = rocksdb::OptimisticTransactionDB::Open(_options, _path, &_db);
  
  if (! status.ok()) {
    LOG(FATAL) << "unable to initialize rocksdb: " << status.ToString();
    FATAL_ERROR_EXIT();
  }
}

void RocksDBFeature::stop() {
  if (!isEnabled()) {
    return;
  }

  LOG(TRACE) << "shutting down rocksdb";

  // flush
  rocksdb::FlushOptions options;
  options.wait = true;
  rocksdb::Status status = _db->GetBaseDB()->Flush(options);
  
  if (! status.ok()) {
    LOG(ERR) << "error flushing rocksdb: " << status.ToString();
  }

  syncWal();
}
  
RocksDBFeature* RocksDBFeature::instance() {
  return Instance;
}

int RocksDBFeature::syncWal() {
  if (Instance == nullptr || !Instance->isEnabled()) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG(TRACE) << "syncing rocksdb WAL";

  rocksdb::Status status = Instance->db()->GetBaseDB()->SyncWAL();

  if (! status.ok()) {
    LOG(ERR) << "error syncing rocksdb WAL: " << status.ToString();
    return TRI_ERROR_INTERNAL;
  }

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
    l.append(builder.slice().startAs<char const>(), builder.slice().byteSize());
    
    builder.clear();
    builder.openArray();
    builder.add(VPackSlice::maxKeySlice());
    builder.close();  
    
    std::string u;
    u.reserve(prefix.size() + builder.slice().byteSize());
    u.append(prefix);
    u.append(builder.slice().startAs<char const>(), builder.slice().byteSize());
 
#if 0 
    for (size_t i = 0; i < prefix.size(); i += sizeof(TRI_idx_iid_t)) {
      char const* x = prefix.c_str() + i;
      LOG(TRACE) << "prefix part: " << std::to_string(*reinterpret_cast<uint64_t const*>(x));
    }
#endif

    LOG(TRACE) << "dropping range: " << VPackSlice(l.c_str() + prefix.size()).toJson() << " - " << VPackSlice(u.c_str() + prefix.size()).toJson();

    rocksdb::Slice lower(l.c_str(), l.size());
    rocksdb::Slice upper(u.c_str(), u.size());
    
    rocksdb::Status status = rocksdb::DeleteFilesInRange(_db->GetBaseDB(), _db->GetBaseDB()->DefaultColumnFamily(), &lower, &upper);

    if (!status.ok()) {
      // if file deletion failed, we will still iterate over the remaining keys, so we
      // don't need to abort and raise an error here
      LOG(WARN) << "rocksdb file deletion failed";
    }

    // go on and delete the remaining keys (delete files in range does not necessarily
    // find them all, just complete files
    
    auto comparator = RocksDBFeature::instance()->comparator();
    rocksdb::DB* db = _db->GetBaseDB();

    rocksdb::WriteBatch batch;
    
    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));

    it->Seek(lower);
    while (it->Valid()) {
      batch.Delete(it->key());
      
      int res = comparator->Compare(it->key(), upper);

      if (res >= 0) {
        break;
      }

      it->Next();
    }
    
    // now apply deletion batch
    status = db->Write(rocksdb::WriteOptions(), &batch);

    if (!status.ok()) {
      LOG(WARN) << "rocksdb key deletion failed";
      return TRI_ERROR_INTERNAL;
    }

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    LOG(ERR) << "caught exception during prefix deletion: " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception during prefix deletion: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(ERR) << "caught unknown exception during prefix deletion";
    return TRI_ERROR_INTERNAL;
  }
}

