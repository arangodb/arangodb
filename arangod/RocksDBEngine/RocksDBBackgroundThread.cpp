////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
        double start = TRI_microtime();
        Result res = _engine.settingsManager()->sync(false);
        if (res.fail()) {
          LOG_TOPIC("a3d0c", WARN, Logger::ENGINES)
              << "background settings sync failed: " << res.errorMessage();
        }

        double end = TRI_microtime();
        if ((end - start) > 0.75) {
          LOG_TOPIC("3ad54", WARN, Logger::ENGINES)
              << "slow background settings sync: " << Logger::FIXED(end - start, 6)
              << " s";
        }
      }

      bool force = isStopping();
      _engine.replicationManager()->garbageCollect(force);

      uint64_t minTick = rocksutils::latestSequenceNumber();
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
      }
        
    } catch (std::exception const& ex) {
      LOG_TOPIC("8236f", WARN, Logger::ENGINES)
          << "caught exception in rocksdb background thread: " << ex.what();
    } catch (...) {
      LOG_TOPIC("a5f59", WARN, Logger::ENGINES)
          << "caught unknown exception in rocksdb background";
    }
  }

  _engine.settingsManager()->sync(true);  // final write on shutdown
}
