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
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/write_batch.h>
#include <velocypack/Iterator.h>

using namespace arangodb;

RocksDBCounterManager::Counter::Counter(VPackSlice const& slice) {
  TRI_ASSERT(slice.isArray());
  
  velocypack::ArrayIterator array(slice);
  if (array.valid()) {
    this->_sequenceNumber = (*array).getUInt();
    this->_count = (*(++array)).getUInt();
    this->_revisionId = (*(++array)).getUInt();
  }
}

RocksDBCounterManager::Counter::Counter() : _sequenceNumber(0), _count(0), _revisionId(0) {}

void RocksDBCounterManager::Counter::serialize(VPackBuilder& b) const {
  b.openArray();
  b.add(VPackValue(this->_sequenceNumber));
  b.add(VPackValue(this->_count));
  b.add(VPackValue(this->_revisionId));
  b.close();
}

void RocksDBCounterManager::Counter::reset() {
  _sequenceNumber = 0;
  _count = 0;
  _revisionId = 0;
}

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
  
  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void RocksDBCounterManager::run() {
  while (!isStopping()) {
    CONDITION_LOCKER(guard, _condition);
    guard.wait(static_cast<uint64_t>(_interval * 1000000.0));

    if (!isStopping()) {
      this->sync();
    }
  }
}

RocksDBCounterManager::Counter const RocksDBCounterManager::loadCounter(uint64_t objectId) {
  static const Counter empty{};
  {
    READ_LOCKER(guard, _rwLock);
    auto const& it = _counters.find(objectId);
    if (it != _counters.end()) {
      return it->second;
    }
  }  return empty;  // do not create
}

/// collections / views / indexes can call this method to update
/// their total counts. Thread-Safe needs the snapshot so we know
/// the sequence number used
void RocksDBCounterManager::updateCounter(uint64_t objectId,
                                          rocksdb::Snapshot const* snapshot,
                                          int64_t delta, uint64_t revisionId) {

  LOG_TOPIC(ERR, Logger::DEVEL) << "UPDATE COUNTER: " <<  objectId;
  LOG_TOPIC(ERR, Logger::DEVEL) << "delta: " << delta;
  bool needsSync = false;
  {
    WRITE_LOCKER(guard, _rwLock);
    LOG_TOPIC(ERR, Logger::DEVEL) << "got lock: " <<  objectId;
    auto const& it = _counters.find(objectId);
    if (it != _counters.end()) {
      // already have a counter for objectId. now adjust its value
      it->second._count += delta;
      LOG_TOPIC(ERR, Logger::DEVEL) << "UPDATE COUNTER count: " << it->second._count;
      // just use the latest snapshot
      if (snapshot->GetSequenceNumber() > it->second._sequenceNumber) {
        it->second._sequenceNumber = snapshot->GetSequenceNumber();
        it->second._revisionId = revisionId;
      }
    } else {
      // insert new counter
      TRI_ASSERT(delta >= 0);
      auto tmpCounter = Counter(snapshot->GetSequenceNumber(), delta, revisionId);
      LOG_TOPIC(ERR, Logger::DEVEL) << "UPDATE COUNTER count: " <<  tmpCounter.count();
      _counters.emplace(std::make_pair(objectId,std::move(tmpCounter)));
      needsSync = true;
    }
  }
  if (needsSync) {
    sync();
  }
}

void RocksDBCounterManager::removeCounter(uint64_t objectId) {
  WRITE_LOCKER(guard, _rwLock);
  auto const& it = _counters.find(objectId);
  if (it != _counters.end()) {
    RocksDBKey key = RocksDBKey::CounterValue(it->first);
    rocksdb::WriteOptions options;
    rocksdb::Status s = _db->Delete(options, key.string());
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "Delete counter failed";
    }
    _counters.erase(it);
  }
}

/// Thread-Safe force sync
Result RocksDBCounterManager::sync() {
  LOG_TOPIC(ERR, Logger::DEVEL) << "ENTER SYNC";
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
    auto const& it = _syncedSeqNum.find(pair.first);
    if (it != _syncedSeqNum.end() && it->second == pair.second._sequenceNumber) {
      continue;
    }

    b.clear();
    pair.second.serialize(b);

    RocksDBKey key = RocksDBKey::CounterValue(pair.first);
    rocksdb::Slice value((char*)b.start(), b.size());
    rocksdb::Status s = rtrx->Put(key.string(), value);
    if (!s.ok()) {
      rtrx->Rollback();
      _syncing = false;
      return rocksutils::convertStatus(s);
    }
  }
  rocksdb::Status s = rtrx->Commit();
  if (s.ok()) {
    for (std::pair<uint64_t, Counter> const& pair : tmp) {
      _syncedSeqNum[pair.first] = pair.second._sequenceNumber;
    }
  }
  _syncing = false;
  return rocksutils::convertStatus(s);
  LOG_TOPIC(ERR, Logger::DEVEL) << "EXIT SYNC";
}

