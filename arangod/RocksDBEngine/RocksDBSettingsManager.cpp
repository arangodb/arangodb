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

//namespace {
//std::pair<arangodb::Result, rocksdb::SequenceNumber>
//  writeCollectionValues(rocksdb::Transaction* rtrx, VPackBuilder& b,
//                        std::unordered_map<uint64_t, rocksdb::SequenceNumber>& syncedSeqNums,
//                        uint64_t objectId, arangodb::RocksDBSettingsManager::CMValue const& counter,
//                        rocksdb::SequenceNumber maxSeqNr) {
//  using arangodb::DatabaseFeature;
//  using arangodb::Logger;
//  using arangodb::Result;
//  using arangodb::RocksDBCollection;
//  using arangodb::RocksDBColumnFamily;
//  using arangodb::RocksDBKey;
//  using arangodb::rocksutils::convertStatus;
//
//  rocksdb::SequenceNumber returnSeq = maxSeqNr;
//
//  // implication: no-one update the collection since the last sync,
//  // we do not need to keep the log entries for this counter
//
//  // Skip values which we did not change
//  auto const& it = syncedSeqNums.find(objectId);
//  if (it == syncedSeqNums.end() || it->second > counter._sequenceNum) {
//
//    b.clear();
//    counter.serialize(b);
//
//    RocksDBKey key;
//    key.constructCounterValue(objectId);
//    rocksdb::Slice value((char*)b.start(), b.size());
//    rocksdb::Status s =
//    rtrx->Put(RocksDBColumnFamily::definitions(), key.string(), value);
//    if (!s.ok()) {
//      LOG_TOPIC(WARN, Logger::ENGINES)
//      << "writing counter for collection with objectId '" << objectId << "' failed: "
//      << s.ToString();
//      return std::make_pair(convertStatus(s), returnSeq);
//    }
//
//    if (it != syncedSeqNums.end()) {
//      it->second = counter._sequenceNum;
//    } else {
//      syncedSeqNums.emplace(objectId, counter._sequenceNum);
//    }
//    returnSeq = std::min(returnSeq, counter._sequenceNum);
//  }
//
//  auto dbColPair = arangodb::rocksutils::mapObjectToCollection(objectId);
//  if (dbColPair.second == 0 && dbColPair.first == 0) {
//    // collection with this objectID not known.Skip.
//    return std::make_pair(Result(), returnSeq);
//  }
//
//  auto dbfeature = arangodb::DatabaseFeature::DATABASE;
//  TRI_ASSERT(dbfeature != nullptr);
//  auto vocbase = dbfeature->useDatabase(dbColPair.first);
//  if (vocbase == nullptr) {
//    // Bad state, we have references to a database that is not known anymore.
//    // However let's just skip in production. Not allowed to crash.
//    // If we cannot find this infos during recovery we can either recompute
//    // or start fresh.
//    return std::make_pair(Result(), returnSeq);
//  }
//  TRI_DEFER(vocbase->release());
//
//  auto coll = vocbase->lookupCollection(dbColPair.second);
//  if (!coll) {
//    // Bad state, we have references to a collection that is not known
//    // anymore. However let's just skip in production. Not allowed to crash.
//    // If we cannot find this infos during recovery we can either recompute
//    // or start fresh.
//    return std::make_pair(Result(), returnSeq);
//  }
//  auto rColl = static_cast<RocksDBCollection*>(coll->getPhysical());
//  TRI_ASSERT(rColl != nullptr);
//  auto serializeResult = rColl->serializeIndexEstimates(rtrx);
//  if (!serializeResult.first.ok()) {
//    LOG_TOPIC(WARN, Logger::ENGINES) << "writing index estimates failed: "
//    << serializeResult.first.errorMessage();
//    return std::make_pair(serializeResult.first, returnSeq);
//  }
//  returnSeq = std::min(returnSeq, serializeResult.second);
//
//  Result res = rColl->serializeKeyGenerator(rtrx);
//  if (!res.ok()) {
//    LOG_TOPIC(WARN, Logger::ENGINES)
//    << "writing key generators failed: " << res.errorMessage();
//    return std::make_pair(res, returnSeq);
//  }
//
//  return std::make_pair(Result(), returnSeq);
//}
//}  // namespace

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
    LOG_TOPIC(WARN, Logger::ENGINES) << "writing settings failed: " << s.ToString();
    return arangodb::rocksutils::convertStatus(s);
  }

  return Result();
}
}  // namespace

