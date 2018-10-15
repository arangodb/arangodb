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

namespace {
std::pair<arangodb::Result, rocksdb::SequenceNumber>
  writeCounterValue(std::unordered_map<uint64_t, rocksdb::SequenceNumber> const& syncedSeqNums,
                    rocksdb::Transaction* rtrx, VPackBuilder& b,
                    std::pair<uint64_t, arangodb::RocksDBSettingsManager::CMValue> const& pair,
                    rocksdb::SequenceNumber baseSeq) {
  using arangodb::Logger;
  using arangodb::Result;
  using arangodb::RocksDBColumnFamily;
  using arangodb::RocksDBKey;
  using arangodb::rocksutils::convertStatus;

  rocksdb::SequenceNumber returnSeq = baseSeq;

  // Skip values which we did not change
  auto const& it = syncedSeqNums.find(pair.first);
  if (it != syncedSeqNums.end() && it->second == pair.second._sequenceNum) {
    // implication: no-one update the collection since the last sync,
    // we do not need to keep the log entries for this counter
    return std::make_pair(Result(), returnSeq);
  }
  
  b.clear();
  pair.second.serialize(b);

  RocksDBKey key;
  key.constructCounterValue(pair.first);
  rocksdb::Slice value((char*)b.start(), b.size());
  rocksdb::Status s =
      rtrx->Put(RocksDBColumnFamily::definitions(), key.string(), value);
  if (!s.ok()) {
    return std::make_pair(convertStatus(s), returnSeq);
  }

  returnSeq = std::min(returnSeq, pair.second._sequenceNum);
  return std::make_pair(Result(), returnSeq);
}
}  // namespace

namespace {
arangodb::Result writeSettings(rocksdb::Transaction* rtrx, VPackBuilder& b,
                               uint64_t seqNumber) {
  using arangodb::EngineSelectorFeature;
  using arangodb::Logger;
  using arangodb::Result;
  using arangodb::RocksDBColumnFamily;
  using arangodb::RocksDBKey;
  using arangodb::RocksDBSettingsType;
  using arangodb::StorageEngine;

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
    return arangodb::rocksutils::convertStatus(s);
  }

  return Result();
}
}  // namespace

namespace {
std::pair<arangodb::Result, rocksdb::SequenceNumber>
writeIndexEstimatorsAndKeyGenerator(
    rocksdb::Transaction* rtrx,
    std::pair<uint64_t, arangodb::RocksDBSettingsManager::CMValue> const& pair,
    rocksdb::SequenceNumber baseSeq) {
  using arangodb::DatabaseFeature;
  using arangodb::Logger;
  using arangodb::Result;
  using arangodb::RocksDBCollection;
  using arangodb::application_features::ApplicationServer;
  using arangodb::rocksutils::mapObjectToCollection;

  auto returnSeq = baseSeq;
  auto dbColPair = mapObjectToCollection(pair.first);
  if (dbColPair.second == 0 && dbColPair.first == 0) {
    // collection with this objectID not known.Skip.
    return std::make_pair(Result(), returnSeq);
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
    return std::make_pair(Result(), returnSeq);
  }

  TRI_DEFER(vocbase->release());

  auto collection = vocbase->lookupCollection(dbColPair.second);
  if (collection == nullptr) {
    // Bad state, we have references to a collection that is not known
    // anymore.
    // However let's just skip in production. Not allowed to crash.
    // If we cannot find this infos during recovery we can either recompute
    // or start fresh.
    return std::make_pair(Result(), returnSeq);
  }
  auto rocksCollection =
      static_cast<RocksDBCollection*>(collection->getPhysical());
  TRI_ASSERT(rocksCollection != nullptr);
  auto serializeResult =
      rocksCollection->serializeIndexEstimates(rtrx, baseSeq);
  if (!serializeResult.first.ok()) {
    LOG_TOPIC(WARN, Logger::ENGINES) << "writing index estimates failed: "
                                     << serializeResult.first.errorMessage();
    return std::make_pair(serializeResult.first, returnSeq);
  }
  returnSeq = std::min(returnSeq, serializeResult.second);

  Result res = rocksCollection->serializeKeyGenerator(rtrx);
  if (!res.ok()) {
    LOG_TOPIC(WARN, Logger::ENGINES)
        << "writing key generators failed: " << res.errorMessage();
    return std::make_pair(res, returnSeq);
  }

  return std::make_pair(Result(), returnSeq);
}
}  // namespace

