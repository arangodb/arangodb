////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBSyncThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/RocksDBUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBEngine.h"

#include <rocksdb/status.h>
#include <rocksdb/utilities/transaction_db.h>

using namespace arangodb;

RocksDBSyncThread::RocksDBSyncThread(RocksDBEngine& engine, std::chrono::milliseconds interval,
                                     std::chrono::milliseconds delayThreshold)
    : Thread(engine.server(), "RocksDBSync"),
      _engine(engine),
      _interval(interval),
      _lastSyncTime(std::chrono::steady_clock::now()),
      _lastSequenceNumber(0),
      _delayThreshold(delayThreshold) {}

RocksDBSyncThread::~RocksDBSyncThread() { shutdown(); }

Result RocksDBSyncThread::syncWal() {
  // note the following line in RocksDB documentation (rocksdb/db.h):
  // > Currently only works if allow_mmap_writes = false in Options.
  TRI_ASSERT(!_engine.rocksDBOptions().allow_mmap_writes);

  auto db = _engine.db()->GetBaseDB();

  // set time of last syncing under the lock
  auto const now = std::chrono::steady_clock::now();
  {
    CONDITION_LOCKER(guard, _condition);

    if (now > _lastSyncTime) {
      // update last sync time...
      _lastSyncTime = now;
    }

    auto lastSequenceNumber = db->GetLatestSequenceNumber();

    if (lastSequenceNumber > _lastSequenceNumber) {
      // update last sequence number
      _lastSequenceNumber = lastSequenceNumber;
    }
  }

  // actual syncing is done without holding the lock
  return sync(db);
}

Result RocksDBSyncThread::sync(rocksdb::DB* db) {
#ifndef _WIN32
  // if called on Windows, we would get the following error from RocksDB:
  // > Not implemented: SyncWAL() is not supported for this implementation of
  // WAL file
  LOG_TOPIC("a3978", TRACE, Logger::ENGINES) << "syncing RocksDB WAL";

  rocksdb::Status status = db->SyncWAL();
  if (!status.ok()) {
    return rocksutils::convertStatus(status);
  }
#endif
  return Result();
}

void RocksDBSyncThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void RocksDBSyncThread::run() {
  auto db = _engine.db()->GetBaseDB();

  LOG_TOPIC("11872", TRACE, Logger::ENGINES)
      << "starting RocksDB sync thread with interval " << _interval.count()
      << " milliseconds";

  while (!isStopping()) {
    try {
      auto const now = std::chrono::steady_clock::now();

      rocksdb::SequenceNumber lastSequenceNumber;
      rocksdb::SequenceNumber previousLastSequenceNumber;
      std::chrono::time_point<std::chrono::steady_clock> lastSyncTime;
      std::chrono::time_point<std::chrono::steady_clock> previousLastSyncTime;

      {
        // wait for time to elapse, and after that update last sync time
        CONDITION_LOCKER(guard, _condition);

        previousLastSequenceNumber = _lastSequenceNumber;
        previousLastSyncTime = _lastSyncTime;
        auto const end = _lastSyncTime + _interval;
        if (end > now) {
          guard.wait(std::chrono::microseconds(
              std::chrono::duration_cast<std::chrono::microseconds>(end - now)));
        }

        if (_lastSyncTime > previousLastSyncTime) {
          // somebody else outside this thread has called sync...
          continue;
        }

        lastSyncTime = std::chrono::steady_clock::now();
        lastSequenceNumber = db->GetLatestSequenceNumber();

        if (lastSequenceNumber == previousLastSequenceNumber) {
          // nothing to sync, so don't cause unnecessary load.
          // still update our lastSyncTime to now, so we don't run into warnings
          // later with syncs being reported as delayed
          _lastSyncTime = lastSyncTime;
          continue;
        }
      }

      {
        if (_delayThreshold.count() > 0 && (lastSyncTime - previousLastSyncTime) > _delayThreshold) {
          LOG_TOPIC("5b708", INFO, Logger::ENGINES)
            << "last RocksDB WAL sync happened longer ago than configured threshold. "
            << "last sync happened " << (std::chrono::duration_cast<std::chrono::milliseconds>(lastSyncTime - previousLastSyncTime)).count() << " ms ago, "
            << "threshold value: " << _delayThreshold.count() << " ms";
        }
      }

      Result res = this->sync(db);

      if (res.ok()) {
        // success case
        CONDITION_LOCKER(guard, _condition);

        if (lastSequenceNumber > _lastSequenceNumber) {
          // bump last sequence number we have synced
          _lastSequenceNumber = lastSequenceNumber;
        }
        if (lastSyncTime > _lastSyncTime) {
          _lastSyncTime = lastSyncTime;
        }
      } else {
        // could not sync... in this case, don't advance our last
        // sync time and last synced sequence number
        LOG_TOPIC("5e275", ERR, Logger::ENGINES)
            << "could not sync RocksDB WAL: " << res.errorMessage();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("77b1e", ERR, Logger::ENGINES)
          << "caught exception in RocksDBSyncThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC("90e8e", ERR, Logger::ENGINES)
          << "caught unknown exception in RocksDBSyncThread";
    }
  }
}
