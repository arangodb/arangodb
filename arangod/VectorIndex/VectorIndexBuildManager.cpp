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

#include "Basics/Exceptions.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
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
#include "Scheduler/Scheduler.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <format>
#include <ranges>
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

namespace arangodb::vector {

namespace {

// Post setValue onto the scheduler so continuations don't run on the
// build manager thread (which could cause deadlocks).
void resolveOnScheduler(Scheduler& scheduler,
                        std::vector<futures::Promise<Result>>&& promises,
                        Result const& result) {
  for (auto& p : promises) {
    scheduler.queue(RequestLane::CONTINUATION,
                    [promise = std::move(p), result]() mutable {
                      promise.setValue(result);
                    });
  }
}

// Move out and erase all promises registered under `id` in `map`. Caller
// holds the owning mutex.
std::vector<futures::Promise<Result>> extractWaiters(
    std::unordered_map<IndexId::BaseType,
                       std::vector<futures::Promise<Result>>>& map,
    IndexId id) {
  auto it = map.find(id.id());
  if (it == map.end()) {
    return {};
  }
  auto out = std::move(it->second);
  map.erase(it);
  return out;
}

}  // namespace

VectorIndexBuildManager::VectorIndexBuildManager(
    DatabaseFeature& dbFeature, MaintenanceFeature& maintenance,
    metrics::MetricsFeature& metrics, Scheduler& scheduler)
    : _dbFeature(dbFeature),
      _maintenance(maintenance),
      _scheduler(scheduler),
      _resourceMonitor(GlobalResourceMonitor::instance()),
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
    std::lock_guard lock(_mutex);
    _buildWaiters[indexId.id()].push_back(std::move(promise));
  }
  return future;
}

void VectorIndexBuildManager::fulfillBuildWaiters(IndexId indexId,
                                                  Result const& result) {
  std::vector<futures::Promise<Result>> promises;
  {
    std::lock_guard lock(_mutex);
    promises = extractWaiters(_buildWaiters, indexId);
  }
  resolveOnScheduler(_scheduler, std::move(promises), result);
}

void VectorIndexBuildManager::fulfillAllWaitersOnShutdown(
    Result const& result) {
  BuildWaiterMap buildWaiters;
  {
    std::lock_guard lock(_mutex);
    buildWaiters = std::move(_buildWaiters);
    _buildWaiters.clear();
  }
  std::vector<futures::Promise<Result>> promises;
  for (auto& bucket : std::views::values(buildWaiters)) {
    std::ranges::move(bucket, std::back_inserter(promises));
  }
  resolveOnScheduler(_scheduler, std::move(promises), result);
}

bool VectorIndexBuildManager::shouldSkipRetry(
    FailedBuildsMap const& failedBuilds, std::uint64_t objectId,
    std::uint64_t currentDocCount) {
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

  auto const fulfillOnExit = scopeGuard([this]() noexcept {
    fulfillAllWaitersOnShutdown(Result{TRI_ERROR_SHUTTING_DOWN});
  });

  // TODO(vector-retrain): revisit whether we need a startup sweep of
  // orphan VectorIndex CF ranges. A crash between persisting shadow
  // ingestion entries and committing the swap would leave per-objectId
  // residue that is never referenced again. The current retrain flow
  // should not produce this state, but confirm once the on-disk layout
  // and crash-recovery story are finalized.
  while (!stopToken.stop_requested()) {
    // lets wait 5 seconds before each scan
    auto const deadline = std::chrono::steady_clock::now() + kScanInterval;
    while (std::chrono::steady_clock::now() < deadline &&
           !stopToken.stop_requested()) {
      {
        std::lock_guard lock(_mutex);
        if (!_buildWaiters.empty()) {
          break;
        }
      }
      std::this_thread::sleep_for(kSleepGranularity);
    }

    try {
      scanAndProcess(stopToken, failedBuilds);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e170b", WARN, Logger::ENGINES)
          << "VectorIndexBuildManager scan error: " << ex.what();
    }
  }
}

