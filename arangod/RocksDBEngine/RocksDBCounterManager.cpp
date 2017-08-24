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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEdgeIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBVPackIndex.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;

RocksDBCounterManager::CMValue::CMValue(VPackSlice const& slice)
    : _sequenceNum(0), _count(0), _revisionId(0) {
  if (!slice.isArray()) {
    // got a somewhat invalid slice. probably old data from before the key
    // structure changes
    return;
  }

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
    : _lastSync(0),_syncing(false), _db(db) {
  readSettings();
  readIndexEstimates();
  readCounterValues();
  readKeyGenerators();
}

/// parse recent RocksDB WAL entries and notify the
/// DatabaseFeature about the successful recovery
void RocksDBCounterManager::runRecovery() {
  if (!_counters.empty()) {
    if (parseRocksWAL()) {
      // TODO: what do we do if parseRocksWAL returns false?
      sync(false);
    }
  }

  // notify everyone that recovery is now done
  auto databaseFeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
  databaseFeature->recoveryDone();
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

arangodb::Result RocksDBCounterManager::setAbsoluteCounter(uint64_t objectId,
                                                           uint64_t value) {
  arangodb::Result res;
  WRITE_LOCKER(guard, _rwLock);
  auto it = _counters.find(objectId);
  if (it != _counters.end()) {
    it->second._count = value;
  } else {
    // nothing to do as the counter has never been written it can not be set to
    // a value that would require correction. but we use the return value to
    // signal that no sync is rquired
    res.reset(TRI_ERROR_INTERNAL, "counter value not found - no sync required");
  }
  return res;
}

void RocksDBCounterManager::removeCounter(uint64_t objectId) {
  WRITE_LOCKER(guard, _rwLock);
  auto const& it = _counters.find(objectId);
  if (it != _counters.end()) {
    RocksDBKey key;
    key.constructCounterValue(it->first);
    rocksdb::WriteOptions options;
    rocksdb::Status s = _db->Delete(options, RocksDBColumnFamily::definitions(), key.string());
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "deleting counter failed";
    }
    _counters.erase(it);
  }
}

/// Thread-Safe force sync
Result RocksDBCounterManager::sync(bool force) {
  TRI_IF_FAILURE("RocksDBCounterManagerSync") {
    return Result();
  }

  if (force) {
    while (true) {
      bool expected = false;
      bool res = _syncing.compare_exchange_strong(
          expected, true, std::memory_order_acquire, std::memory_order_relaxed);
      if (res) {
        break;
      }
      usleep(10000);
    }
  } else {
    bool expected = false;

    if (!_syncing.compare_exchange_strong(expected, true,
                                          std::memory_order_acquire,
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
  auto seqNumber = db->GetLatestSequenceNumber();
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

    RocksDBKey key;
    key.constructCounterValue(pair.first);
    rocksdb::Slice value((char*)b.start(), b.size());
    rocksdb::Status s = rtrx->Put(RocksDBColumnFamily::definitions(),
                                  key.string(), value);
    if (!s.ok()) {
      rtrx->Rollback();
      auto rStatus = rocksutils::convertStatus(s);
      LOG_TOPIC(WARN, Logger::ENGINES) << "writing counters failed: " << rStatus.errorMessage();
      return rStatus;
    }
  }

  // now write global settings
  b.clear();
  b.openObject();
  b.add("tick", VPackValue(std::to_string(TRI_CurrentTickServer())));
  b.add("hlc", VPackValue(std::to_string(TRI_HybridLogicalClock())));
  b.add("lastSync", VPackValue(std::to_string(seqNumber)));
  b.close();

  VPackSlice slice = b.slice();
  LOG_TOPIC(TRACE, Logger::ENGINES) << "writing settings: " << slice.toJson();

  RocksDBKey key;
  key.constructSettingsValue(RocksDBSettingsType::ServerTick);
  rocksdb::Slice value(slice.startAs<char>(), slice.byteSize());

  rocksdb::Status s = rtrx->Put(RocksDBColumnFamily::definitions(),
                                key.string(), value);

  if (!s.ok()) {
    auto rStatus = rocksutils::convertStatus(s);
    LOG_TOPIC(WARN, Logger::ENGINES) << "writing settings failed: " << rStatus.errorMessage();
    rtrx->Rollback();
    return rStatus;
  }

  // Now persist the index estimates and key generators
  {
    for (auto const& pair : copy) {
      auto dbColPair = rocksutils::mapObjectToCollection(pair.first);
      if (dbColPair.second == 0 && dbColPair.first == 0) {
        // collection with this objectID not known.Skip.
        continue;
      }
      auto dbfeature =
          ApplicationServer::getFeature<DatabaseFeature>("Database");
      TRI_ASSERT(dbfeature != nullptr);
      auto vocbase = dbfeature->useDatabase(dbColPair.first);
      if (vocbase == nullptr) {
        // Bad state, we have references to a database that is not known
        // anymore.
        // However let's just skip in production. Not allowed to crash.
        // If we cannot find this infos during recovery we can either recompute
        // or start fresh.
        continue;
      }
      TRI_DEFER(vocbase->release());

      auto collection = vocbase->lookupCollection(dbColPair.second);
      if (collection == nullptr) {
        // Bad state, we have references to a collection that is not known
        // anymore.
        // However let's just skip in production. Not allowed to crash.
        // If we cannot find this infos during recovery we can either recompute
        // or start fresh.
        continue;
      }
      auto rocksCollection =
          static_cast<RocksDBCollection*>(collection->getPhysical());
      TRI_ASSERT(rocksCollection != nullptr);
      Result res = rocksCollection->serializeIndexEstimates(rtrx.get());
      if (!res.ok()) {
        LOG_TOPIC(WARN, Logger::ENGINES) << "writing index estimates failed: " << res.errorMessage();
        return res;
      }

      res = rocksCollection->serializeKeyGenerator(rtrx.get());
      if (!res.ok()) {
        LOG_TOPIC(WARN, Logger::ENGINES) << "writing key generators failed: " << res.errorMessage();
        return res;
      }
    }
  }

  // we have to commit all counters in one batch
  s = rtrx->Commit();
  if (s.ok()) {
    {
      WRITE_LOCKER(guard, _rwLock);
      _lastSync = seqNumber;
    }
    for (std::pair<uint64_t, CMValue> const& pair : copy) {
      _syncedSeqNums[pair.first] = pair.second._sequenceNum;
    }
  }

  return rocksutils::convertStatus(s);
}

void RocksDBCounterManager::readSettings() {
  RocksDBKey key;
  key.constructSettingsValue(RocksDBSettingsType::ServerTick);

  rocksdb::PinnableSlice result;
  rocksdb::Status status =
      _db->Get(rocksdb::ReadOptions(), RocksDBColumnFamily::definitions(),
               key.string(), &result);
  if (status.ok()) {
    // key may not be there, so don't fail when not found
    VPackSlice slice = VPackSlice(result.data());
    TRI_ASSERT(slice.isObject());
    LOG_TOPIC(TRACE, Logger::ENGINES) << "read initial settings: "
                                      << slice.toJson();

    if (!result.empty()) {
      try {
        uint64_t lastTick =
            basics::VelocyPackHelper::stringUInt64(slice.get("tick"));
        LOG_TOPIC(TRACE, Logger::ENGINES) << "using last tick: " << lastTick;
        TRI_UpdateTickServer(lastTick);

        if (slice.hasKey("hlc")) {
          uint64_t lastHlc =
              basics::VelocyPackHelper::stringUInt64(slice.get("hlc"));
          LOG_TOPIC(TRACE, Logger::ENGINES) << "using last hlc: " << lastHlc;
          TRI_HybridLogicalClock(lastHlc);
        }

        _lastSync = basics::VelocyPackHelper::stringUInt64(slice.get("lastSync"));
        LOG_TOPIC(TRACE, Logger::ENGINES) << "last background settings sync: "
                                          << _lastSync;
      } catch (...) {
        LOG_TOPIC(WARN, Logger::ENGINES)
            << "unable to read initial settings: invalid data";
      }
    }
  }
}

void RocksDBCounterManager::readIndexEstimates() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::IndexEstimateValues();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, cf));
  iter->Seek(bounds.start());

  for (; iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0;
       iter->Next()) {
    uint64_t objectId = RocksDBKey::definitionsObjectId(iter->key());
    uint64_t lastSeqNumber =
        rocksutils::uint64FromPersistent(iter->value().data());

    StringRef estimateSerialisation(iter->value().data() + sizeof(uint64_t),
                                    iter->value().size() - sizeof(uint64_t));
    // If this hits we have two estimates for the same index
    TRI_ASSERT(_estimators.find(objectId) == _estimators.end());
    try {
      if (RocksDBCuckooIndexEstimator<uint64_t>::isFormatSupported(
              estimateSerialisation)) {
        _estimators.emplace(
            objectId,
            std::make_pair(
                lastSeqNumber,
                std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(
                    estimateSerialisation)));
      }
    } catch (...) {
      // Nothing to do, if the estimator fails to create we let it be recreated.
      // Just validate that no corrupted memory was produced.
      TRI_ASSERT(_estimators.find(objectId) == _estimators.end());
    }
  }
}

