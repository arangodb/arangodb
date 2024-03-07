////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Logger/LogMacros.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Replication/ReplicationClients.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBDumpManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "Utils/CursorRepository.h"

#include <atomic>

using namespace arangodb;

DECLARE_GAUGE(rocksdb_wal_released_tick_replication, uint64_t,
              "Released tick for RocksDB WAL deletion (replication-induced)");

RocksDBBackgroundThread::RocksDBBackgroundThread(RocksDBEngine& engine,
                                                 double interval)
    : Thread(engine.server(), "RocksDBThread"),
      _engine(engine),
      _interval(interval),
      _metricsWalReleasedTickReplication(
          engine.server().getFeature<metrics::MetricsFeature>().add(
              rocksdb_wal_released_tick_replication{})) {}

RocksDBBackgroundThread::~RocksDBBackgroundThread() { shutdown(); }

void RocksDBBackgroundThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  std::lock_guard guard{_condition.mutex};
  _condition.cv.notify_all();
}

void RocksDBBackgroundThread::run() {
  FlushFeature& flushFeature = _engine.server().getFeature<FlushFeature>();

  double const startTime = TRI_microtime();
  uint64_t runsUntilSyncForced = 1;
  constexpr uint64_t maxRunsUntilSyncForced = 5;

  while (!isStopping()) {
    {
      std::unique_lock guard{_condition.mutex};
      _condition.cv.wait_for(guard,
                             std::chrono::microseconds{
                                 static_cast<uint64_t>(_interval * 1000000.0)});
    }

    if (_engine.inRecovery()) {
      continue;
    }

    TRI_IF_FAILURE("RocksDBBackgroundThread::run") { continue; }

    try {
      if (!isStopping()) {
        flushFeature.releaseUnusedTicks();

        // it is important that we wrap the sync operation inside a
        // try..catch of its own, because we still want the following
        // garbage collection operations to be carried out even if
        // the sync fails.
        try {
          // forceSync will effectively be true for the initial run that
          // will happen when the recovery has finished. that way we
          // can quickly push forward the WAL lower bound value after
          // the recovery
          bool forceSync = false;

          // force a sync after at most x iterations (or initial run)
          if (runsUntilSyncForced > 0 && --runsUntilSyncForced == 0) {
            TRI_ASSERT(runsUntilSyncForced == 0);
            forceSync = true;
          }

          TRI_IF_FAILURE("BuilderIndex::purgeWal") { forceSync = true; }

          LOG_TOPIC("34a21", TRACE, Logger::ENGINES)
              << "running " << (forceSync ? "forced " : "")
              << "background settings sync";

          double start = TRI_microtime();
          auto syncRes = _engine.settingsManager()->sync(forceSync);
          double end = TRI_microtime();

          if (syncRes.fail()) {
            LOG_TOPIC("a3d0c", WARN, Logger::ENGINES)
                << "background settings sync failed: "
                << syncRes.errorMessage();
          } else if (syncRes.get()) {
            // reset our counter
            runsUntilSyncForced = maxRunsUntilSyncForced;
          }

          if (end - start > 5.0) {
            LOG_TOPIC("3ad54", WARN, Logger::ENGINES)
                << "slow background settings sync took: "
                << Logger::FIXED(end - start, 6) << " s";
          } else if (end - start > 0.75) {
            LOG_TOPIC("dd9ea", DEBUG, Logger::ENGINES)
                << "slow background settings sync took: "
                << Logger::FIXED(end - start, 6) << " s";
          }
        } catch (std::exception const& ex) {
          LOG_TOPIC("4652c", WARN, Logger::ENGINES)
              << "caught exception in rocksdb background sync operation: "
              << ex.what();
        }
      }

      bool force = isStopping();
      _engine.replicationManager()->garbageCollect(force);
      _engine.dumpManager()->garbageCollect(force);

      if (!force) {
        try {
          // this only schedules tree rebuilds, but the actual rebuilds are
          // performed by async tasks in the scheduler.
          _engine.processTreeRebuilds();
        } catch (std::exception const& ex) {
          LOG_TOPIC("eea93", WARN, Logger::ENGINES)
              << "caught exception during tree rebuilding: " << ex.what();
        }
      }

      uint64_t const latestSeqNo = _engine.db()->GetLatestSequenceNumber();
      auto const earliestSeqNeeded =
          _engine.settingsManager()->earliestSeqNeeded();

      uint64_t minTick = latestSeqNo;

      if (earliestSeqNeeded < minTick) {
        minTick = earliestSeqNeeded;
      }

      uint64_t minTickForReplication = latestSeqNo;
      if (_engine.server().hasFeature<DatabaseFeature>()) {
        _engine.server().getFeature<DatabaseFeature>().enumerateDatabases(
            [&minTickForReplication, minTick](TRI_vocbase_t& vocbase) -> void {
              // lowestServedValue will return the lowest of the lastServedTick
              // values stored, or UINT64_MAX if no clients are registered
              TRI_voc_tick_t lowestServedValue =
                  vocbase.replicationClients().lowestServedValue();

              if (lowestServedValue != UINT64_MAX) {
                // only log noteworthy things
                LOG_TOPIC("e979f", DEBUG, Logger::ENGINES)
                    << "lowest served tick for database '" << vocbase.name()
                    << "': " << lowestServedValue << ", minTick: " << minTick
                    << ", minTickForReplication: " << minTickForReplication;
              }

              minTickForReplication =
                  std::min(minTickForReplication, lowestServedValue);
            });

        minTick = std::min(minTick, minTickForReplication);
      }
      _metricsWalReleasedTickReplication.store(minTickForReplication,
                                               std::memory_order_relaxed);

      LOG_TOPIC("cfe65", DEBUG, Logger::ENGINES)
          << "latest seq number: " << latestSeqNo
          << ", earliest seq needed: " << earliestSeqNeeded
          << ", min tick for replication: " << minTickForReplication;

      try {
        _engine.flushOpenFilesIfRequired();
      } catch (...) {
        // whatever happens here, we don't want it to block/skip any of
        // the following operations
      }

      bool canPrune =
          TRI_microtime() >= startTime + _engine.pruneWaitTimeInitial();
      TRI_IF_FAILURE("BuilderIndex::purgeWal") { canPrune = true; }

      // only start pruning of obsolete WAL files a few minutes after
      // server start. if we start pruning too early, replication followers
      // will not have a chance to reconnect to a restarted leader in
      // time so the leader may purge WAL files that replication followers
      // would still like to peek into
      if (canPrune) {
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

  // final write on shutdown
  auto syncRes = _engine.settingsManager()->sync(/*force*/ true);
  if (syncRes.fail()) {
    LOG_TOPIC("f3aa6", WARN, Logger::ENGINES)
        << "caught exception during final RocksDB sync operation: "
        << syncRes.errorMessage();
  }
}
