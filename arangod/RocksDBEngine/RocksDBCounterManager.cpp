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
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBCounterManager::CMValue::CMValue(VPackSlice const& slice) {
  if (!slice.isArray()) {
    // got a somewhat invalid slice. probably old data from before the key structure changes
    return;
  }
  TRI_ASSERT(slice.isArray());

  velocypack::ArrayIterator array(slice);
  if (array.valid()) {
    this->_sequenceNum = (*array).getUInt();
    this->_count = (*(++array)).getUInt();
    this->_revisionId = (*(++array)).getUInt();
  }
}

void RocksDBCounterManager::CMValue::serialize(VPackBuilder& b) const {
  b.openArray();
  b.add(VPackValue(this->_sequenceNum));
  b.add(VPackValue(this->_count));
  b.add(VPackValue(this->_revisionId));
  b.close();
}

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBCounterManager::RocksDBCounterManager(rocksdb::DB* db)
    : _syncing(false), _db(db) {
  readCounterValues();
  if (!_counters.empty()) {
    if (parseRocksWAL()) {
      sync(false);
    }
  }
}

RocksDBCounterManager::CounterAdjustment RocksDBCounterManager::loadCounter(
    uint64_t objectId) const {
  TRI_ASSERT(objectId != 0);  // TODO fix this

  READ_LOCKER(guard, _rwLock);
  auto const& it = _counters.find(objectId);
  if (it != _counters.end()) {
    return CounterAdjustment(it->second._sequenceNum, it->second._count, 0,
                             it->second._revisionId);
  }
  return CounterAdjustment();  // do not create
}

/// collections / views / indexes can call this method to update
/// their total counts. Thread-Safe needs the snapshot so we know
/// the sequence number used
void RocksDBCounterManager::updateCounter(uint64_t objectId,
                                          CounterAdjustment const& update) {
  // From write_batch.cc in rocksdb: first 64 bits in the internal rep_
  // buffer are the sequence number
  /*TRI_ASSERT(trx->GetState() == rocksdb::Transaction::COMMITED);
  rocksdb::WriteBatchWithIndex *batch = trx->GetWriteBatch();
  rocksdb::SequenceNumber seq =
  DecodeFixed64(batch->GetWriteBatch()->Data().data());*/

  bool needsSync = false;
  {
    WRITE_LOCKER(guard, _rwLock);

    auto it = _counters.find(objectId);
    if (it != _counters.end()) {
      it->second._count += update.added();
      it->second._count -= update.removed();
      // just use the latest trx info
      if (update.sequenceNumber() > it->second._sequenceNum) {
        it->second._sequenceNum = update.sequenceNumber();
        it->second._revisionId = update.revisionId();
      }
    } else {
      // insert new counter
      _counters.emplace(std::make_pair(
          objectId,
          CMValue(update.sequenceNumber(), update.added() - update.removed(),
                  update.revisionId())));
      needsSync = true;  // only count values from WAL if they are in the DB
    }
  }
  if (needsSync) {
    sync(true);
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
      LOG_TOPIC(ERR, Logger::ENGINES) << "deleting counter failed";
    }
    _counters.erase(it);
  }
}

/// Thread-Safe force sync
Result RocksDBCounterManager::sync(bool force) {
  if (force) {
    while(true) {
      bool expected = false;
      bool res = _syncing.compare_exchange_strong(expected, true, std::memory_order_acquire,
                                                  std::memory_order_relaxed);
      if (res) {
        break;
      }
      usleep(10000);
    }
  } else {
    bool expected = false;

    if (!_syncing.compare_exchange_strong(expected, true, std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
      return Result();
    }
  }
  TRI_DEFER(_syncing = false);
  std::unordered_map<uint64_t, CMValue> copy;
  {  // block all updates
    WRITE_LOCKER(guard, _rwLock);
    copy = _counters;
  }

  rocksdb::WriteOptions writeOptions;
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  std::unique_ptr<rocksdb::Transaction> rtrx(
      db->BeginTransaction(writeOptions));

  VPackBuilder b;
  for (std::pair<uint64_t, CMValue> const& pair : copy) {
    // Skip values which we did not change
    auto const& it = _syncedSeqNums.find(pair.first);
    if (it != _syncedSeqNums.end() && it->second == pair.second._sequenceNum) {
      continue;
    }

    b.clear();
    pair.second.serialize(b);

    RocksDBKey key = RocksDBKey::CounterValue(pair.first);
    rocksdb::Slice value((char*)b.start(), b.size());
    rocksdb::Status s = rtrx->Put(key.string(), value);
    if (!s.ok()) {
      rtrx->Rollback();
      return rocksutils::convertStatus(s);
    }
  }

  // we have to commit all counters in one batch otherwise
  // there would be the possibility of
  rocksdb::Status s = rtrx->Commit();
  if (s.ok()) {
    for (std::pair<uint64_t, CMValue> const& pair : copy) {
      _syncedSeqNums[pair.first] = pair.second._sequenceNum;
    }
  }

  return rocksutils::convertStatus(s);
}

/// Parse counter values from rocksdb
void RocksDBCounterManager::readCounterValues() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CounterValues();

  rocksdb::Comparator const* cmp = _db->GetOptions().comparator;
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions));
  iter->Seek(bounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0) {
    uint64_t objectId = RocksDBKey::counterObjectId(iter->key());
    auto const& it =
        _counters.emplace(objectId, CMValue(VPackSlice(iter->value().data())));
    _syncedSeqNums[objectId] = it.first->second._sequenceNum;

    iter->Next();
  }
}