void RocksDBCounterManager::readKeyGenerators() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::KeyGenerators();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, cf));
  iter->Seek(bounds.start());

  for (; iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0;
       iter->Next()) {
    uint64_t objectId = RocksDBKey::definitionsObjectId(iter->key());
    auto properties = RocksDBValue::data(iter->value());
    uint64_t lastValue = properties.get("lastValue").getUInt();

    // If this hits we have two generators for the same collection
    TRI_ASSERT(_generators.find(objectId) == _generators.end());
    try {
      _generators.emplace(objectId, lastValue);
    } catch (...) {
      // Nothing to do, just validate that no corrupted memory was produced.
      TRI_ASSERT(_generators.find(objectId) == _generators.end());
    }
  }
}

std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>
RocksDBCounterManager::stealIndexEstimator(uint64_t objectId) {
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> res(nullptr);
  auto it = _estimators.find(objectId);
  if (it != _estimators.end()) {
    // We swap out the stored estimate in order to move it to the caller
    res.swap(it->second.second);
    // Drop the now empty estimator
    _estimators.erase(objectId);
  }
  return res;
}

uint64_t RocksDBCounterManager::stealKeyGenerator(uint64_t objectId) {
  uint64_t res = 0;
  auto it = _generators.find(objectId);
  if (it != _generators.end()) {
    // We swap out the stored estimate in order to move it to the caller
    res = it->second;
    // Drop the now empty estimator
    _generators.erase(objectId);
  }
  return res;
}

