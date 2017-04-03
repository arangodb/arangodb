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

#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/write_batch.h>
#include <velocypack/Iterator.h>

using namespace arangodb;

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBCounterManager::RocksDBCounterManager(rocksdb::DB* db, double interval)
    : Thread("RocksDBCounters"), _db(db), _interval(interval) {
  
  readCounterValues();
  if (_counters.size() > 0) {
    if (parseRocksWAL()) {
      sync();
    }
  }  
}

void RocksDBCounterManager::beginShutdown() {
  Thread::beginShutdown();
  _condition.broadcast();
  // CONDITION_LOCKER(locker, _condition);
  // locker.signal();
}

void RocksDBCounterManager::run() {
  while (!isStopping()) {
    CONDITION_LOCKER(locker, _condition);
    locker.wait(static_cast<uint64_t>(_interval * 1000000.0));
    if (!isStopping()) {
      this->sync();
    }
  }
}

uint64_t RocksDBCounterManager::loadCounter(uint64_t objectId) {
  {
    READ_LOCKER(guard, _rwLock);
    auto const& it = _counters.find(objectId);
    if (it != _counters.end()) {
      return it->second.count;
    }
  }
  return 0;  // do not create
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
      it->second = Counter{.sequenceNumber = snapshot->GetSequenceNumber(),
                           .count = counter};
    }
  }
  {
    WRITE_LOCKER(guard, _rwLock);
    auto const& it = _counters.find(objectId);
    if (it == _counters.end()) {
      _counters[objectId] = Counter{
          .sequenceNumber = snapshot->GetSequenceNumber(), .count = counter};
    }
  }
}

void RocksDBCounterManager::removeCounter(uint64_t objectId) {
  WRITE_LOCKER(guard, _rwLock);
  auto const& it = _counters.find(objectId);
  if (it != _counters.end()) {
    RocksDBKey key = RocksDBKey::CounterValue(it->first);
    rocksdb::WriteOptions options;
    _db->Delete(options, key.string());
    _counters.erase(it);
  }
}

/// Thread-Safe force sync
Result RocksDBCounterManager::sync() {
  if (_syncing) {
    return Result();
  }

  std::unordered_map<uint64_t, Counter> tmp;
  {  // block all updates
    WRITE_LOCKER(guard, _rwLock);
    if (_syncing) {
      return Result();
    }
    _syncing = true;
    tmp = _counters;
  }

  rocksdb::WriteOptions writeOptions;
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  std::unique_ptr<rocksdb::Transaction> rtrx(
      db->BeginTransaction(writeOptions));

  VPackBuilder b;
  for (std::pair<uint64_t, Counter> const& pair : tmp) {
    // Skip values which we did not change
    auto const& it = _syncedCounters.find(pair.first);
    if (it != _syncedCounters.end() &&
        it->second.sequenceNumber == pair.second.sequenceNumber) {
      continue;
    }

    b.clear();
    b.openArray();
    b.add(VPackValue(pair.first));
    b.add(VPackValue(pair.second.sequenceNumber));
    b.add(VPackValue(pair.second.count));
    b.close();

    RocksDBKey key = RocksDBKey::CounterValue(pair.first);
    rocksdb::Status s =
        rtrx->Put(key.string(), rocksdb::Slice((char*)b.start(), b.size()));
    if (!s.ok()) {
      rtrx->Rollback();
      _syncing = false;
      return rocksutils::convertStatus(s);
    }
  }
  rocksdb::Status s = rtrx->Commit();
  if (s.ok()) {
    _syncedCounters = tmp;
  }
  _syncing = false;
  return rocksutils::convertStatus(s);
}

void RocksDBCounterManager::readCounterValues() {
  WRITE_LOCKER(guard, _rwLock);
  
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::Comparator const* cmp = db->GetOptions().comparator;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CounterValues();

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(db->NewIterator(readOptions));
  iter->Seek(bounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) == -1) {
    rocksdb::Slice sl = iter->value();
    velocypack::ArrayIterator array(VPackSlice(sl.data()));
    if (array.valid()) {
      uint64_t objectId = (*array).getUInt();
      uint64_t sequenceNumber = (*(++array)).getUInt();
      uint64_t count = (*(++array)).getUInt();
      _counters.emplace(
          objectId, Counter{.sequenceNumber = sequenceNumber, .count = count});
    }

    iter->Next();
  }
  // update synced counters
  _syncedCounters = _counters;
}

struct WBReader : public rocksdb::WriteBatch::Handler {
  std::unordered_map<uint64_t, RocksDBCounterManager::Counter>& counters;
  rocksdb::SequenceNumber seqNum = UINT64_MAX;
  bool recovered = false;

  WBReader(std::unordered_map<uint64_t, RocksDBCounterManager::Counter>& cnts)
      : counters(cnts) {}

  void Put(const rocksdb::Slice& key,
           const rocksdb::Slice& /*value*/) override {
    uint64_t obj = RocksDBKey::extractObjectId(key);
    auto it = counters.find(obj);
    if (it != counters.end() && it->second.sequenceNumber < seqNum) {
      it->second.count++;
      recovered = true;
    }
  }
  void Delete(const rocksdb::Slice& key) override {
    uint64_t obj = RocksDBKey::extractObjectId(key);
    auto it = counters.find(obj);
    if (it != counters.end() && it->second.sequenceNumber < seqNum) {
      it->second.count--;
      recovered = true;
    }
  }
  void SingleDelete(const rocksdb::Slice& key) override {
    uint64_t obj = RocksDBKey::extractObjectId(key);
    auto it = counters.find(obj);
    if (it != counters.end() && it->second.sequenceNumber < seqNum) {
      it->second.count--;
      recovered = true;
    }
  }
};

// No locking in this one
bool RocksDBCounterManager::parseRocksWAL() {
  TRI_ASSERT(_counters.size() != 0);

  rocksdb::SequenceNumber minSeq = UINT64_MAX;
  for (auto const& it : _syncedCounters) {
    if (it.second.sequenceNumber < minSeq) {
      minSeq = it.second.sequenceNumber;
    }
  }

  std::unique_ptr<WBReader> handler(new WBReader(_counters));

  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  rocksdb::Status s = _db->GetUpdatesSince(minSeq, &iterator);
  if (!s.ok()) {  // TODO do something?
    return false;
  }
  while (iterator->Valid()) {
    rocksdb::BatchResult result = iterator->GetBatch();
    if (result.sequence <= minSeq) {
      continue;
    }

    handler->seqNum = result.sequence;
    s = result.writeBatchPtr->Iterate(handler.get());
    if (!s.ok()) {
      break;
    }
    iterator->Next();
  }
  return handler->recovered;
}