namespace arangodb {

RocksDBSettingsManager::CMValue::CMValue(VPackSlice const& slice)
    : _sequenceNum(0), _added(0), _removed(0), _revisionId(0) {
  if (!slice.isArray()) {
    // got a somewhat invalid slice. probably old data from before the key
    // structure changes
    return;
  }

  velocypack::ArrayIterator array(slice);
  if (array.valid()) {
    this->_sequenceNum = (*array).getUInt();
    // versions pre 3.4 stored only a single "count" value
    // 3.4 and higher store "added" and "removed" seperately
    this->_added = (*(++array)).getUInt();
    if (array.size() > 3) {
      TRI_ASSERT(array.size() == 4);
      this->_removed = (*(++array)).getUInt();
    }
    this->_revisionId = (*(++array)).getUInt();
  }
}

void RocksDBSettingsManager::CMValue::serialize(VPackBuilder& b) const {
  b.openArray();
  b.add(VPackValue(_sequenceNum));
  b.add(VPackValue(_added));
  b.add(VPackValue(_removed));
  b.add(VPackValue(_revisionId));
  b.close();
}

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBSettingsManager::RocksDBSettingsManager(rocksdb::TransactionDB* db)
    : _lastSync(0), 
      _syncing(false), 
      _db(db), 
      _initialReleasedTick(0),
      _maxUpdateSeqNo(1),
      _lastSyncedSeqNo(0) {}

/// bump up the value of the last rocksdb::SequenceNumber we have seen
/// and that is pending a sync update
void RocksDBSettingsManager::setMaxUpdateSequenceNumber(rocksdb::SequenceNumber seqNo) {
  if (seqNo == 0) {
    // we don't care about this
    return;
  }

  auto current = _maxUpdateSeqNo.load(std::memory_order_acquire);

  if (current >= seqNo) {
    // current sequence number is already higher than the one we got
    return;
  }

  bool res = _maxUpdateSeqNo.compare_exchange_strong(current, seqNo, std::memory_order_release);

  if (!res) {
    // someone else has updated the max sequence number
    TRI_ASSERT(current >= seqNo);
  }
}

/// retrieve initial values from the database
void RocksDBSettingsManager::retrieveInitialValues() {
  loadSettings();
  loadIndexEstimates();
  loadCounterValues();
  loadKeyGenerators();

  EngineSelectorFeature::ENGINE->releaseTick(_initialReleasedTick);
}

RocksDBSettingsManager::CounterAdjustment RocksDBSettingsManager::loadCounter(
    uint64_t objectId) const {
  TRI_ASSERT(objectId != 0);  // TODO fix this

  READ_LOCKER(guard, _rwLock);

  auto const& it = _counters.find(objectId);
  if (it != _counters.end()) {
    return CounterAdjustment(it->second._sequenceNum, 
                             it->second._added, 
                             it->second._removed,
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
  auto seqNo = update.sequenceNumber();
  {
    WRITE_LOCKER(guard, _rwLock);

    auto it = _counters.find(objectId);
    if (it != _counters.end()) {
      it->second._added += update.added();
      it->second._removed += update.removed();
      // just use the latest trx info
      if (seqNo > it->second._sequenceNum) {
        it->second._sequenceNum = seqNo;
        if (update.revisionId() != 0) {
          it->second._revisionId = update.revisionId();
        }
      }
    } else {
      // insert new counter
      _counters.emplace(std::make_pair(
          objectId,
          CMValue(update.sequenceNumber(), update.added(), update.removed(),
                  update.revisionId())));
      needsSync = true;  // only count values from WAL if they are in the DB
    }
  }

  setMaxUpdateSequenceNumber(seqNo);

  if (needsSync) {
    sync(true);
  }
}

arangodb::Result RocksDBSettingsManager::setAbsoluteCounter(uint64_t objectId,
                                                            rocksdb::SequenceNumber seq,
                                                            uint64_t value) {
  arangodb::Result res;
  rocksdb::SequenceNumber seqNo = 0;

  {
    WRITE_LOCKER(guard, _rwLock);
    
    auto it = _counters.find(objectId);

    if (it != _counters.end()) {
      LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "resetting counter value to " << value;
      it->second._sequenceNum = std::max(seq, it->second._sequenceNum);
      it->second._added = value;
      it->second._removed = 0;
    } else {
      // nothing to do as the counter has never been written it can not be set to
      // a value that would require correction. but we use the return value to
      // signal that no sync is rquired
      res.reset(TRI_ERROR_INTERNAL, "counter value not found - no sync required");
    }
  }
  
  setMaxUpdateSequenceNumber(seqNo);

  return res;
}

void RocksDBSettingsManager::removeCounter(uint64_t objectId) {
  WRITE_LOCKER(guard, _rwLock);

  auto it = _counters.find(objectId);

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
  {  // block all updates while we copy
    READ_LOCKER(guard, _rwLock);
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

/// Thread-Safe force sync
Result RocksDBSettingsManager::sync(bool force) {
  TRI_IF_FAILURE("RocksDBSettingsManagerSync") { return Result(); }

  if (!lockForSync(force)) {
    return Result();
  }
  
  // only one thread can enter here at a time

  // make sure we give up our lock when we exit this function
  auto guard = scopeGuard([this]() { _syncing = false; });
  
  auto maxUpdateSeqNo =  _maxUpdateSeqNo.load(std::memory_order_acquire);

  if (!force && maxUpdateSeqNo <= _lastSyncedSeqNo) {
    // if noone has updated any counters etc. since we were here last,
    // there is no need to do anything!
    return Result();
  }

  // ok, when we are here, we will write out something back to the database

  std::unordered_map<uint64_t, CMValue> copy;
  {  // block all updates
    WRITE_LOCKER(guard, _rwLock);
    copy = _counters;
  }

  // fetch the seq number prior to any writes; this guarantees that we save
  // any subsequent updates in the WAL to replay if we crash in the middle
  auto seqNumber = _db->GetLatestSequenceNumber();
 
  rocksdb::WriteOptions writeOptions;
  std::unique_ptr<rocksdb::Transaction> rtrx(
      _db->BeginTransaction(writeOptions));

  // recycle our builder
  _builder.clear();

  for (std::pair<uint64_t, CMValue> const& pair : copy) {
    Result res;
    rocksdb::SequenceNumber returnSeq;
    
    std::tie(res, returnSeq) = writeCounterValue(_syncedSeqNums, rtrx.get(),
                                                 _builder, pair, seqNumber);
    if (res.fail()) {
      return res;
    }
    seqNumber = std::min(seqNumber, returnSeq);

    std::tie(res, returnSeq) =
        writeIndexEstimatorsAndKeyGenerator(rtrx.get(), pair, seqNumber);
    if (res.fail()) {
      return res;
    }
    seqNumber = std::min(seqNumber, returnSeq);
  }

  Result res = writeSettings(rtrx.get(), _builder, seqNumber);

  if (res.fail()) {
    return res;
  }

  // we have to commit all counters in one batch
  auto s = rtrx->Commit();
  
  if (s.ok()) {
    _lastSyncedSeqNo = maxUpdateSeqNo;

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

void RocksDBSettingsManager::loadSettings() {
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
      WRITE_LOCKER(guard, _rwLock);
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

void RocksDBSettingsManager::loadIndexEstimates() {
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

    StringRef estimateSerialization(iter->value().data() + sizeof(uint64_t),
                                    iter->value().size() - sizeof(uint64_t));
    
    WRITE_LOCKER(guard, _rwLock);
    // If this hits we have two estimates for the same index
    TRI_ASSERT(_estimators.find(objectId) == _estimators.end());
    try {
      if (RocksDBCuckooIndexEstimator<uint64_t>::isFormatSupported(
              estimateSerialization)) {
        auto it = _estimators.emplace(
            objectId, std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(
                          lastSeqNumber, estimateSerialization));
        if (it.second) {
          auto estimator = it.first->second.get();
          LOG_TOPIC(TRACE, Logger::ENGINES)
              << "found index estimator for objectId '" << objectId
              << "' last synced at " << lastSeqNumber << " with estimate "
              << estimator->computeEstimate();
        }
      }
    } catch (...) {
      // Nothing to do, if the estimator fails to create we let it be recreated.
      // Just validate that no corrupted memory was produced.
      TRI_ASSERT(_estimators.find(objectId) == _estimators.end());
    }
  }
}

void RocksDBSettingsManager::loadKeyGenerators() {
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

    VPackSlice s = properties.get(StaticStrings::LastValue);
    if (!s.isNone()) { 
      uint64_t lastValue = properties.get(StaticStrings::LastValue).getUInt();

      WRITE_LOCKER(guard, _rwLock);
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
}

std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>
RocksDBSettingsManager::stealIndexEstimator(uint64_t objectId) {
  std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>> res;

  WRITE_LOCKER(guard, _rwLock);

  auto it = _estimators.find(objectId);
  
  if (it != _estimators.end()) {
    // We swap out the stored estimate in order to move it to the caller
    res.swap(it->second);
    // Drop the now empty estimator
    _estimators.erase(objectId);
  }
  
  return res;
}

uint64_t RocksDBSettingsManager::stealKeyGenerator(uint64_t objectId) {
  uint64_t res = 0;

  {
    WRITE_LOCKER(guard, _rwLock);
    
    auto it = _generators.find(objectId);

    if (it != _generators.end()) {
      // We swap out the stored generator state in order to move it to the caller
      res = it->second;
      // we are now not responsible for the generator anymore
      _generators.erase(objectId);
    }
  }
  
  return res;
}

void RocksDBSettingsManager::clearIndexEstimators() {
  // We call this to remove all index estimators that have been stored but are
  // no longer read by recovery.

  // TODO REMOVE RocksDB Keys of all not stolen values?
  WRITE_LOCKER(guard, _rwLock);
  _estimators.clear();
}

void RocksDBSettingsManager::clearKeyGenerators() {
  WRITE_LOCKER(guard, _rwLock);
  _generators.clear();
}

/// Parse counter values from rocksdb
void RocksDBSettingsManager::loadCounterValues() {
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CounterValues();

  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::Comparator const* cmp = cf->GetComparator();
  rocksdb::ReadOptions readOptions;
  std::unique_ptr<rocksdb::Iterator> iter(_db->NewIterator(readOptions, cf));
  iter->Seek(bounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), bounds.end()) < 0) {
    uint64_t objectId = RocksDBKey::definitionsObjectId(iter->key());
    
    {
      WRITE_LOCKER(guard, _rwLock);
      auto const& it =
          _counters.emplace(objectId, CMValue(VPackSlice(iter->value().data())));
      _syncedSeqNums[objectId] = it.first->second._sequenceNum;
      LOG_TOPIC(TRACE, Logger::ENGINES)
          << "found count marker for objectId '" << objectId
          << "' last synced at " << it.first->first << " with added "
          << it.first->second._added << ", removed " << it.first->second._removed;
    }

    iter->Next();
  }
}

/// earliest safe sequence number to throw away from wal
rocksdb::SequenceNumber RocksDBSettingsManager::earliestSeqNeeded() const {
  READ_LOCKER(guard, _rwLock);
  return _lastSync;
}

}  // namespace arangodb
