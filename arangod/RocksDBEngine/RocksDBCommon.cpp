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
/// @author Daniel H. Larkin
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBCommon.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/StringRef.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"

#include <rocksdb/comparator.h>
#include <rocksdb/convenience.h>
#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Iterator.h>

namespace arangodb {
namespace rocksutils {

rocksdb::TransactionDB* globalRocksDB() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  RocksDBEngine* rocks = static_cast<RocksDBEngine*>(engine);
  TRI_ASSERT(rocks->db() != nullptr);
  return rocks->db();
}

RocksDBEngine* globalRocksEngine() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  return static_cast<RocksDBEngine*>(engine);
}

arangodb::Result globalRocksDBPut(rocksdb::ColumnFamilyHandle* cf,
                                  rocksdb::Slice const& key,
                                  rocksdb::Slice const& val,
                                  rocksdb::WriteOptions const& options) {
  TRI_ASSERT(cf != nullptr);
  auto status = globalRocksDB()->Put(options, cf, key, val);
  return convertStatus(status);
};

arangodb::Result globalRocksDBRemove(rocksdb::ColumnFamilyHandle* cf,
                                     rocksdb::Slice const& key,
                                     rocksdb::WriteOptions const& options) {
  TRI_ASSERT(cf != nullptr);
  auto status = globalRocksDB()->Delete(options, cf, key);
  return convertStatus(status);
};

uint64_t latestSequenceNumber() {
  auto seq = globalRocksDB()->GetLatestSequenceNumber();
  return static_cast<uint64_t>(seq);
};

void addCollectionMapping(uint64_t objectId, TRI_voc_tick_t did,
                          TRI_voc_cid_t cid) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  RocksDBEngine* rocks = static_cast<RocksDBEngine*>(engine);
  TRI_ASSERT(rocks->db() != nullptr);
  return rocks->addCollectionMapping(objectId, did, cid);
}

std::pair<TRI_voc_tick_t, TRI_voc_cid_t> mapObjectToCollection(
    uint64_t objectId) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  RocksDBEngine* rocks = static_cast<RocksDBEngine*>(engine);
  TRI_ASSERT(rocks->db() != nullptr);
  return rocks->mapObjectToCollection(objectId);
}

/// @brief count all keys in the given column family
std::size_t countKeys(rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);

  rocksdb::ReadOptions opts;
  opts.fill_cache = false;
  opts.total_order_seek = true;

  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(opts, cf));
  std::size_t count = 0;

  rocksdb::Slice lower("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
  it->Seek(lower);
  while (it->Valid()) {
    ++count;
    it->Next();
  }
  return count;
}

/// @brief iterate over all keys in range and count them
std::size_t countKeyRange(rocksdb::DB* db, 
                          RocksDBKeyBounds const& bounds,
                          bool prefix_same_as_start) {
  rocksdb::Slice lower(bounds.start());
  rocksdb::Slice upper(bounds.end());

  rocksdb::ReadOptions readOptions;
  readOptions.prefix_same_as_start = prefix_same_as_start;
  readOptions.iterate_upper_bound = &upper;
  readOptions.total_order_seek = !prefix_same_as_start;
  readOptions.verify_checksums = false;
  readOptions.fill_cache = false;

  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(readOptions, cf));
  std::size_t count = 0;

  it->Seek(lower);
  while (it->Valid() && cmp->Compare(it->key(), upper) < 0) {
    ++count;
    it->Next();
  }
  return count;
}

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
Result removeLargeRange(rocksdb::TransactionDB* db,
                        RocksDBKeyBounds const& bounds,
                        bool prefix_same_as_start) {
  LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "removing large range: " << bounds;
  
  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  rocksdb::DB* bDB = db->GetBaseDB();
  TRI_ASSERT(bDB != nullptr);

  try {
    // delete files in range lower..upper
    rocksdb::Slice lower(bounds.start());
    rocksdb::Slice upper(bounds.end());
    {
      rocksdb::Status status =
          rocksdb::DeleteFilesInRange(bDB, cf, &lower, &upper);
      if (!status.ok()) {
        // if file deletion failed, we will still iterate over the remaining
        // keys, so we don't need to abort and raise an error here
        arangodb::Result r = rocksutils::convertStatus(status);
        LOG_TOPIC(WARN, arangodb::Logger::ROCKSDB)
            << "RocksDB file deletion failed: " << r.errorMessage();
      }
    }
    
    // go on and delete the remaining keys (delete files in range does not
    // necessarily find them all, just complete files)
    rocksdb::Comparator const* cmp = cf->GetComparator();
    rocksdb::WriteBatch batch;
    rocksdb::ReadOptions readOptions;
    readOptions.iterate_upper_bound = &upper;
    readOptions.prefix_same_as_start = prefix_same_as_start;
    readOptions.total_order_seek = !prefix_same_as_start;
    readOptions.verify_checksums = false;
    readOptions.fill_cache = false;
    std::unique_ptr<rocksdb::Iterator> it(bDB->NewIterator(readOptions, cf));

    size_t total = 0;
    size_t counter = 0;
    for (it->Seek(lower); it->Valid(); it->Next()) {
      TRI_ASSERT(cmp->Compare(it->key(), lower) > 0);
      TRI_ASSERT(cmp->Compare(it->key(), upper) < 0);
      ++total;
      ++counter;
      batch.Delete(cf, it->key());
      if (counter == 1000) {
        LOG_TOPIC(DEBUG, Logger::FIXME) << "intermediate delete write";
        // Persist deletes all 1000 documents
        rocksdb::Status status = bDB->Write(rocksdb::WriteOptions(), &batch);
        if (!status.ok()) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME)
              << "RocksDB key deletion failed: " << status.ToString();
          return TRI_ERROR_INTERNAL;
        }
        batch.Clear();
        counter = 0;
      }
    }
  
    LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "removing large range deleted in total: " << total;

    if (counter > 0) {
      LOG_TOPIC(DEBUG, Logger::FIXME) << "intermediate delete write";
      // We still have sth to write
      // now apply deletion batch
      rocksdb::Status status = bDB->Write(rocksdb::WriteOptions(), &batch);

      if (!status.ok()) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "RocksDB key deletion failed: " << status.ToString();
        return TRI_ERROR_INTERNAL;
      }
    }
    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught unknown exception during RocksDB key prefix deletion";
    return TRI_ERROR_INTERNAL;
  }
}

std::vector<std::pair<RocksDBKey, RocksDBValue>> collectionKVPairs(
    TRI_voc_tick_t databaseId) {
  std::vector<std::pair<RocksDBKey, RocksDBValue>> rv;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::DatabaseCollections(databaseId);
  iterateBounds(bounds,
                [&rv](rocksdb::Iterator* it) {
                  rv.emplace_back(
                      RocksDBKey(it->key()),
                      RocksDBValue(RocksDBEntryType::Collection, it->value()));
                },
                arangodb::RocksDBColumnFamily::definitions());
  return rv;
}

std::vector<std::pair<RocksDBKey, RocksDBValue>> viewKVPairs(
    TRI_voc_tick_t databaseId) {
  std::vector<std::pair<RocksDBKey, RocksDBValue>> rv;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::DatabaseViews(databaseId);
  iterateBounds(bounds,
                [&rv](rocksdb::Iterator* it) {
                  rv.emplace_back(
                      RocksDBKey(it->key()),
                      RocksDBValue(RocksDBEntryType::View, it->value()));
                },
                arangodb::RocksDBColumnFamily::definitions());
  return rv;
}

}  // namespace rocksutils
}  // namespace arangodb