void VectorIndexBuildManager::scanAndProcess(std::stop_token const& stopToken,
                                             FailedBuildsMap& failedBuilds) {
  ScanState state;

  _dbFeature.enumerateDatabases([&](TRI_vocbase_t& vocbase) {
    if (stopToken.stop_requested()) {
      return;
    }
    for (auto const& coll : vocbase.collections(false)) {
      for (auto const& idx : coll->getPhysical()->getReadyIndexes()) {
        if (idx->type() != Index::TRI_IDX_TYPE_VECTOR_INDEX) {
          continue;
        }
        auto result =
            processVectorIndex(failedBuilds, vocbase, *coll, idx, stopToken);
        state.seenIndexIds.insert(result.indexId.id());
        if (result.unusableObjectId.has_value()) {
          state.seenObjectIds.insert(*result.unusableObjectId);
          ++state.unusableIndexesCount;
        }
        if (result.skippedWaiter) {
          state.skippedWaiters.insert(result.indexId.id());
        }
        if (result.buildCompleted) {
          // Publish the decremented untrained count so observers see
          // progress mid-scan. The final post-scan store overwrites
          // this with the total unusable count for the pass.
          _untrainedCount.store(state.unusableIndexesCount > 0
                                    ? state.unusableIndexesCount - 1
                                    : 0,
                                std::memory_order_relaxed);
        }
      }
    }
  });

  bool const scanCompletedFully = !stopToken.stop_requested();
  _untrainedCount.store(state.unusableIndexesCount, std::memory_order_relaxed);

  // Prune failed build entries for indexes that no longer exist.
  std::erase_if(failedBuilds, [&](auto const& entry) {
    return !state.seenObjectIds.contains(entry.first);
  });

  // Fulfill waiters for indexes that were scanned but could not be built
  // (below threshold or in retry backoff) so the REST handler doesn't hang.
  for (auto const indexId : state.skippedWaiters) {
    fulfillBuildWaiters(IndexId{indexId},
                        Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                               "vector index not ready"});
  }

  // Fulfill waiters for indexes that no longer exist (dropped while a
  // request was waiting).  Only safe when the scan visited every
  // database/collection without an early return.
  if (scanCompletedFully) {
    std::vector<IndexId::BaseType> orphaned;
    {
      std::lock_guard lock(_mutex);
      for (auto const& [id, bucket] : _buildWaiters) {
        if (!state.seenIndexIds.contains(id)) {
          orphaned.push_back(id);
        }
      }
    }
    for (auto const id : orphaned) {
      fulfillBuildWaiters(IndexId{id}, Result{TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                              "index was dropped"});
    }
  }
}