namespace arangodb {

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBSettingsManager::RocksDBSettingsManager(rocksdb::TransactionDB* db)
    : _lastSync(0), 
      _syncing(false), 
      _db(db), 
      _initialReleasedTick(0) {}

/// retrieve initial values from the database
void RocksDBSettingsManager::retrieveInitialValues() {
  loadSettings();
  EngineSelectorFeature::ENGINE->releaseTick(_initialReleasedTick);
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
  auto guard = scopeGuard([this]() { _syncing.store(false, std::memory_order_release); });
  
#warning fix maxUpdateSeqNo

  // fetch the seq number prior to any writes; this guarantees that we save
  // any subsequent updates in the WAL to replay if we crash in the middle
  auto maxSeqNr = _db->GetLatestSequenceNumber();
  auto minSeqNr = maxSeqNr;
  LOG_DEVEL << "starting sync with latestSeqNr: " << maxSeqNr;

  rocksdb::WriteOptions wo;
  std::unique_ptr<rocksdb::Transaction> rtrx(_db->BeginTransaction(wo));

  
  _tmpBuilder.clear(); // recycle our builder
  auto dbfeature = arangodb::DatabaseFeature::DATABASE;
  dbfeature->enumerateDatabases([&](TRI_vocbase_t& vocbase) {
    if (!vocbase.use()) {
      return;
    }
    TRI_DEFER(vocbase.release());
    // TODO do not use all databases
    vocbase.processCollections([&](LogicalCollection* coll) {
      RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(coll->getPhysical());
      rocksdb::SequenceNumber seq;
      Result res = rcoll->meta().serializeMeta(rtrx.get(), *coll, force, _tmpBuilder, seq);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }, /*includeDeleted*/false);
  });
  
  if (rtrx->GetNumKeys() == 0) {
    LOG_DEVEL << "nothing happened skipping settings up to " << minSeqNr;
    return Result(); // nothing was written
  }
  
//  for (std::pair<uint64_t, CMValue> const& pair : countersCpy) {
//    Result res;
//    rocksdb::SequenceNumber returnSeq;
//
//    const uint64_t objectId = pair.first; // collection objectId
//    CMValue const& counterVal = pair.second;
//    std::tie(res, returnSeq) = writeCollectionValues(rtrx.get(), _tmpBuilder,
//                                                     syncedSeqNumsCpy, objectId,
//                                                     counterVal, maxSeqNr);
//    if (res.fail()) {
//      return res;
//    }
//    minSeqNr = std::min(minSeqNr, returnSeq);
//  }

  _tmpBuilder.clear();
  Result res = writeSettings(rtrx.get(), _tmpBuilder, minSeqNr);
  if (res.fail()) {
    return res;
  }

  LOG_DEVEL << "commiting settings up to " << minSeqNr;
  // we have to commit all counters in one batch
  auto s = rtrx->Commit();
  
  if (s.ok()) {
    WRITE_LOCKER(guard, _rwLock);
    _lastSync = minSeqNr;
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
          LOG_TOPIC(ERR, Logger::ENGINES)
              << "last background settings sync: " << _lastSync;
        }
      } catch (...) {
        LOG_TOPIC(WARN, Logger::ENGINES)
            << "unable to read initial settings: invalid data";
      }
    }
  }
}

/// earliest safe sequence number to throw away from wal
rocksdb::SequenceNumber RocksDBSettingsManager::earliestSeqNeeded() const {
  READ_LOCKER(guard, _rwLock);
  return _lastSync;
}

}  // namespace arangodb
