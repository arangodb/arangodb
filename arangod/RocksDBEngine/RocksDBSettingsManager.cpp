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
#include "RocksDBEngine/RocksDBEngine.h"
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
arangodb::Result writeSettings(rocksdb::WriteBatch& batch, VPackBuilder& b,
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

  rocksdb::Status s = batch.Put(RocksDBColumnFamily::definitions(), key.string(), value);
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
      _db(db->GetRootDB()),
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
  if (!_db) {
    return Result();
  }

  if (!lockForSync(force)) {
    return Result();
  }
  
  // only one thread can enter here at a time

  // make sure we give up our lock when we exit this function
  auto guard = scopeGuard([this]() { _syncing.store(false, std::memory_order_release); });
  
  // fetch the seq number prior to any writes; this guarantees that we save
  // any subsequent updates in the WAL to replay if we crash in the middle
  auto maxSeqNr = _db->GetLatestSequenceNumber();
  auto minSeqNr = maxSeqNr;

  rocksdb::TransactionOptions opts;
  opts.lock_timeout = 50; // do not wait for locking keys
  
  rocksdb::WriteOptions wo;
  rocksdb::WriteBatch batch;
  _tmpBuilder.clear(); // recycle our builder
  
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  auto dbfeature = arangodb::DatabaseFeature::DATABASE;
  
  bool didWork = false;
  auto mappings = engine->collectionMappings();
  for (auto const& pair : mappings) {
    
    TRI_voc_tick_t dbid = pair.first;
    TRI_voc_cid_t cid = pair.second;
    TRI_vocbase_t* vocbase = dbfeature->useDatabase(dbid);
    if (!vocbase) {
      continue;
    }
    TRI_ASSERT(!vocbase->isDangling());
    TRI_DEFER(vocbase->release());

    // intentionally do not `useCollection`, tends to break CI tests
    TRI_vocbase_col_status_e status;
    std::shared_ptr<LogicalCollection> coll = vocbase->useCollection(cid, status);
    if (!coll) {
      continue;
    }
    TRI_DEFER(vocbase->releaseCollection(coll.get()));
    
    auto* rcoll = static_cast<RocksDBCollection*>(coll->getPhysical());
    rocksdb::SequenceNumber appliedSeq = minSeqNr;
    Result res = rcoll->meta().serializeMeta(batch, *coll, force, _tmpBuilder, appliedSeq);
    minSeqNr = std::min(minSeqNr, appliedSeq);
    
    const std::string err = "could not sync metadata for collection '";
    if (res.fail()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << err << coll->name() << "'";
      return res;
    }
    
    if (batch.Count() > 0) {
      auto s = _db->Write(wo, &batch);
      if (!s.ok()) {
        LOG_TOPIC(WARN, Logger::ENGINES) << err << coll->name() << "'";
        return rocksutils::convertStatus(s);
      }
      didWork = true;
    }
    batch.Clear();
  }
  
  if (!didWork) {
    WRITE_LOCKER(guard, _rwLock);
    _lastSync = minSeqNr;
    return Result(); // nothing was written
  }
  
  _tmpBuilder.clear();
  Result res = writeSettings(batch, _tmpBuilder, minSeqNr);
  if (res.fail()) {
    LOG_TOPIC(WARN, Logger::ENGINES) << "could not store metadata settings "
      << res.errorMessage();
    return res;
  }

  // we have to commit all counters in one batch
  auto s = _db->Write(wo, &batch);
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

/// earliest safe sequence number to throw away from wal
rocksdb::SequenceNumber RocksDBSettingsManager::earliestSeqNeeded() const {
  READ_LOCKER(guard, _rwLock);
  return _lastSync;
}

}  // namespace arangodb