VectorIndexBuildManager::IndexScanResult
VectorIndexBuildManager::processVectorIndex(FailedBuildsMap& failedBuilds,
                                            TRI_vocbase_t& vocbase,
                                            LogicalCollection& coll,
                                            std::shared_ptr<Index> const& idx,
                                            std::stop_token const& stopToken) {
  auto& vecIdx = static_cast<RocksDBVectorIndex&>(*idx);
  IndexScanResult result;
  result.indexId = vecIdx.id();

  switch (vecIdx.trainingState()) {
    case VectorIndexTrainingState::kReady:
      fulfillBuildWaiters(vecIdx.id(), Result{});
      return result;
    case VectorIndexTrainingState::kTraining:
    case VectorIndexTrainingState::kIngesting:
      // Build in progress — keep waiters pending until it finishes.
      return result;
    case VectorIndexTrainingState::kUnusable:
      break;
  }

  result.unusableObjectId = vecIdx.objectId();

  auto const* rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());
  auto const numDocs = rcoll->meta().numberDocuments();
  if (numDocs < vecIdx.trainingThreshold()) {
    result.skippedWaiter = true;
    reportIndexError(vocbase, coll, vecIdx,
                     Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                            std::format("not enough training data for vector "
                                        "index, need at least {} documents "
                                        "but only {} present",
                                        vecIdx.trainingThreshold(), numDocs)});
    return result;
  }

  if (shouldSkipRetry(failedBuilds, vecIdx.objectId(), numDocs)) {
    result.skippedWaiter = true;
    return result;
  }

  LOG_TOPIC("e171b", INFO, Logger::ENGINES)
      << "[shard=" << vecIdx.collection().name()
      << ", index=" << vecIdx.id().id() << "] Training threshold reached ("
      << vecIdx.trainingThreshold()
      << " documents). Starting deferred training.";

  _trainingOngoingCount.fetch_add(1);
  auto const decrementOnExit =
      scopeGuard([this]() noexcept { _trainingOngoingCount.fetch_sub(1); });
  auto indexPtr = std::static_pointer_cast<RocksDBIndex>(idx);
  VectorIndexBuilder builder(vecIdx, _resourceMonitor);

  // build() can throw via VectorIndexTrainer::train (ResourceUsageScope
  // memory-limit overflow, faiss exceptions, factory-string mismatches).
  // Translate to Result so the cleanup below runs uniformly.
  auto const res = [&]() -> Result {
    try {
      return builder.build(std::move(indexPtr), _trainingDuration,
                           _ingestionDuration, stopToken);
    } catch (basics::Exception const& e) {
      return {e.code(), e.message()};
    } catch (std::exception const& e) {
      return {TRI_ERROR_INTERNAL, e.what()};
    }
  }();

  if (res.fail()) {
    // Surface the failure on the in-memory index so GET /_api/index and the
    // per-shard state endpoint return the actual error (not the stale or
    // default string). Reset the training state idempotently in case the
    // builder threw before reaching its own resetTrainingState() — otherwise
    // the index stays stuck in kTraining and the next scan skips it.
    vecIdx.setTrainingError(std::string{res.errorMessage()});
    vecIdx.resetTrainingState();
    fulfillBuildWaiters(vecIdx.id(), res);
    LOG_TOPIC("e164b", ERR, Logger::ENGINES)
        << "[index=" << vecIdx.id().id()
        << "] Vector build failed: " << res.errorMessage();
    failedBuilds[vecIdx.objectId()] = {std::chrono::steady_clock::now(),
                                       numDocs};
    reportIndexError(vocbase, coll, vecIdx, res);
    return result;
  }

  fulfillBuildWaiters(vecIdx.id(), Result{});
  clearIndexError(vocbase, coll, vecIdx);
  failedBuilds.erase(vecIdx.objectId());
  result.buildCompleted = true;
  return result;
}

void VectorIndexBuildManager::reportIndexError(TRI_vocbase_t const& vocbase,
                                               LogicalCollection const& coll,
                                               RocksDBVectorIndex const& vecIdx,
                                               Result const& error) {
  auto const& database = vocbase.name();
  auto const collection = std::to_string(coll.planId().id());
  auto const& shard = coll.name();
  auto const indexId = std::to_string(vecIdx.id().id());

  // Serialize the full index definition so that Current in the agency
  // carries the complete vector-index metadata, not just the
  // error/training-state fields.
  VPackBuilder indexBuilder;
  vecIdx.toVelocyPack(indexBuilder,
                      static_cast<std::underlying_type<Index::Serialize>::type>(
                          Index::Serialize::Basics));

  VPackBuilder eb;
  {
    VPackObjectBuilder o(&eb);
    for (auto const& it : VPackObjectIterator(indexBuilder.slice())) {
      eb.add(it.key.stringView(), it.value);
    }
    eb.add(StaticStrings::Error, VPackValue(true));
    eb.add(StaticStrings::ErrorMessage, VPackValue(error.errorMessage()));
    eb.add(StaticStrings::ErrorNum, VPackValue(error.errorNumber()));
  }
  _maintenance.storeIndexError(database, collection, shard, indexId,
                               eb.steal());
}

void VectorIndexBuildManager::clearIndexError(
    TRI_vocbase_t const& vocbase, LogicalCollection const& coll,
    RocksDBVectorIndex const& vecIdx) {
  auto const& database = vocbase.name();
  auto const collection = std::to_string(coll.planId().id());
  auto const& shard = coll.name();
  auto const indexId = std::to_string(vecIdx.id().id());
  _maintenance.clearIndexError(database, collection, shard, indexId);
}

}  // namespace arangodb::vector
