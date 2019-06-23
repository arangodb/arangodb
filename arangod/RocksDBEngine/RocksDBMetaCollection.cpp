////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////


#include "RocksDBMetaCollection.h"

#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Methods.h"
#include "Utils/OperationOptions.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             VPackSlice const& info)
: PhysicalCollection(collection, info),
  _objectId(basics::VelocyPackHelper::stringUInt64(info, "objectId")) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  VPackSlice s = info.get("isVolatile");
  if (s.isBoolean() && s.getBoolean()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "volatile collections are unsupported in the RocksDB engine");
  }
  
  TRI_ASSERT(_logicalCollection.isAStub() || _objectId != 0);
  rocksutils::globalRocksEngine()->addCollectionMapping(_objectId, _logicalCollection.vocbase().id(),
                                                        _logicalCollection.id());
}

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             PhysicalCollection const* physical)
: PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
  _objectId(static_cast<RocksDBMetaCollection const*>(physical)->_objectId) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  rocksutils::globalRocksEngine()->addCollectionMapping(_objectId, _logicalCollection.vocbase().id(), _logicalCollection.id());
}

std::string const& RocksDBMetaCollection::path() const {
  return StaticStrings::Empty;  // we do not have any path
}

TRI_voc_rid_t RocksDBMetaCollection::revision(transaction::Methods* trx) const {
  auto* state = RocksDBTransactionState::toState(trx);
  auto trxCollection = static_cast<RocksDBTransactionCollection*>(
                                                                  state->findCollection(_logicalCollection.id()));
  
  TRI_ASSERT(trxCollection != nullptr);
  
  return trxCollection->revision();
}

uint64_t RocksDBMetaCollection::numberDocuments(transaction::Methods* trx) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto* state = RocksDBTransactionState::toState(trx);
  auto trxCollection = static_cast<RocksDBTransactionCollection*>(
                                                                  state->findCollection(_logicalCollection.id()));
  
  TRI_ASSERT(trxCollection != nullptr);
  
  return trxCollection->numberDocuments();
}

void RocksDBMetaCollection::invokeOnAllElements(transaction::Methods* trx,
                                                std::function<bool(LocalDocumentId const&)> callback) {
  std::unique_ptr<IndexIterator> cursor(this->getAllIterator(trx));
  bool cnt = true;
  auto cb = [&](LocalDocumentId token) {
    if (cnt) {
      cnt = callback(token);
    }
  };
  
  while (cursor->next(cb, 1000) && cnt) {
  }
}

/// @brief write locks a collection, with a timeout
int RocksDBMetaCollection::lockWrite(double timeout) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;
  
  while (true) {
    TRY_WRITE_LOCKER(locker, _exclusiveLock);
    
    if (locker.isLocked()) {
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }
    
    double now = TRI_microtime();
    
    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      
      startTime = now;
      waitTime = 1;
    }
    
    if (now > startTime + timeout) {
      LOG_TOPIC("d1e53", TRACE, arangodb::Logger::ENGINES)
      << "timed out after " << timeout << " s waiting for write-lock on collection '"
      << _logicalCollection.name() << "'";
      
      return TRI_ERROR_LOCK_TIMEOUT;
    }
    
    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief write unlocks a collection
void RocksDBMetaCollection::unlockWrite() { _exclusiveLock.unlockWrite(); }

/// @brief read locks a collection, with a timeout
int RocksDBMetaCollection::lockRead(double timeout) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;
  
  while (true) {
    TRY_READ_LOCKER(locker, _exclusiveLock);
    
    if (locker.isLocked()) {
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }
    
    double now = TRI_microtime();
    
    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      
      startTime = now;
      waitTime = 1;
    }
    
    if (now > startTime + timeout) {
      LOG_TOPIC("dcbd2", TRACE, arangodb::Logger::ENGINES)
      << "timed out after " << timeout << " s waiting for read-lock on collection '"
      << _logicalCollection.name() << "'";
      
      return TRI_ERROR_LOCK_TIMEOUT;
    }
    
    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
      
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief read unlocks a collection
void RocksDBMetaCollection::unlockRead() { _exclusiveLock.unlockRead(); }


void RocksDBMetaCollection::trackWaitForSync(arangodb::transaction::Methods* trx,
                                             OperationOptions& options) {
  if (_logicalCollection.waitForSync() && !options.isRestore) {
    options.waitForSync = true;
  }
  
  if (options.waitForSync) {
    trx->state()->waitForSync(true);
  }
}

// rescans the collection to update document count
uint64_t RocksDBMetaCollection::recalculateCounts() {
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::TransactionDB* db = engine->db();
  const rocksdb::Snapshot* snapshot = nullptr;
  // start transaction to get a collection lock
  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  if (!vocbase.use()) {  // someone dropped the database
    return _meta.numberDocuments();
  }
  auto useGuard = scopeGuard([&] {
    if (snapshot) {
      db->ReleaseSnapshot(snapshot);
    }
    vocbase.release();
  });
  
  TRI_vocbase_col_status_e status;
  int res = vocbase.useCollection(&_logicalCollection, status);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  auto collGuard =
  scopeGuard([&] { vocbase.releaseCollection(&_logicalCollection); });
  
  uint64_t snapNumberOfDocuments = 0;
  {
    // fetch number docs and snapshot under exclusive lock
    // this should enable us to correct the count later
    auto lockGuard = scopeGuard([this] { unlockWrite(); });
    res = lockWrite(transaction::Options::defaultLockTimeout);
    if (res != TRI_ERROR_NO_ERROR) {
      lockGuard.cancel();
      THROW_ARANGO_EXCEPTION(res);
    }
    
    snapNumberOfDocuments = _meta.numberDocuments();
    snapshot = engine->db()->GetSnapshot();
    TRI_ASSERT(snapshot);
  }
  
  // count documents
  RocksDBKeyBounds bounds = this->bounds();
  rocksdb::Slice upper(bounds.end());
  
  rocksdb::ReadOptions ro;
  ro.snapshot = snapshot;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;
  
  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(ro, cf));
  std::size_t count = 0;
  
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    ++count;
  }
  
  int64_t adjustment = snapNumberOfDocuments - count;
  if (adjustment != 0) {
    LOG_TOPIC("ad6d3", WARN, Logger::REPLICATION)
    << "inconsistent collection count detected, "
    << "an offet of " << adjustment << " will be applied";
    _meta.adjustNumberDocuments(0, static_cast<TRI_voc_rid_t>(0), adjustment);
  }
  
  return _meta.numberDocuments();
}

Result RocksDBMetaCollection::compact() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = this->bounds();
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, bounds.columnFamily(), &b, &e);
  
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    index->compact();
  }
  
  return {};
}

void RocksDBMetaCollection::estimateSize(velocypack::Builder& builder) {
  TRI_ASSERT(!builder.isOpenObject() && !builder.isOpenArray());
  
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = this->bounds();
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out = 0, total = 0;
  db->GetApproximateSizes(bounds.columnFamily(), &r, 1, &out,
                          static_cast<uint8_t>(
                                               rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                                               rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));
  total += out;
  
  builder.openObject();
  builder.add("documents", VPackValue(out));
  builder.add("indexes", VPackValue(VPackValueType::Object));
  
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    out = index->memory();
    builder.add(std::to_string(index->id()), VPackValue(out));
    total += out;
  }
  builder.close();
  builder.add("total", VPackValue(total));
  builder.close();
}