/// Parse counter values from rocksdb
void RocksDBCounterManager::readCounterValues() {
  WRITE_LOCKER(guard, _rwLock);
  
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::Comparator const* cmp = db->GetOptions().comparator;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CounterValues();

  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(db->NewIterator(readOptions));
  iter->Seek(bounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0) {
    
    uint64_t objectId = RocksDBKey::counterObjectId(iter->key());
    Counter counter(VPackSlice(iter->value().data()));
    _counters.emplace(objectId, counter);
    _syncedSeqNum[objectId] = counter._sequenceNumber;

    iter->Next();
  }
}

/// WAL parser, no locking required here, because we have been locked from the outside
struct WBReader : public rocksdb::WriteBatch::Handler {
  std::unordered_map<uint64_t, rocksdb::SequenceNumber> prevCounters;
  std::unordered_map<uint64_t, RocksDBCounterManager::Counter> deltas;
  rocksdb::SequenceNumber seqNum = UINT64_MAX;

  explicit WBReader() {}
  
  bool prepKey(const rocksdb::Slice& key) {
    
    if (RocksDBKey::type(key) == RocksDBEntryType::CounterValue) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      auto const& it = prevCounters.find(objectId);
      if (it == prevCounters.end()) {
        // We found a counter value, now we can track all writes from this
        // point in the WAL
        prevCounters.emplace(objectId, seqNum);
        deltas.emplace(objectId, RocksDBCounterManager::Counter());
      } else if (it->second < seqNum) {
        // we found our counter again at a later point in the WAL
        // which means we can forget about every write we have seen
        // up to this point
        TRI_ASSERT(deltas.find(objectId) != deltas.end());
        it->second = seqNum;
        deltas[objectId].reset();
      }
      return false;
    } else if (RocksDBKey::type(key) == RocksDBEntryType::Document) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      auto const& it = prevCounters.find(objectId);
      if (it == prevCounters.end()) {
        return false;
      } else {
        TRI_ASSERT(deltas.find(objectId) != deltas.end());
        return it->second < seqNum;
      }
    }
    return false;
  }

  void Put(const rocksdb::Slice& key,
           const rocksdb::Slice& /*value*/) override {
    if (prepKey(key)) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      uint64_t revisionId = RocksDBKey::revisionId(key);
      
      RocksDBCounterManager::Counter& cc = deltas[objectId];
      if (cc._sequenceNumber < seqNum) {
        cc._sequenceNumber = seqNum;
        cc._count++;
        cc._revisionId = revisionId;
      }
    }
  }

  void Delete(const rocksdb::Slice& key) override {
    if (prepKey(key)) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      uint64_t revisionId = RocksDBKey::revisionId(key);
      
      RocksDBCounterManager::Counter& cc = deltas[objectId];
      if (cc._sequenceNumber < seqNum) {
        cc._sequenceNumber = seqNum;
        cc._count--;
        cc._revisionId = revisionId;
      }
    }
  }

  void SingleDelete(const rocksdb::Slice& key) override {
    if (prepKey(key)) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      uint64_t revisionId = RocksDBKey::revisionId(key);
      
      RocksDBCounterManager::Counter& cc = deltas[objectId];
      if (cc._sequenceNumber < seqNum) {
        cc._sequenceNumber = seqNum;
        cc._count--;
        cc._revisionId = revisionId;
      }
    }
  }
};

/// parse the WAL with the above handler parser class
bool RocksDBCounterManager::parseRocksWAL() {
  WRITE_LOCKER(guard, _rwLock);
  TRI_ASSERT(_counters.size() > 0);

  // Determine which counter values might be outdated
  rocksdb::SequenceNumber minSeq = UINT64_MAX;
  for (auto const& it : _syncedSeqNum) {
    if (it.second < minSeq) {
      minSeq = it.second;
    }
  }

  std::unique_ptr<WBReader> handler(new WBReader());

  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  rocksdb::Status s = _db->GetUpdatesSince(minSeq, &iterator);
  if (!s.ok()) {  // TODO do something?
    return false;
  }
  while (iterator->Valid()) {
    rocksdb::BatchResult result = iterator->GetBatch();
    if (result.sequence > minSeq) {
      handler->seqNum = result.sequence;
      s = result.writeBatchPtr->Iterate(handler.get());
      if (!s.ok()) {
        break;
      }
    }
    iterator->Next();
  }
  
  for (std::pair<uint64_t, RocksDBCounterManager::Counter> pair : handler->deltas) {
    auto const& it = _counters.find(pair.first);
    if (it != _counters.end()
        && it->second._sequenceNumber < pair.second._sequenceNumber) {
      it->second._sequenceNumber = pair.second._sequenceNumber;
      it->second._count += pair.second._count;
      it->second._revisionId = pair.second._revisionId;
    }
  }
  return handler->deltas.size() > 0;
}
