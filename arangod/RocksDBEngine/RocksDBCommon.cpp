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

uint64_t uint64FromPersistent(char const* p) {
  uint64_t value = 0;
  uint64_t x = 0;
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(p);
  uint8_t const* end = ptr + sizeof(uint64_t);
  do {
    value += static_cast<uint64_t>(*ptr++) << x;
    x += 8;
  } while (ptr < end);
  return value;
}

void uint64ToPersistent(char* p, uint64_t value) {
  char* end = p + sizeof(uint64_t);
  do {
    *p++ = static_cast<uint8_t>(value & 0xffU);
    value >>= 8;
  } while (p < end);
}

void uint64ToPersistent(std::string& p, uint64_t value) {
  size_t len = 0;
  do {
    p.push_back(static_cast<char>(value & 0xffU));
    value >>= 8;
  } while (++len < sizeof(uint64_t));
}

uint32_t uint32FromPersistent(char const* p) {
  uint32_t value = 0;
  uint32_t x = 0;
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(p);
  uint8_t const* end = ptr + sizeof(uint32_t);
  do {
    value += static_cast<uint16_t>(*ptr++) << x;
    x += 8;
  } while (ptr < end);
  return value;
}

void uint32ToPersistent(char* p, uint32_t value) {
  char* end = p + sizeof(uint32_t);
  do {
    *p++ = static_cast<uint8_t>(value & 0xffU);
    value >>= 8;
  } while (p < end);
}

void uint32ToPersistent(std::string& p, uint32_t value) {
  size_t len = 0;
  do {
    p.push_back(static_cast<char>(value & 0xffU));
    value >>= 8;
  } while (++len < sizeof(uint32_t));
}

uint16_t uint16FromPersistent(char const* p) {
  uint16_t value = 0;
  uint16_t x = 0;
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(p);
  uint8_t const* end = ptr + sizeof(uint16_t);
  do {
    value += static_cast<uint16_t>(*ptr++) << x;
    x += 8;
  } while (ptr < end);
  return value;
}

void uint16ToPersistent(char* p, uint16_t value) {
  char* end = p + sizeof(uint16_t);
  do {
    *p++ = static_cast<uint8_t>(value & 0xffU);
    value >>= 8;
  } while (p < end);
}

void uint16ToPersistent(std::string& p, uint16_t value) {
  size_t len = 0;
  do {
    p.push_back(static_cast<char>(value & 0xffU));
    value >>= 8;
  } while (++len < sizeof(uint16_t));
}

RocksDBTransactionState* toRocksTransactionState(transaction::Methods* trx) {
  TRI_ASSERT(trx != nullptr);
  TransactionState* state = trx->state();
  TRI_ASSERT(state != nullptr);
  return static_cast<RocksDBTransactionState*>(state);
}

RocksDBMethods* toRocksMethods(transaction::Methods* trx) {
  TRI_ASSERT(trx != nullptr);
  TransactionState* state = trx->state();
  TRI_ASSERT(state != nullptr);
  return static_cast<RocksDBTransactionState*>(state)->rocksdbMethods();
}

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

arangodb::Result globalRocksDBPut(rocksdb::Slice const& key,
                                  rocksdb::Slice const& val,
                                  rocksdb::WriteOptions const& options) {
  auto status = globalRocksDB()->Put(options, key, val);
  return convertStatus(status);
};

arangodb::Result globalRocksDBRemove(rocksdb::Slice const& key,
                                     rocksdb::WriteOptions const& options) {
  auto status = globalRocksDB()->Delete(options, key);
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

std::size_t countKeyRange(rocksdb::DB* db, rocksdb::ReadOptions const& opts,
                          RocksDBKeyBounds const& bounds) {
  rocksdb::ColumnFamilyHandle* handle = bounds.columnFamily();
  rocksdb::Comparator const* cmp = db->GetOptions().comparator;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(opts, handle));
  std::size_t count = 0;

  rocksdb::Slice lower(bounds.start());
  rocksdb::Slice upper(bounds.end());
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
                        RocksDBKeyBounds const& bounds) {
  LOG_TOPIC(DEBUG, Logger::FIXME) << "removing large range: " << bounds;
  rocksdb::ColumnFamilyHandle* handle = bounds.columnFamily();
  try {
    // delete files in range lower..upper
    rocksdb::Slice lower(bounds.start());
    rocksdb::Slice upper(bounds.end());
    {
      rocksdb::Status status =
          rocksdb::DeleteFilesInRange(db->GetBaseDB(), handle, &lower, &upper);
      if (!status.ok()) {
        // if file deletion failed, we will still iterate over the remaining
        // keys, so we don't need to abort and raise an error here
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "RocksDB file deletion failed";
      }
    }

    // go on and delete the remaining keys (delete files in range does not
    // necessarily find them all, just complete files)
    rocksdb::Comparator const* cmp = handle->GetComparator();
    rocksdb::WriteBatch batch;
    rocksdb::ReadOptions readOptions;
    readOptions.fill_cache = false;
    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(readOptions, handle));

    // TODO: split this into multiple batches if batches get too big
    it->Seek(lower);
    size_t counter = 0;
    while (it->Valid() && cmp->Compare(it->key(), upper) < 0) {
      TRI_ASSERT(cmp->Compare(it->key(), lower) > 0);
      counter++;
      batch.Delete(it->key());
      it->Next();
      if (counter == 1000) {
        LOG_TOPIC(DEBUG, Logger::FIXME) << "Intermediate delete write";
        // Persist deletes all 1000 documents
        rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);
        if (!status.ok()) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME)
              << "RocksDB key deletion failed: " << status.ToString();
          return TRI_ERROR_INTERNAL;
        }
        batch.Clear();
        counter = 0;
      }
    }

    if (counter > 0) {
      LOG_TOPIC(DEBUG, Logger::FIXME) << "Remove large batch from bounds";
      // We still have sth to write
      // now apply deletion batch
      rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);

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
                arangodb::RocksDBColumnFamily::other());
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
                arangodb::RocksDBColumnFamily::other());
  return rv;
}

}  // namespace rocksutils
}  // namespace arangodb
