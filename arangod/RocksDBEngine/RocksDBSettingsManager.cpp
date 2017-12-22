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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBSettingsManager.h"

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
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
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

RocksDBSettingsManager::CMValue::CMValue(VPackSlice const& slice)
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

void RocksDBSettingsManager::CMValue::serialize(VPackBuilder& b) const {
  b.openArray();
  b.add(VPackValue(this->_sequenceNum));
  b.add(VPackValue(this->_count));
  b.add(VPackValue(this->_revisionId));
  b.close();
}

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBSettingsManager::RocksDBSettingsManager(rocksdb::TransactionDB* db)
    : _lastSync(0), _syncing(false), _db(db), _initialReleasedTick(0) {}

/// retrieve initial values from the database
void RocksDBSettingsManager::retrieveInitialValues() {
  readSettings();
  readIndexEstimates();
  readCounterValues();
  readKeyGenerators();

  EngineSelectorFeature::ENGINE->releaseTick(_initialReleasedTick);
}

RocksDBSettingsManager::CounterAdjustment RocksDBSettingsManager::loadCounter(
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
void RocksDBSettingsManager::updateCounter(uint64_t objectId,
                                           CounterAdjustment const& update) {
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

arangodb::Result RocksDBSettingsManager::setAbsoluteCounter(uint64_t objectId,
                                                            uint64_t value) {
  arangodb::Result res;
  WRITE_LOCKER(guard, _rwLock);
  auto it = _counters.find(objectId);
  if (it != _counters.end()) {
    rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
    it->second._sequenceNum = db->GetLatestSequenceNumber();
    it->second._count = value;
  } else {
    // nothing to do as the counter has never been written it can not be set to
    // a value that would require correction. but we use the return value to
    // signal that no sync is rquired
    res.reset(TRI_ERROR_INTERNAL, "counter value not found - no sync required");
  }
  return res;
}

void RocksDBSettingsManager::removeCounter(uint64_t objectId) {
  WRITE_LOCKER(guard, _rwLock);
  auto const& it = _counters.find(objectId);
  if (it != _counters.end()) {
    RocksDBKey key;
    key.constructCounterValue(it->first);
    rocksdb::WriteOptions options;
    rocksdb::Status s =
        _db->Delete(options, RocksDBColumnFamily::definitions(), key.string());
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "deleting counter failed";
    }
    _counters.erase(it);
  }
}

std::unordered_map<uint64_t, rocksdb::SequenceNumber>
RocksDBSettingsManager::counterSeqs() {
  std::unordered_map<uint64_t, rocksdb::SequenceNumber> seqs;
  {  // block all updates
    WRITE_LOCKER(guard, _rwLock);
    for (auto it : _counters) {
      seqs.emplace(it.first, it.second._sequenceNum);
    }
  }
  return seqs;
}

bool RocksDBSettingsManager::lockForSync(bool force) {
  if (force) {
    while (true) {
      bool expected = false;
      bool res = _syncing.compare_exchange_strong(
          expected, true, std::memory_order_acquire, std::memory_order_relaxed);
      if (res) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } else {
    bool expected = false;

    if (!_syncing.compare_exchange_strong(expected, true,
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
      return false;
    }
  }

  return true;
}

Result RocksDBSettingsManager::writeCounterValue(
    rocksdb::Transaction* rtrx, VPackBuilder& b,
    std::pair<uint64_t, CMValue> const& pair) {
  // Skip values which we did not change
  auto const& it = _syncedSeqNums.find(pair.first);
  if (it != _syncedSeqNums.end() && it->second == pair.second._sequenceNum) {
    return Result();
  }

  b.clear();
  pair.second.serialize(b);

  RocksDBKey key;
  key.constructCounterValue(pair.first);
  rocksdb::Slice value((char*)b.start(), b.size());
  rocksdb::Status s =
      rtrx->Put(RocksDBColumnFamily::definitions(), key.string(), value);
  if (!s.ok()) {
    rtrx->Rollback();
    auto rStatus = rocksutils::convertStatus(s);
    LOG_TOPIC(WARN, Logger::ENGINES)
        << "writing counters failed: " << rStatus.errorMessage();
    return rStatus;
  }

  return Result();
}

Result RocksDBSettingsManager::writeSettings(rocksdb::Transaction* rtrx,
                                             VPackBuilder& b,
                                             uint64_t seqNumber) {
  // now write global settings
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  b.clear();
  b.openObject();
  b.add("tick", VPackValue(std::to_string(TRI_CurrentTickServer())));
  b.add("hlc", VPackValue(std::to_string(TRI_HybridLogicalClock())));
  b.add("releasedTick", VPackValue(std::to_string(engine->releasedTick())));
  b.add("lastSync", VPackValue(std::to_string(seqNumber)));
  b.close();

  VPackSlice slice = b.slice();
  LOG_TOPIC(TRACE, Logger::ENGINES) << "writing settings: " << slice.toJson();

  RocksDBKey key;
  key.constructSettingsValue(RocksDBSettingsType::ServerTick);
  rocksdb::Slice value(slice.startAs<char>(), slice.byteSize());

  rocksdb::Status s =
      rtrx->Put(RocksDBColumnFamily::definitions(), key.string(), value);

  if (!s.ok()) {
    auto rStatus = rocksutils::convertStatus(s);
    LOG_TOPIC(WARN, Logger::ENGINES)
        << "writing settings failed: " << rStatus.errorMessage();
    rtrx->Rollback();
    return rStatus;
  }

  return Result();
}

Result RocksDBSettingsManager::writeIndexEstimatorAndKeyGenerator(
    rocksdb::Transaction* rtrx, VPackBuilder& b,
    std::pair<uint64_t, CMValue> const& pair) {
  auto dbColPair = rocksutils::mapObjectToCollection(pair.first);
  if (dbColPair.second == 0 && dbColPair.first == 0) {
    // collection with this objectID not known.Skip.
    return Result();
  }
  auto dbfeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
  TRI_ASSERT(dbfeature != nullptr);
  auto vocbase = dbfeature->useDatabase(dbColPair.first);
  if (vocbase == nullptr) {
    // Bad state, we have references to a database that is not known
    // anymore.
    // However let's just skip in production. Not allowed to crash.
    // If we cannot find this infos during recovery we can either recompute
    // or start fresh.
    return Result();
  }
  TRI_DEFER(vocbase->release());

  auto collection = vocbase->lookupCollection(dbColPair.second);
  if (collection == nullptr) {
    // Bad state, we have references to a collection that is not known
    // anymore.
    // However let's just skip in production. Not allowed to crash.
    // If we cannot find this infos during recovery we can either recompute
    // or start fresh.
    return Result();
  }
  auto rocksCollection =
      static_cast<RocksDBCollection*>(collection->getPhysical());
  TRI_ASSERT(rocksCollection != nullptr);
  Result res = rocksCollection->serializeIndexEstimates(rtrx);
  if (!res.ok()) {
    LOG_TOPIC(WARN, Logger::ENGINES)
        << "writing index estimates failed: " << res.errorMessage();
    return res;
  }

  res = rocksCollection->serializeKeyGenerator(rtrx);
  if (!res.ok()) {
    LOG_TOPIC(WARN, Logger::ENGINES)
        << "writing key generators failed: " << res.errorMessage();
    return res;
  }

  return Result();
}

/// Thread-Safe force sync
Result RocksDBSettingsManager::sync(bool force) {
  TRI_IF_FAILURE("RocksDBSettingsManagerSync") { return Result(); }

  if (!lockForSync(force)) {
    return Result();
  }
  TRI_DEFER(_syncing = false);

  std::unordered_map<uint64_t, CMValue> copy;
  {  // block all updates
    WRITE_LOCKER(guard, _rwLock);
    copy = _counters;
  }

  rocksdb::WriteOptions writeOptions;
  auto seqNumber = _db->GetLatestSequenceNumber();
  std::unique_ptr<rocksdb::Transaction> rtrx(
      _db->BeginTransaction(writeOptions));

  VPackBuilder b;
  for (std::pair<uint64_t, CMValue> const& pair : copy) {
    Result res = writeCounterValue(rtrx.get(), b, pair);
    if (res.fail()) {
      return res;
    }

    res = writeIndexEstimatorAndKeyGenerator(rtrx.get(), b, pair);
    if (res.fail()) {
      return res;
    }
  }

  Result res = writeSettings(rtrx.get(), b, seqNumber);
  if (res.fail()) {
    return res;
  }

  // we have to commit all counters in one batch
  auto s = rtrx->Commit();
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

void RocksDBSettingsManager::readSettings() {
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
    LOG_TOPIC(TRACE, Logger::ENGINES)
        << "read initial settings: " << slice.toJson();

    if (!result.empty()) {
      try {
        if (slice.hasKey("tick")) {
          uint64_t lastTick =
              basics::VelocyPackHelper::stringUInt64(slice.get("tick"));
          LOG_TOPIC(TRACE, Logger::ENGINES) << "using last tick: " << lastTick;
          TRI_UpdateTickServer(lastTick);
        }

        if (slice.hasKey("hlc")) {
          uint64_t lastHlc =
              basics::VelocyPackHelper::stringUInt64(slice.get("hlc"));
          LOG_TOPIC(TRACE, Logger::ENGINES) << "using last hlc: " << lastHlc;
          TRI_HybridLogicalClock(lastHlc);
        }

        if (slice.hasKey("releasedTick")) {
          _initialReleasedTick =
              basics::VelocyPackHelper::stringUInt64(slice.get("releasedTick"));
          LOG_TOPIC(TRACE, Logger::ENGINES)
              << "using released tick: " << _initialReleasedTick;
          EngineSelectorFeature::ENGINE->releaseTick(_initialReleasedTick);
        }

        if (slice.hasKey("lastSync")) {
          _lastSync =
              basics::VelocyPackHelper::stringUInt64(slice.get("lastSync"));
          LOG_TOPIC(TRACE, Logger::ENGINES)
              << "last background settings sync: " << _lastSync;
        }
      } catch (...) {
        LOG_TOPIC(WARN, Logger::ENGINES)
            << "unable to read initial settings: invalid data";
      }
    }
  }
}

void RocksDBSettingsManager::readIndexEstimates() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::IndexEstimateValues();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions, cf));
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

void RocksDBSettingsManager::readKeyGenerators() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::KeyGenerators();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions, cf));
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

