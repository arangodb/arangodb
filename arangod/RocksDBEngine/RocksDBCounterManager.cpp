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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBCounterManager.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBValue.h"

#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/write_batch.h>

using namespace arangodb;

struct WBReader : public rocksdb::WriteBatch::Handler {
  void Put(const rocksdb::Slice& /*key*/, const rocksdb::Slice& /*value*/) override {
   
  }
  void Delete(const rocksdb::Slice& /*key*/) override {
   
  }
  void SingleDelete(const rocksdb::Slice& /*key*/) override {
   
  }
};

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBCounterManager::RocksDBCounterManager(uint64_t interval)
  : Thread("RocksDBCounters"),
  _interval(interval) {
  
}

uint64_t RocksDBCounterManager::loadCounter(uint64_t objectId) {
  {
    READ_LOCKER(guard, _rwLock);
    auto const& it = _counters.find(objectId);
    if (it != _counters.end()) {
      return it->second.count;
    }
  }
  return 0;// do not create
}

/// collections / views / indexes can call this method to update
/// their total counts. Thread-Safe needs the snapshot so we know
/// the sequence number used
void RocksDBCounterManager::updateCounter(uint64_t objectId,
                                          rocksdb::Snapshot const* snapshot,
                                          uint64_t counter) {
  {
    READ_LOCKER(guard, _rwLock);
    auto const& it = _counters.find(objectId);
    if (it != _counters.end()) {
      it->second = Counter {
        .sequenceNumber = snapshot->GetSequenceNumber(),
        .count = counter
      };
    }
    
  }
  {
    WRITE_LOCKER(guard, _rwLock);
    auto const& it = _counters.find(objectId);
    if (it == _counters.end()) {
      _counters[objectId] = Counter {
        .sequenceNumber = snapshot->GetSequenceNumber(),
        .count = counter
      };
    }
  }
}

/// Thread-Safe force sync
Result RocksDBCounterManager::sync() {
  if (_syncing) {
    return Result();
  }
  
  std::unordered_map<uint64_t, Counter> tmp;
  {// block all updates
    WRITE_LOCKER(guard, _rwLock);
    if (_syncing) {
      return Result();
    }
    _syncing = true;
    tmp = _counters;
  }
  
  rocksdb::WriteOptions writeOptions;
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  std::unique_ptr<rocksdb::Transaction> rtrx(db->BeginTransaction(writeOptions));
  
  VPackBuilder b;
  for (std::pair<uint64_t, Counter> const& pair : tmp) {
    b.clear();
    b.openArray();
    b.add(VPackValue(pair.second.sequenceNumber));
    b.add(VPackValue(pair.second.count));
    b.close();
    
    RocksDBKey key = RocksDBKey::CounterValue(pair.first);
    rocksdb::Status s = rtrx->Put(key.string(), rocksdb::Slice((char*)b.start(), b.size()));
    if (!s.ok()) {
      rtrx->Rollback();
      _syncing = false;
      return rocksutils::convertStatus(s);
    }
  }
  rocksdb::Status s = rtrx->Commit();
  _syncing = false;
  return rocksutils::convertStatus(s);
}

void RocksDBCounterManager::loadCounterValues() {
  
}

