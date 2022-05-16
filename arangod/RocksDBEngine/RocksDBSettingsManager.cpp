////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "RocksDBEngine/RocksDBVPackIndex.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

namespace {
void buildSettings(arangodb::StorageEngine& engine, VPackBuilder& b,
                   uint64_t seqNumber) {
  b.clear();
  b.openObject();
  b.add("tick", VPackValue(std::to_string(TRI_CurrentTickServer())));
  b.add("hlc", VPackValue(std::to_string(TRI_HybridLogicalClock())));
  b.add("releasedTick", VPackValue(std::to_string(engine.releasedTick())));
  b.add("lastSync", VPackValue(std::to_string(seqNumber)));
  b.close();
}

arangodb::Result writeSettings(VPackSlice slice, rocksdb::WriteBatch& batch) {
  using arangodb::Logger;
  using arangodb::RocksDBColumnFamilyManager;
  using arangodb::RocksDBKey;
  using arangodb::RocksDBSettingsType;

  LOG_TOPIC("f5e34", DEBUG, Logger::ENGINES)
      << "writing settings: " << slice.toJson();

  RocksDBKey key;
  key.constructSettingsValue(RocksDBSettingsType::ServerTick);
  rocksdb::Slice value(slice.startAs<char>(), slice.byteSize());

  rocksdb::Status s =
      batch.Put(RocksDBColumnFamilyManager::get(
                    RocksDBColumnFamilyManager::Family::Definitions),
                key.string(), value);
  if (!s.ok()) {
    LOG_TOPIC("140ec", WARN, Logger::ENGINES)
        << "writing settings failed: " << s.ToString();
    return arangodb::rocksutils::convertStatus(s);
  }

  return {};
}
}  // namespace

