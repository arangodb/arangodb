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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBBackgroundThread.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Replication/ReplicationClients.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "Utils/CursorRepository.h"

using namespace arangodb;

RocksDBBackgroundThread::RocksDBBackgroundThread(RocksDBEngine& eng, double interval)
    : Thread(eng.server(), "RocksDBThread"), _engine(eng), _interval(interval) {}

RocksDBBackgroundThread::~RocksDBBackgroundThread() { shutdown(); }

void RocksDBBackgroundThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void RocksDBBackgroundThread::run() {
  double const startTime = TRI_microtime();

  while (!isStopping()) {
    {
      CONDITION_LOCKER(guard, _condition);
      guard.wait(static_cast<uint64_t>(_interval * 1000000.0));
    }

    if (_engine.inRecovery()) {
      continue;
    }

    TRI_IF_FAILURE("RocksDBBackgroundThread::run") { continue; }

    try {
      if (!isStopping()) {
        try {
          // it is important that we wrap the sync operation inside a
          // try..catch of its own, because we still want the following
          // garbage collection operations to be carried out even if
          // the sync fails.
          double start = TRI_microtime();
          Result res = _engine.settingsManager()->sync(false);
          if (res.fail()) {
            LOG_TOPIC("a3d0c", WARN, Logger::ENGINES)
                << "background settings sync failed: " << res.errorMessage();
          }

          double end = TRI_microtime();
          if (end - start > 5.0) {
            LOG_TOPIC("3ad54", WARN, Logger::ENGINES)
                << "slow background settings sync: " << Logger::FIXED(end - start, 6)
                << " s";
          } else if (end - start > 0.75) {
            LOG_TOPIC("dd9ea", DEBUG, Logger::ENGINES)
                << "slow background settings sync took: " << Logger::FIXED(end - start, 6)
                << " s";
          }
        } catch (std::exception const& ex) {
          LOG_TOPIC("4652c", WARN, Logger::ENGINES)
            << "caught exception in rocksdb background sync operation: " << ex.what();
        }
      }

      bool force = isStopping();
      _engine.replicationManager()->garbageCollect(force);

      if (!force) {
        try {
          // this only schedules tree rebuilds, but the actual rebuilds are performed
          // by async tasks in the scheduler.
          _engine.processTreeRebuilds();
        } catch (std::exception const& ex) {
          LOG_TOPIC("eea93", WARN, Logger::ENGINES) 
              << "caught exception during tree rebuilding: " << ex.what();
        }
      }

      uint64_t minTick = _engine.db()->GetLatestSequenceNumber();
      auto cmTick = _engine.settingsManager()->earliestSeqNeeded();

      if (cmTick < minTick) {
        minTick = cmTick;
      }

      if (_engine.server().hasFeature<DatabaseFeature>()) {
        _engine.server().getFeature<DatabaseFeature>().enumerateDatabases(
            [&minTick](TRI_vocbase_t& vocbase) -> void {
              // lowestServedValue will return the lowest of the lastServedTick
              // values stored, or UINT64_MAX if no clients are registered
              minTick = std::min(minTick, vocbase.replicationClients().lowestServedValue());
            });
      }

      // only start pruning of obsolete WAL files a few minutes after
      // server start. if we start pruning too early, replication slaves
      // will not have a chance to reconnect to a restarted master in
      // time so the master may purge WAL files that replication slaves
      // would still like to peek into
      if (TRI_microtime() >= startTime + _engine.pruneWaitTimeInitial()) {
        // determine which WAL files can be pruned
        _engine.determinePrunableWalFiles(minTick);
        // and then prune them when they expired
        _engine.pruneWalFiles();
      } else {
        // WAL file pruning not (yet) enabled. this will be the case the 
        // first few minutes after the instance startup.
        // only keep track of which WAL files exist and what the lower
        // bound sequence number is
        _engine.determineWalFilesInitial();
      }
        
      if (!isStopping()) {
        _engine.processCompactions();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("8236f", WARN, Logger::ENGINES)
          << "caught exception in rocksdb background thread: " << ex.what();
    } catch (...) {
      LOG_TOPIC("a5f59", WARN, Logger::ENGINES)
          << "caught unknown exception in rocksdb background";
    }
  }

  try {
    _engine.settingsManager()->sync(true);  // final write on shutdown
  } catch (std::exception const& ex) {
    LOG_TOPIC("f3aa6", WARN, Logger::ENGINES)
        << "caught exception during final RocksDB sync operation: " << ex.what();
  }
}
