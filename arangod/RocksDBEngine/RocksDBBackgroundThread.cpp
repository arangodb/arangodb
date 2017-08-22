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
#include "Basics/ConditionLocker.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "Utils/CursorRepository.h"

using namespace arangodb;

RocksDBBackgroundThread::RocksDBBackgroundThread(RocksDBEngine* eng,
                                                 double interval)
    : Thread("RocksDBThread"), _engine(eng), _interval(interval) {}

RocksDBBackgroundThread::~RocksDBBackgroundThread() { shutdown(); }

void RocksDBBackgroundThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void RocksDBBackgroundThread::run() {
  while (!isStopping()) {
    {
      CONDITION_LOCKER(guard, _condition);
      guard.wait(static_cast<uint64_t>(_interval * 1000000.0));
    }

    try {
      if (!isStopping()) {
        _engine->counterManager()->sync(false);
      }

      bool force = isStopping();
      _engine->replicationManager()->garbageCollect(force);

      TRI_voc_tick_t minTick = rocksutils::latestSequenceNumber();
      auto cmTick = _engine->counterManager()->earliestSeqNeeded();
      if (cmTick < minTick) {
        minTick = cmTick;
      }
      if (DatabaseFeature::DATABASE != nullptr) {
        DatabaseFeature::DATABASE->enumerateDatabases(
            [force, &minTick](TRI_vocbase_t* vocbase) {
              vocbase->cursorRepository()->garbageCollect(force);
              // FIXME: configurable interval tied to follower timeout
              vocbase->garbageCollectReplicationClients(120.0);
              auto clients = vocbase->getReplicationClients();
              for (auto c : clients) {
                if (std::get<2>(c) < minTick) {
                  minTick = std::get<2>(c);
                }
              }
            });
      }

      // determine which WAL files can be pruned
      _engine->determinePrunableWalFiles(minTick);
      // and then prune them when they expired
      _engine->pruneWalFiles();
    } catch (std::exception const& ex) {
      LOG_TOPIC(WARN, Logger::FIXME)
          << "caught exception in rocksdb background thread: " << ex.what();
    } catch (...) {
      LOG_TOPIC(WARN, Logger::FIXME)
          << "caught unknown exception in rocksdb background";
    }
  }
  _engine->counterManager()->sync(true);  // final write on shutdown
}
