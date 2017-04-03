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

#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "Transaction/Methods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/convenience.h>
#include <rocksdb/comparator.h>
#include "Logger/Logger.h"

namespace arangodb {
namespace rocksutils {

arangodb::Result convertStatus(rocksdb::Status const& status, StatusHint hint) {
  switch (status.code()) {
    case rocksdb::Status::Code::kOk:
      return {TRI_ERROR_NO_ERROR};
    case rocksdb::Status::Code::kNotFound:
      switch (hint) {
        case StatusHint::collection:
          return {TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, status.ToString()};
        case StatusHint::database:
          return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, status.ToString()};
        case StatusHint::document:
          return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, status.ToString()};
        case StatusHint::index:
          return {TRI_ERROR_ARANGO_INDEX_NOT_FOUND, status.ToString()};
        case StatusHint::view:
          return {TRI_ERROR_ARANGO_VIEW_NOT_FOUND, status.ToString()};
        default:
          return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, status.ToString()};
      }
    case rocksdb::Status::Code::kCorruption:
      return {TRI_ERROR_ARANGO_CORRUPTED_DATAFILE, status.ToString()};
    case rocksdb::Status::Code::kNotSupported:
      return {TRI_ERROR_ILLEGAL_OPTION, status.ToString()};
    case rocksdb::Status::Code::kInvalidArgument:
      return {TRI_ERROR_BAD_PARAMETER, status.ToString()};
    case rocksdb::Status::Code::kIOError:
      return {TRI_ERROR_ARANGO_IO_ERROR, status.ToString()};
    case rocksdb::Status::Code::kMergeInProgress:
      return {TRI_ERROR_ARANGO_MERGE_IN_PROGRESS, status.ToString()};
    case rocksdb::Status::Code::kIncomplete:
      return {TRI_ERROR_NO_ERROR, status.ToString()};
    case rocksdb::Status::Code::kShutdownInProgress:
      return {TRI_ERROR_SHUTTING_DOWN, status.ToString()};
    case rocksdb::Status::Code::kTimedOut:
      return {TRI_ERROR_LOCK_TIMEOUT, status.ToString()};
    case rocksdb::Status::Code::kAborted:
      return {TRI_ERROR_TRANSACTION_ABORTED, status.ToString()};
    case rocksdb::Status::Code::kBusy:
      return {TRI_ERROR_ARANGO_BUSY, status.ToString()};
    case rocksdb::Status::Code::kExpired:
      return {TRI_ERROR_INTERNAL, "key expired; TTL was set in error"};
    case rocksdb::Status::Code::kTryAgain:
      return {TRI_ERROR_ARANGO_TRY_AGAIN, status.ToString()};
    default:
      return {TRI_ERROR_INTERNAL, "unknown RocksDB status code"};
  }
}

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
  
RocksDBTransactionState* toRocksTransactionState(transaction::Methods* trx) {
  TRI_ASSERT(trx != nullptr);
  TransactionState* state = trx->state();
  TRI_ASSERT(state != nullptr);
  return static_cast<RocksDBTransactionState*>(trx->state());
}

rocksdb::TransactionDB* globalRocksDB() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  RocksDBEngine *rocks = static_cast<RocksDBEngine*>(engine);
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
                                  rocksdb::WriteOptions const&) {
  auto status = globalRocksDB()->Put(rocksdb::WriteOptions{}, key, val);
  return convertStatus(status);
};

std::size_t countKeyRange(rocksdb::DB* db, rocksdb::ReadOptions const& opts,
                          RocksDBKeyBounds const& bounds) {
  const rocksdb::Comparator* cmp = db->GetOptions().comparator;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(opts));
  std::size_t count = 0;
  
  rocksdb::Slice lower(bounds.start());
  rocksdb::Slice upper(bounds.end());
  it->Seek(lower);
  while (it->Valid()) {
    if (cmp->Compare(it->key(), upper) != -1) {
      break;
    }
    ++count;
    it->Next();
  }
  return count;
}

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
int removeLargeRange(rocksdb::DB* db, RocksDBKeyBounds const& bounds) {
  
  try {
    
    // delete files in range lower..upper
    rocksdb::Slice lower(bounds.start());
    rocksdb::Slice upper(bounds.end());
    {
      rocksdb::Status status = rocksdb::DeleteFilesInRange(db, db->DefaultColumnFamily(),
                                                           &lower, &upper);
      if (!status.ok()) {
        // if file deletion failed, we will still iterate over the remaining keys, so we
        // don't need to abort and raise an error here
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "RocksDB file deletion failed";
      }
    }
    
    // go on and delete the remaining keys (delete files in range does not necessarily
    // find them all, just complete files)
    const rocksdb::Comparator *cmp = db->GetOptions().comparator;
    rocksdb::WriteBatch batch;
    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));
    
    it->Seek(lower);
    while (it->Valid()) {
      int res = cmp->Compare(it->key(), upper);
      
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
}  // namespace rocksutils
}  // namespace arangodb