void RocksDBCounterManager::clearIndexEstimators() {
  // We call this to remove all index estimators that have been stored but are
  // no longer read
  // by recovery.

  // TODO REMOVE RocksDB Keys of all not stolen values?
  _estimators.clear();
}

void RocksDBCounterManager::clearKeyGenerators() { _generators.clear(); }

/// Parse counter values from rocksdb
void RocksDBCounterManager::readCounterValues() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CounterValues();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(
      _db->NewIterator(readOptions, cf));
  iter->Seek(bounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0) {
    uint64_t objectId = RocksDBKey::definitionsObjectId(iter->key());
    auto const& it =
        _counters.emplace(objectId, CMValue(VPackSlice(iter->value().data())));
    _syncedSeqNums[objectId] = it.first->second._sequenceNum;

    iter->Next();
  }
}

/// WAL parser, no locking required here, because we have been locked from the
/// outside
class WBReader final : public rocksdb::WriteBatch::Handler {
 public:
  // must be set by the counter manager
  std::unordered_map<uint64_t, rocksdb::SequenceNumber> seqStart;
  std::unordered_map<uint64_t, RocksDBCounterManager::CounterAdjustment> deltas;
  std::unordered_map<
      uint64_t,
      std::pair<uint64_t,
                std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>>>*
      _estimators;
  std::unordered_map<uint64_t, uint64_t>* _generators;
  rocksdb::SequenceNumber currentSeqNum;
  uint64_t _maxTick = 0;
  uint64_t _maxHLC = 0;