/// WAL parser, no locking required here, because we have been locked from the
/// outside
struct WBReader : public rocksdb::WriteBatch::Handler {
  // must be set by the counter manager
  std::unordered_map<uint64_t, rocksdb::SequenceNumber> seqStart;
  std::unordered_map<uint64_t, RocksDBCounterManager::CounterAdjustment> deltas;
  rocksdb::SequenceNumber currentSeqNum;

  explicit WBReader() {}

  bool prepKey(const rocksdb::Slice& key) {
    if (RocksDBKey::type(key) == RocksDBEntryType::Document) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      auto const& it = seqStart.find(objectId);
      if (it != seqStart.end()) {
        if (deltas.find(objectId) == deltas.end()) {
          deltas.emplace(objectId, RocksDBCounterManager::CounterAdjustment());
        }
        return it->second <= currentSeqNum;
      }
    }
    return false;
  }

  void Put(const rocksdb::Slice& key,
           const rocksdb::Slice& /*value*/) override {
    if (prepKey(key)) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      uint64_t revisionId = RocksDBKey::revisionId(key);

      auto const& it = deltas.find(objectId);
      if (it != deltas.end()) {
        it->second._sequenceNum = currentSeqNum;
        it->second._added++;
        it->second._revisionId = revisionId;
      }
    }
  }

  void Delete(const rocksdb::Slice& key) override {
    if (prepKey(key)) {
      uint64_t objectId = RocksDBKey::counterObjectId(key);
      uint64_t revisionId = RocksDBKey::revisionId(key);

      auto const& it = deltas.find(objectId);
      if (it != deltas.end()) {
        it->second._sequenceNum = currentSeqNum;
        it->second._removed++;
        it->second._revisionId = revisionId;
      }
    }
  }

  void SingleDelete(const rocksdb::Slice& key) override {
    Delete(key);
  }
};

/// parse the WAL with the above handler parser class
bool RocksDBCounterManager::parseRocksWAL() {
  WRITE_LOCKER(guard, _rwLock);
  TRI_ASSERT(_counters.size() > 0);

  rocksdb::SequenceNumber start = UINT64_MAX;
  // Tell the WriteBatch reader the transaction markers to look for
  std::unique_ptr<WBReader> handler(new WBReader());
  for (auto const& pair : _counters) {
    handler->seqStart.emplace(pair.first, pair.second._sequenceNum);
    start = std::min(start, pair.second._sequenceNum);
  }

  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  rocksdb::Status s = _db->GetUpdatesSince(start, &iterator);
  if (!s.ok()) {  // TODO do something?
    return false;
  }

  while (iterator->Valid()) {
    s = iterator->status();
    if (s.ok()) {
      rocksdb::BatchResult batch = iterator->GetBatch();
      start = batch.sequence;
      handler->currentSeqNum = start;
      s = batch.writeBatchPtr->Iterate(handler.get());
    }
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "error during WAL scan";
      break;
    }

    iterator->Next();
  }

  LOG_TOPIC(TRACE, Logger::ENGINES) << "finished WAL scan with "
                                   << handler->deltas.size();
  for (std::pair<uint64_t, RocksDBCounterManager::CounterAdjustment> pair :
       handler->deltas) {
    auto const& it = _counters.find(pair.first);
    if (it != _counters.end()) {
      it->second._sequenceNum = start;
      it->second._count += pair.second.added();
      it->second._count -= pair.second.removed();
      it->second._revisionId = pair.second._revisionId;
      LOG_TOPIC(TRACE, Logger::ENGINES)
          << "WAL recovered " << pair.second.added() << " PUTs and "
          << pair.second.removed() << " DELETEs for a total of "
          << it->second._count;
    }
  }
  return handler->deltas.size() > 0;
}