namespace arangodb {

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBSettingsManager::RocksDBSettingsManager(RocksDBEngine& engine)
    : _engine(engine),
      _lastSync(0),
      _db(engine.db()->GetRootDB()),
      _initialReleasedTick(0) {}

/// retrieve initial values from the database
void RocksDBSettingsManager::retrieveInitialValues() {
  loadSettings();
  _engine.releaseTick(_initialReleasedTick);
}

// Thread-Safe force sync.
ResultT<bool> RocksDBSettingsManager::sync(bool force) {
  TRI_IF_FAILURE("RocksDBSettingsManagerSync") {
    return ResultT<bool>::success(false);
  }

  std::unique_lock lock{_syncingMutex, std::defer_lock};

  if (force) {
    lock.lock();
  } else if (!lock.try_lock()) {
    // if we can't get the lock, we need to exit here without getting
    // any work done. callers can use the force flag to indicate work
    // *must* be performed.
    return ResultT<bool>::success(false);
  }

  TRI_ASSERT(lock.owns_lock());

  try {
    // need superuser scope to ensure we can sync all collections and keep seq
    // numbers in sync; background index creation will call this function as
    // user, and can lead to seq numbers getting out of sync
    ExecContextSuperuserScope superuser;

    // fetch the seq number prior to any writes; this guarantees that we save
    // any subsequent updates in the WAL to replay if we crash in the middle
    auto const maxSeqNr = _db->GetLatestSequenceNumber();
    auto minSeqNr = maxSeqNr;
    TRI_ASSERT(minSeqNr > 0);

    rocksdb::TransactionOptions opts;
    opts.lock_timeout = 50;  // do not wait for locking keys

    rocksdb::WriteOptions wo;
    rocksdb::WriteBatch batch;
    _tmpBuilder.clear();  // recycle our builder

    auto& dbfeature = _engine.server().getFeature<arangodb::DatabaseFeature>();
    TRI_ASSERT(!_engine.inRecovery());  // just don't

    bool didWork = false;
    auto mappings = _engine.collectionMappings();

    // reserve a bit of scratch space to work with.
    // note: the scratch buffer is recycled, so we can start
    // small here. it will grow as needed.
    constexpr size_t scratchBufferSize = 128 * 1024;
    _scratch.reserve(scratchBufferSize);

    for (auto const& pair : mappings) {
      TRI_voc_tick_t dbid = pair.first;
      DataSourceId cid = pair.second;
      TRI_vocbase_t* vocbase = dbfeature.useDatabase(dbid);
      if (!vocbase) {
        continue;
      }
      TRI_ASSERT(!vocbase->isDangling());
      auto sg = arangodb::scopeGuard([&]() noexcept { vocbase->release(); });

      std::shared_ptr<LogicalCollection> coll;
      try {
        coll = vocbase->useCollection(cid, /*checkPermissions*/ false);
      } catch (...) {
        // will fail if collection does not exist
      }
      // Collections which are marked as isAStub are not allowed to have
      // physicalCollections. Therefore, we cannot continue serializing in that
      // case.
      if (!coll || coll->isAStub()) {
        continue;
      }
      auto sg2 = arangodb::scopeGuard(
          [&]() noexcept { vocbase->releaseCollection(coll.get()); });

      LOG_TOPIC("afb17", TRACE, Logger::ENGINES)
          << "syncing metadata for collection '" << coll->name() << "'";

      // clear our scratch buffers for this round
      _scratch.clear();
      _tmpBuilder.clear();
      batch.Clear();

      auto* rcoll = static_cast<RocksDBCollection*>(coll->getPhysical());
      rocksdb::SequenceNumber appliedSeq = maxSeqNr;
      Result res = rcoll->meta().serializeMeta(batch, *coll, force, _tmpBuilder,
                                               appliedSeq, _scratch);

      if (res.ok() && batch.Count() > 0) {
        didWork = true;

        auto s = _db->Write(wo, &batch);
        if (!s.ok()) {
          res.reset(rocksutils::convertStatus(s));
        }
      }

      if (res.fail()) {
        LOG_TOPIC("afa17", WARN, Logger::ENGINES)
            << "could not sync metadata for collection '" << coll->name()
            << "'";
        return res;
      }

      minSeqNr = std::min(minSeqNr, appliedSeq);
    }

    if (_scratch.capacity() >= 32 * 1024 * 1024) {
      // much data in _scratch, let's shrink it to save memory
      TRI_ASSERT(scratchBufferSize < 32 * 1024 * 1024);
      _scratch.resize(scratchBufferSize);
      _scratch.shrink_to_fit();
    }
    _scratch.clear();
    TRI_ASSERT(_scratch.empty());

    auto const lastSync = _lastSync.load();

    LOG_TOPIC("53e4c", TRACE, Logger::ENGINES)
        << "about to store lastSync. previous value: " << lastSync
        << ", current value: " << minSeqNr;

    if (minSeqNr < lastSync) {
      if (minSeqNr != 0) {
        LOG_TOPIC("1038e", ERR, Logger::ENGINES)
            << "min tick is smaller than "
               "safe delete tick (minSeqNr: "
            << minSeqNr << ") < (lastSync = " << lastSync << ")";
        TRI_ASSERT(false);
      }
      return ResultT<bool>::success(false);  // do not move backwards in time
    }

    TRI_ASSERT(lastSync <= minSeqNr);
    if (!didWork && !force) {
      LOG_TOPIC("1039e", TRACE, Logger::ENGINES)
          << "no collection data to serialize, updating lastSync to "
          << minSeqNr;
      _lastSync.store(minSeqNr);
      return ResultT<bool>::success(false);  // nothing was written
    }

    TRI_IF_FAILURE("TransactionChaos::randomSleep") {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }

    // prepare new settings to be written out to disk
    batch.Clear();
    _tmpBuilder.clear();
    auto newLastSync = std::max(_lastSync.load(), minSeqNr);
    ::buildSettings(_engine, _tmpBuilder, newLastSync);

    TRI_ASSERT(_tmpBuilder.slice().isObject());

    TRI_ASSERT(batch.Count() == 0);
    Result res = ::writeSettings(_tmpBuilder.slice(), batch);
    if (res.fail()) {
      LOG_TOPIC("8a5e6", WARN, Logger::ENGINES)
          << "could not write metadata settings " << res.errorMessage();
      return res;
    }

    TRI_ASSERT(didWork || force);

    // make sure everything is synced properly when we are done
    TRI_ASSERT(batch.Count() == 1);
    wo.sync = true;
    auto s = _db->Write(wo, &batch);
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    LOG_TOPIC("103ae", TRACE, Logger::ENGINES)
        << "updating lastSync to " << newLastSync;
    _lastSync.store(newLastSync);

    // we have written the settings!
    return ResultT<bool>::success(true);
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  }
}

void RocksDBSettingsManager::loadSettings() {
  RocksDBKey key;
  key.constructSettingsValue(RocksDBSettingsType::ServerTick);

  rocksdb::PinnableSlice result;
  rocksdb::Status status =
      _db->Get(rocksdb::ReadOptions(),
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::Definitions),
               key.string(), &result);
  if (status.ok()) {
    // key may not be there, so don't fail when not found
    VPackSlice slice =
        VPackSlice(reinterpret_cast<uint8_t const*>(result.data()));
    TRI_ASSERT(slice.isObject());
    LOG_TOPIC("7458b", TRACE, Logger::ENGINES)
        << "read initial settings: " << slice.toJson();

    if (!result.empty()) {
      try {
        if (slice.hasKey("tick")) {
          uint64_t lastTick =
              basics::VelocyPackHelper::stringUInt64(slice.get("tick"));
          LOG_TOPIC("369d3", TRACE, Logger::ENGINES)
              << "using last tick: " << lastTick;
          TRI_UpdateTickServer(lastTick);
        }

        if (slice.hasKey("hlc")) {
          uint64_t lastHlc =
              basics::VelocyPackHelper::stringUInt64(slice.get("hlc"));
          LOG_TOPIC("647a8", TRACE, Logger::ENGINES)
              << "using last hlc: " << lastHlc;
          TRI_HybridLogicalClock(lastHlc);
        }

        if (slice.hasKey("releasedTick")) {
          _initialReleasedTick =
              basics::VelocyPackHelper::stringUInt64(slice.get("releasedTick"));
          LOG_TOPIC("e13f4", TRACE, Logger::ENGINES)
              << "using released tick: " << _initialReleasedTick;
          _engine.releaseTick(_initialReleasedTick);
        }

        if (slice.hasKey("lastSync")) {
          _lastSync =
              basics::VelocyPackHelper::stringUInt64(slice.get("lastSync"));
          LOG_TOPIC("9e695", TRACE, Logger::ENGINES)
              << "last background settings sync: " << _lastSync;
        }
      } catch (...) {
        LOG_TOPIC("1b3de", WARN, Logger::ENGINES)
            << "unable to read initial settings: invalid data";
      }
    } else {
      LOG_TOPIC("7558b", TRACE, Logger::ENGINES) << "no initial settings found";
    }
  }
}

/// earliest safe sequence number to throw away from wal
rocksdb::SequenceNumber RocksDBSettingsManager::earliestSeqNeeded() const {
  return _lastSync.load();
}

}  // namespace arangodb