  explicit WBReader(
      std::unordered_map<
          uint64_t,
          std::pair<uint64_t,
                    std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>>>*
          estimators,
      std::unordered_map<uint64_t, uint64_t>* generators)
      : _estimators(estimators), _generators(generators), currentSeqNum(0) {}

  ~WBReader() {
    // update ticks after parsing wal
    LOG_TOPIC(TRACE, Logger::ENGINES) << "max tick found in WAL: " << _maxTick
                                      << ", last HLC value: " << _maxHLC;

    TRI_UpdateTickServer(_maxTick);
    TRI_HybridLogicalClock(_maxHLC);
  }

  bool shouldHandleDocument(uint32_t column_family_id,
                            const rocksdb::Slice& key) {
    if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      uint64_t objectId = RocksDBKey::objectId(key);
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

  void storeMaxHLC(uint64_t hlc) {
    if (hlc > _maxHLC) {
      _maxHLC = hlc;
    }
  }

  void storeMaxTick(uint64_t tick) {
    if (tick > _maxTick) {
      _maxTick = tick;
    }
  }

  void storeLastKeyValue(uint64_t objectId, uint64_t keyValue) {
    if (keyValue == 0) {
      return;
    }

    auto it = _generators->find(objectId);

    if (it == _generators->end()) {
      try {
        _generators->emplace(objectId, keyValue);
      } catch (...) {
      }
      return;
    }

    if (keyValue > (*it).second) {
      (*it).second = keyValue;
    }
  }

  void updateMaxTick(uint32_t column_family_id, const rocksdb::Slice& key,
                     const rocksdb::Slice& value) {
    // RETURN (side-effect): update _maxTick
    //
    // extract max tick from Markers and store them as side-effect in
    // _maxTick member variable that can be used later (dtor) to call
    // TRI_UpdateTickServer (ticks.h)
    // Markers: - collections (id,objectid) as tick and max tick in indexes
    // array
    //          - documents - _rev (revision as maxtick)
    //          - databases

    if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      storeMaxHLC(RocksDBKey::revisionId(RocksDBEntryType::Document, key));
      storeLastKeyValue(RocksDBKey::objectId(key),
                        RocksDBValue::keyValue(value));
    } else if (column_family_id == RocksDBColumnFamily::primary()->GetID()) {
      // document key
      StringRef ref = RocksDBKey::primaryKey(key);
      TRI_ASSERT(!ref.empty());
      // check if the key is numeric
      if (ref[0] >= '1' && ref[0] <= '9') {
        // numeric start byte. looks good
        try {
          // extract uint64_t value from key. this will throw if the key
          // is non-numeric
          uint64_t tick =
              basics::StringUtils::uint64_check(ref.data(), ref.size());
          // if no previous _maxTick set or the numeric value found is
          // "near" our previous _maxTick, then we update it
          if (tick > _maxTick && (_maxTick == 0 || tick - _maxTick < 2048)) {
            storeMaxTick(tick);
          }
        } catch (...) {
          // non-numeric key. simply ignore it
        }
      }
    } else if (column_family_id ==
               RocksDBColumnFamily::definitions()->GetID()) {
      auto const type = RocksDBKey::type(key);

      if (type == RocksDBEntryType::Collection) {
        storeMaxTick(RocksDBKey::collectionId(key));
        auto slice = RocksDBValue::data(value);
        storeMaxTick(basics::VelocyPackHelper::stringUInt64(slice, "objectId"));
        VPackSlice indexes = slice.get("indexes");
        for (VPackSlice const& idx : VPackArrayIterator(indexes)) {
          storeMaxTick(
              std::max(basics::VelocyPackHelper::stringUInt64(idx, "objectId"),
                       basics::VelocyPackHelper::stringUInt64(idx, "id")));
        }
      } else if (type == RocksDBEntryType::Database) {
        storeMaxTick(RocksDBKey::databaseId(key));
      } else if (type == RocksDBEntryType::View) {
        storeMaxTick(std::max(RocksDBKey::databaseId(key),
                              RocksDBKey::viewId(key)));
      }
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                        const rocksdb::Slice& value) override {
    updateMaxTick(column_family_id, key, value);
    if (shouldHandleDocument(column_family_id, key)) {
      uint64_t objectId = RocksDBKey::objectId(key);
      uint64_t revisionId =
          RocksDBKey::revisionId(RocksDBEntryType::Document, key);

      auto const& it = deltas.find(objectId);
      if (it != deltas.end()) {
        it->second._sequenceNum = currentSeqNum;
        it->second._added++;
        it->second._revisionId = revisionId;
      }
    } else {
      // We have to adjust the estimate with an insert
      if (column_family_id == RocksDBColumnFamily::vpack()->GetID()) {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto it = _estimators->find(objectId);
        if (it != _estimators->end() && it->second.first < currentSeqNum) {
          // We track estimates for this index
          uint64_t hash = RocksDBVPackIndex::HashForKey(key);
          it->second.second->insert(hash);
        }
      } else if (column_family_id == RocksDBColumnFamily::edge()->GetID()) {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto it = _estimators->find(objectId);
        if (it != _estimators->end() && it->second.first < currentSeqNum) {
          // We track estimates for this index
          uint64_t hash = RocksDBEdgeIndex::HashForKey(key);
          it->second.second->insert(hash);
        }
      }
    }
    return rocksdb::Status();
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id,
                           const rocksdb::Slice& key) override {
    if (shouldHandleDocument(column_family_id, key)) {
      uint64_t objectId = RocksDBKey::objectId(key);
      uint64_t revisionId =
          RocksDBKey::revisionId(RocksDBEntryType::Document, key);

      auto const& it = deltas.find(objectId);
      if (it != deltas.end()) {
        it->second._sequenceNum = currentSeqNum;
        it->second._removed++;
        it->second._revisionId = revisionId;
      }
    } else {
      // We have to adjust the estimate with an remove
      if (column_family_id == RocksDBColumnFamily::vpack()->GetID()) {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto it = _estimators->find(objectId);
        if (it != _estimators->end() && it->second.first < currentSeqNum) {
          // We track estimates for this index
          uint64_t hash = RocksDBVPackIndex::HashForKey(key);
          it->second.second->remove(hash);
        }
      } else if (column_family_id == RocksDBColumnFamily::edge()->GetID()) {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto it = _estimators->find(objectId);
        if (it != _estimators->end() && it->second.first < currentSeqNum) {
          // We track estimates for this index
          uint64_t hash = RocksDBEdgeIndex::HashForKey(key);
          it->second.second->remove(hash);
        }
      }
    }
    return rocksdb::Status();
  }
};

/// earliest safe sequence number to throw away from wal
rocksdb::SequenceNumber RocksDBCounterManager::earliestSeqNeeded() const {
  READ_LOCKER(guard, _rwLock);
  return _lastSync;
}

/// parse the WAL with the above handler parser class
bool RocksDBCounterManager::parseRocksWAL() {
  WRITE_LOCKER(guard, _rwLock);
  TRI_ASSERT(_counters.size() > 0);

  rocksdb::SequenceNumber start = _lastSync;
  // Tell the WriteBatch reader the transaction markers to look for
  auto handler = std::make_unique<WBReader>(&_estimators, &_generators);

  for (auto const& pair : _counters) {
    handler->seqStart.emplace(pair.first, pair.second._sequenceNum);
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
