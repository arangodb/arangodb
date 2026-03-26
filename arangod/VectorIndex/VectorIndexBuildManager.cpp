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

#include "VectorIndex/VectorIndexBuildManager.h"

#include "Basics/ScopeGuard.h"
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
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>

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

VectorIndexBuildManager::VectorIndexBuildManager(
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

void VectorIndexBuildManager::start() {
  _thread = std::jthread([this](std::stop_token stopToken) { run(stopToken); });
}

void VectorIndexBuildManager::beginShutdown() { _thread.request_stop(); }

void VectorIndexBuildManager::stop() {
  beginShutdown();

  if (_thread.joinable()) {
    _thread.join();
  }
}

futures::Future<Result> VectorIndexBuildManager::waitForIndexReady(
    IndexId indexId) {
  futures::Promise<Result> promise;
  auto future = promise.getFuture();
  {
    std::lock_guard lock(_waitersMutex);
    _waiters[indexId.id()].push_back(std::move(promise));
  }
  return future;
}

void VectorIndexBuildManager::fulfillWaiters(IndexId indexId,
                                             Result const& result) {
  std::vector<futures::Promise<Result>> waiters;
  {
    std::lock_guard lock(_waitersMutex);
    auto it = _waiters.find(indexId.id());
    if (it == _waiters.end()) {
      return;
    }
    waiters = std::move(it->second);
    _waiters.erase(it);
  }
  for (auto& p : waiters) {
    p.setValue(result);
  }
}

void VectorIndexBuildManager::fulfillAllWaiters(Result const& result) {
  std::unordered_map<IndexId::BaseType, std::vector<futures::Promise<Result>>>
      waiters;
  {
    std::lock_guard lock(_waitersMutex);
    waiters = std::move(_waiters);
    _waiters.clear();
  }
  for (auto& [_, promises] : waiters) {
    for (auto& p : promises) {
      p.setValue(result);
    }
  }
}

bool VectorIndexBuildManager::shouldSkipRetry(
    FailedBuildsMap const& failedBuilds, std::uint64_t objectId,
    std::int64_t currentDocCount) {
  auto const it = failedBuilds.find(objectId);
  if (it == failedBuilds.end()) {
    return false;
  }
  auto const& info = it->second;
  bool backoffElapsed =
      std::chrono::steady_clock::now() - info.failedAt >= kRetryBackoff;
  bool docCountChanged = currentDocCount != info.documentCount;
  // Retry if either the backoff has elapsed or the document count has
  // changed.  Using OR avoids permanently blocking retries when the doc
  // count stays the same (e.g. after a restore with all data present).
  return !(backoffElapsed || docCountChanged);
}

void VectorIndexBuildManager::run(std::stop_token stopToken) {
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(pthread_self(), "VecIdxBuild");
#endif

  FailedBuildsMap failedBuilds;

  auto fulfillOnExit = scopeGuard([this]() noexcept {
    fulfillAllWaiters(Result{TRI_ERROR_SHUTTING_DOWN});
  });

  while (!stopToken.stop_requested()) {
    auto const deadline = std::chrono::steady_clock::now() + kScanInterval;
    while (std::chrono::steady_clock::now() < deadline &&
           !stopToken.stop_requested()) {
      {
        std::lock_guard lock(_waitersMutex);
        if (!_waiters.empty()) {
          break;
        }
      }
      std::this_thread::sleep_for(kSleepGranularity);
    }

    try {
      scanAndBuild(stopToken, failedBuilds);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e170b", WARN, Logger::ENGINES)
          << "VectorIndexBuildManager scan error: " << ex.what();
    }
  }
}

void VectorIndexBuildManager::scanAndBuild(std::stop_token const& stopToken,
                                           FailedBuildsMap& failedBuilds) {
  // Collect objectIds seen this scan to prune stale entries from
  // failedBuilds (e.g. indexes that were dropped since the last scan).
  std::unordered_set<std::uint64_t> seenObjectIds;
  // Track IndexIds of unusable indexes that have pending waiters but
  // could not be built in this scan (below threshold or in backoff).
  std::unordered_set<IndexId::BaseType> skippedWaiters;
  uint64_t unusableIndexesCount = 0;

  _dbFeature.enumerateDatabases([&](TRI_vocbase_t& vocbase) {
    if (stopToken.stop_requested()) {
      return;
    }

    auto const collections = vocbase.collections(false);
    for (auto const& coll : collections) {
      auto const indexes = coll->getPhysical()->getReadyIndexes();
      for (auto const& idx : indexes) {
        if (idx->type() != Index::TRI_IDX_TYPE_VECTOR_INDEX) {
          continue;
        }
        auto& vecIdx = static_cast<RocksDBVectorIndex&>(*idx);
        if (vecIdx.trainingState() != VectorIndexTrainingState::kUnusable) {
          fulfillWaiters(vecIdx.id(), Result{});
          continue;
        }

        ++unusableIndexesCount;
        seenObjectIds.insert(vecIdx.objectId());

        auto const* rcoll =
            static_cast<RocksDBCollection*>(coll->getPhysical());
        auto const numDocs =
            static_cast<std::int64_t>(rcoll->meta().numberDocuments());
        if (numDocs < vecIdx.trainingThreshold()) {
          skippedWaiters.insert(vecIdx.id().id());
          continue;
        }

        if (shouldSkipRetry(failedBuilds, vecIdx.objectId(), numDocs)) {
          skippedWaiters.insert(vecIdx.id().id());
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
          fulfillWaiters(vecIdx.id(), res);
          LOG_TOPIC("e164b", ERR, Logger::ENGINES)
              << "[index=" << vecIdx.id().id()
              << "] Vector build failed: " << res.errorMessage();
          failedBuilds[vecIdx.objectId()] = {std::chrono::steady_clock::now(),
                                             numDocs};

          // Report the error via MaintenanceFeature so it flows to the
          // agency Current section and becomes visible on the Coordinator.
          auto const& database = vocbase.name();
          auto const collection = std::to_string(coll->planId().id());
          auto const& shard = coll->name();
          auto const indexId = std::to_string(vecIdx.id().id());

          // Serialize the full index definition so that Current in the
          // agency carries the complete vector-index metadata, not just
          // the error/training-state fields.
          VPackBuilder indexBuilder;
          vecIdx.toVelocyPack(
              indexBuilder,
              static_cast<std::underlying_type<Index::Serialize>::type>(
                  Index::Serialize::Basics));

          VPackBuilder eb;
          {
            VPackObjectBuilder o(&eb);
            for (auto const& it : VPackObjectIterator(indexBuilder.slice())) {
              eb.add(it.key.stringView(), it.value);
            }
            eb.add(StaticStrings::Error, VPackValue(true));
            eb.add(StaticStrings::ErrorMessage, VPackValue(res.errorMessage()));
            eb.add(StaticStrings::ErrorNum, VPackValue(res.errorNumber()));
          }
          _maintenance.storeIndexError(database, collection, shard, indexId,
                                       eb.steal());

          continue;
        }

        fulfillWaiters(vecIdx.id(), Result{});

        // Clear any previous error for this index.
        {
          auto const& database = vocbase.name();
          auto const collection = std::to_string(coll->planId().id());
          auto const& shard = coll->name();
          auto const indexId = std::to_string(vecIdx.id().id());
          _maintenance.clearIndexError(database, collection, shard, indexId);
        }
        failedBuilds.erase(vecIdx.objectId());

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
  std::erase_if(failedBuilds, [&](auto const& entry) {
    return !seenObjectIds.contains(entry.first);
  });

  // Fulfill waiters for indexes that were scanned but could not be built
  // (below threshold or in retry backoff) so the REST handler doesn't hang.
  for (auto indexId : skippedWaiters) {
    fulfillWaiters(IndexId{indexId},
                   Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                          "not enough training data for vector index"});
  }
}

}  // namespace arangodb