std::pair<std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>, uint64_t>
RocksDBSettingsManager::stealIndexEstimator(uint64_t objectId) {
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> res(nullptr);
  uint64_t seq = 0;
  auto it = _estimators.find(objectId);
  if (it != _estimators.end()) {
    // We swap out the stored estimate in order to move it to the caller
    res.swap(it->second.second);
    seq = it->second.first;
    // Drop the now empty estimator
    _estimators.erase(objectId);
  }
  return std::make_pair(std::move(res), seq);
}

uint64_t RocksDBSettingsManager::stealKeyGenerator(uint64_t objectId) {
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

void RocksDBSettingsManager::clearIndexEstimators() {
  // We call this to remove all index estimators that have been stored but are
  // no longer read
  // by recovery.

  // TODO REMOVE RocksDB Keys of all not stolen values?
  _estimators.clear();
}

void RocksDBSettingsManager::clearKeyGenerators() { _generators.clear(); }

/// Parse counter values from rocksdb
void RocksDBSettingsManager::readCounterValues() {
  WRITE_LOCKER(guard, _rwLock);
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CounterValues();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions, cf));
  iter->Seek(bounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0) {
    uint64_t objectId = RocksDBKey::definitionsObjectId(iter->key());
    auto const& it =
        _counters.emplace(objectId, CMValue(VPackSlice(iter->value().data())));
    _syncedSeqNums[objectId] = it.first->second._sequenceNum;

    iter->Next();
  }
}

/// earliest safe sequence number to throw away from wal
rocksdb::SequenceNumber RocksDBSettingsManager::earliestSeqNeeded() const {
  READ_LOCKER(guard, _rwLock);
  return _lastSync;
}
