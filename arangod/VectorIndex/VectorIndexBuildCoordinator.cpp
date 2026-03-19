////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/VectorIndexBuildCoordinator.h"

#include "Basics/StaticStrings.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <unordered_set>

DECLARE_GAUGE(arangodb_vector_index_unusable, uint64_t,
              "Number of unusable vector indexes on this DBServer");
DECLARE_GAUGE(arangodb_vector_index_training_ongoing, uint64_t,
              "Number of vector index trainings currently ongoing");

struct VectorTrainingDurationScale {
  static arangodb::metrics::LogScale<double> scale() {
    return {10.0, 0.0, 100000.0, 10};
  }
};
DECLARE_HISTOGRAM(arangodb_vector_index_training_duration,
                  VectorTrainingDurationScale,
                  "Duration of vector index training [s]");

struct VectorIngestionDurationScale {
  static arangodb::metrics::LogScale<double> scale() {
    return {10.0, 0.0, 100000.0, 10};
  }
};
DECLARE_HISTOGRAM(arangodb_vector_index_ingestion_duration,
                  VectorIngestionDurationScale,
                  "Duration of vector index ingestion [s]");

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <pthread.h>
#endif

namespace arangodb {

VectorIndexBuildCoordinator::VectorIndexBuildCoordinator(
    DatabaseFeature& dbFeature, MaintenanceFeature& maintenance,
    metrics::MetricsFeature& metrics)
    : _dbFeature(dbFeature),
      _maintenance(maintenance),
      _untrainedCount(metrics.add(arangodb_vector_index_unusable{})),
      _trainingOngoingCount(
          metrics.add(arangodb_vector_index_training_ongoing{})),
      _trainingDuration(metrics.add(arangodb_vector_index_training_duration{})),
      _ingestionDuration(
          metrics.add(arangodb_vector_index_ingestion_duration{})) {}

void VectorIndexBuildCoordinator::start() {
  _thread = std::jthread([this](std::stop_token stopToken) { run(stopToken); });
}

void VectorIndexBuildCoordinator::beginShutdown() { _thread.request_stop(); }

void VectorIndexBuildCoordinator::stop() {
  beginShutdown();

  if (_thread.joinable()) {
    _thread.join();
  }
}

bool VectorIndexBuildCoordinator::shouldSkipRetry(
    std::uint64_t objectId, std::int64_t currentDocCount) const {
  auto const it = _failedBuilds.find(objectId);
  if (it == _failedBuilds.end()) {
    return false;
  }
  auto const& info = it->second;
  bool backoffElapsed =
      std::chrono::steady_clock::now() - info.failedAt >= kRetryBackoff;
  bool docCountChanged = currentDocCount != info.documentCount;
  // Retry only if backoff elapsed AND document count changed.
  return !(backoffElapsed && docCountChanged);
}

void VectorIndexBuildCoordinator::recordFailure(std::uint64_t objectId,
                                                std::int64_t docCount) {
  _failedBuilds[objectId] = {std::chrono::steady_clock::now(), docCount};
}

void VectorIndexBuildCoordinator::clearFailure(std::uint64_t objectId) {
  _failedBuilds.erase(objectId);
}

void VectorIndexBuildCoordinator::run(std::stop_token stopToken) {
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(pthread_self(), "VecIdxBuild");
#endif

  while (!stopToken.stop_requested()) {
    auto const deadline = std::chrono::steady_clock::now() + kScanInterval;
    while (std::chrono::steady_clock::now() < deadline &&
           !stopToken.stop_requested()) {
      std::this_thread::sleep_for(kSleepGranularity);
    }

    try {
      scanAndBuild(stopToken);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e170b", WARN, Logger::ENGINES)
          << "VectorIndexBuildCoordinator scan error: " << ex.what();
    }
  }
}

void VectorIndexBuildCoordinator::scanAndBuild(
    std::stop_token const& stopToken) {
  // Collect objectIds seen this scan to prune stale entries from
  // _failedBuilds (e.g. indexes that were dropped since the last scan).
  std::unordered_set<std::uint64_t> seenObjectIds;
  uint64_t unusableIndexesCount = 0;

  _dbFeature.enumerateDatabases([&](TRI_vocbase_t& vocbase) {
    if (stopToken.stop_requested()) {
      return;
    }

    auto const collections = vocbase.collections(false);
    for (auto const& coll : collections) {
      if (ServerState::instance()->isDBServer() && !coll->isLeadingShard()) {
        continue;
      }

      auto const indexes = coll->getPhysical()->getReadyIndexes();
      for (auto const& idx : indexes) {
        if (idx->type() != Index::TRI_IDX_TYPE_VECTOR_INDEX) {
          continue;
        }
        auto& vecIdx = static_cast<RocksDBVectorIndex&>(*idx);
        if (vecIdx.trainingState() != VectorIndexTrainingState::kUnusable) {
          continue;
        }

        ++unusableIndexesCount;
        seenObjectIds.insert(vecIdx.objectId());

        auto const* rcoll =
            static_cast<RocksDBCollection*>(coll->getPhysical());
        auto const numDocs =
            static_cast<std::int64_t>(rcoll->meta().numberDocuments());
        if (numDocs < vecIdx.trainingThreshold()) {
          continue;
        }

        if (shouldSkipRetry(vecIdx.objectId(), numDocs)) {
          continue;
        }

        LOG_TOPIC("e171b", INFO, Logger::ENGINES)
            << "[shard=" << vecIdx.collection().name()
            << ", index=" << vecIdx.id().id()
            << "] Training threshold reached (" << vecIdx.trainingThreshold()
            << " documents). Starting deferred training.";

        _trainingOngoingCount.fetch_add(1);
        auto indexPtr = std::static_pointer_cast<RocksDBIndex>(idx);
        vector::VectorIndexBuildManager builder(vecIdx);
        auto const res = builder.build(std::move(indexPtr), _trainingDuration,
                                       _ingestionDuration, stopToken);
        _trainingOngoingCount.fetch_sub(1);

        if (res.fail()) {
          LOG_TOPIC("e164b", ERR, Logger::ENGINES)
              << "[index=" << vecIdx.id().id()
              << "] Vector build failed: " << res.errorMessage();
          recordFailure(vecIdx.objectId(), numDocs);

          // Report the error via MaintenanceFeature so it flows to the
          // agency Current section and becomes visible on the Coordinator.
          auto const& database = vocbase.name();
          auto const collection = std::to_string(coll->planId().id());
          auto const& shard = coll->name();
          auto const indexId = std::to_string(vecIdx.id().id());

          VPackBuilder eb;
          {
            VPackObjectBuilder o(&eb);
            eb.add(StaticStrings::Error, VPackValue(true));
            eb.add(StaticStrings::ErrorMessage, VPackValue(res.errorMessage()));
            eb.add(StaticStrings::ErrorNum, VPackValue(res.errorNumber()));
            eb.add("id", VPackValue(indexId));
            eb.add(StaticStrings::IndexTrainingState,
                   VPackValue(trainingStateToString(vecIdx.trainingState())));
          }
          _maintenance.storeIndexError(database, collection, shard, indexId,
                                       eb.steal());

          continue;
        }

        // Persist the trained data to RocksDB so it survives a restart.
        // Mirrors the persistence step in RocksDBCollection::createIndex.
        {
          auto& engine = vocbase.engine<RocksDBEngine>();
          // Step 6. persist in rocksdb
          if (!engine.inRecovery()) {
            // write new collection marker
            auto builder = coll->toVelocyPackIgnore(
                {"path", "statusString"},
                LogicalDataSource::Serialization::PersistenceWithInProgress);
            VPackBuilder indexInfo;
            vecIdx.toVelocyPack(indexInfo,
                                Index::makeFlags(Index::Serialize::Internals));
            auto const res = engine.writeCreateCollectionMarker(
                vocbase.id(), coll->id(), builder.slice(),
                RocksDBLogValue::IndexCreate(vocbase.id(), coll->id(),
                                             indexInfo.slice()));

            if (res.fail()) {
              LOG_TOPIC("e172b", WARN, Logger::ENGINES)
                  << "[shard=" << coll->name() << ", index=" << vecIdx.id().id()
                  << "] Failed to persist trained vector index: "
                  << res.errorMessage();
            }
          }
        }

        // Clear any previous error for this index.
        {
          auto const& database = vocbase.name();
          auto const collection = std::to_string(coll->planId().id());
          auto const& shard = coll->name();
          auto const indexId = std::to_string(vecIdx.id().id());
          _maintenance.clearIndexError(database, collection, shard, indexId);
        }
        clearFailure(vecIdx.objectId());

        // Built one index — return to the scan loop so we sleep
        // before starting the next one.
        _untrainedCount.store(
            unusableIndexesCount > 0 ? unusableIndexesCount - 1 : 0,
            std::memory_order_relaxed);
        return;
      }
    }
  });

  _untrainedCount.store(unusableIndexesCount, std::memory_order_relaxed);

  // Prune failed build entries for indexes that no longer exist.
  std::erase_if(_failedBuilds, [&](auto const& entry) {
    return !seenObjectIds.contains(entry.first);
  });
}

}  // namespace arangodb
