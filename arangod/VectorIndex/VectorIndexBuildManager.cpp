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
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBEngine.h"
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

VectorIndexBuildManager::VectorIndexBuildManager(
    DatabaseFeature& dbFeature, MaintenanceFeature& maintenance,
    metrics::MetricsFeature& metrics, Scheduler& scheduler)
    : _dbFeature(dbFeature),
      _maintenance(maintenance),
      _scheduler(scheduler),
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

void VectorIndexBuildManager::fulfillBuildWaiters(IndexId indexId,
                                                  Result const& result) {
  std::vector<futures::Promise<Result>> promises;
  {
    std::lock_guard lock(_mutex);
    promises = extractWaiters(_buildWaiters, indexId);
  }
  resolveOnScheduler(_scheduler, std::move(promises), result);
}

void VectorIndexBuildManager::fulfillRetrainWaiters(IndexId oldIndexId,
                                                    Result const& result) {
  std::vector<futures::Promise<Result>> promises;
  {
    std::lock_guard lock(_mutex);
    promises = extractWaiters(_retrainWaiters, oldIndexId);
    _activeRetrains.erase(oldIndexId.id());
  }
  resolveOnScheduler(_scheduler, std::move(promises), result);
}

Result VectorIndexBuildManager::requestRetrain(
    std::shared_ptr<Index> const& oldIndex) {
  TRI_ASSERT(oldIndex != nullptr);
  if (oldIndex->type() != Index::TRI_IDX_TYPE_VECTOR_INDEX) {
    return {TRI_ERROR_BAD_PARAMETER, "retrain target is not a vector index"};
  }

  auto const* vecIdx = dynamic_cast<RocksDBVectorIndex*>(oldIndex.get());
  if (vecIdx == nullptr ||
      vecIdx->trainingState() != VectorIndexTrainingState::kReady) {
    return {TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
            "cannot retrain a vector index whose initial training has not "
            "completed"};
  }
  {
    std::lock_guard lock(_mutex);
    if (!_activeRetrains.insert(vecIdx->id().id()).second) {
      return {TRI_ERROR_ARANGO_CONFLICT,
              "a retrain is already in flight for this vector index"};
    }
    _pendingRetrains.push_back(vecIdx->id());
  }
  return {};
}

futures::Future<Result> VectorIndexBuildManager::waitForRetrainComplete(
    IndexId oldIndexId) {
  futures::Promise<Result> promise;
  auto future = promise.getFuture();
  {
    std::lock_guard lock(_mutex);
    if (!_activeRetrains.contains(oldIndexId.id())) {
      // No retrain in flight — resolve immediately.
      promise.setValue(Result{});
      return future;
    }
    _retrainWaiters[oldIndexId.id()].push_back(std::move(promise));
  }
  return future;
}

void VectorIndexBuildManager::fulfillAllWaitersOnShutdown(
    Result const& result) {
  WaiterMap buildWaiters;
  WaiterMap retrainWaiters;
  {
    std::lock_guard lock(_mutex);
    buildWaiters = std::move(_buildWaiters);
    retrainWaiters = std::move(_retrainWaiters);
    _buildWaiters.clear();
    _retrainWaiters.clear();
    _activeRetrains.clear();
    _pendingRetrains.clear();
  }
  std::vector<futures::Promise<Result>> promises;
  auto const drain = [&](WaiterMap& map) {
    for (auto& bucket : std::views::values(map)) {
      std::ranges::move(bucket, std::back_inserter(promises));
    }
  };
  drain(buildWaiters);
  drain(retrainWaiters);
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
        if (!_buildWaiters.empty() || !_retrainWaiters.empty() ||
            !_pendingRetrains.empty()) {
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
  // Snapshot queued retrains by old IndexId so we can match them in-line
  // while iterating each collection's indexes — no second pass needed.
  {
    std::lock_guard lock(_mutex);
    for (auto const id : _pendingRetrains) {
      state.pendingRetrains.insert(id.id());
    }
    _pendingRetrains.clear();
  }

  _dbFeature.enumerateDatabases([&](TRI_vocbase_t& vocbase) {
    if (stopToken.stop_requested()) {
      return;
    }
    for (auto const& coll : vocbase.collections(false)) {
      for (auto const& idx : coll->getPhysical()->getReadyIndexes()) {
        if (idx->type() != Index::TRI_IDX_TYPE_VECTOR_INDEX) {
          continue;
        }
        auto result = processVectorIndex(state.pendingRetrains, failedBuilds,
                                         vocbase, *coll, idx, stopToken);
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

  // Any retrain whose target index didn't turn up during the scan: either
  // shutdown bailed us out (requeue for the shutdown guard to fulfill) or
  // the collection/database/index was dropped (fulfill with error).
  if (!state.pendingRetrains.empty()) {
    if (!scanCompletedFully) {
      std::lock_guard lock(_mutex);
      for (auto const id : state.pendingRetrains) {
        _pendingRetrains.push_back(IndexId{id});
      }
    } else {
      for (auto const id : state.pendingRetrains) {
        fulfillRetrainWaiters(
            IndexId{id}, Result{TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                "index was dropped before retrain could run"});
      }
    }
  }
}

VectorIndexBuildManager::IndexScanResult
VectorIndexBuildManager::processVectorIndex(
    std::unordered_set<IndexId::BaseType>& pendingRetrains,
    FailedBuildsMap& failedBuilds, TRI_vocbase_t& vocbase,
    LogicalCollection& coll, std::shared_ptr<Index> const& idx,
    std::stop_token const& stopToken) {
  auto& vecIdx = static_cast<RocksDBVectorIndex&>(*idx);
  IndexScanResult result;
  result.indexId = vecIdx.id();

  switch (vecIdx.trainingState()) {
    case VectorIndexTrainingState::kReady: {
      fulfillBuildWaiters(vecIdx.id(), Result{});
      // If a retrain was queued for this index, handle it here — we
      // already have vocbase, coll, and idx resolved.
      if (auto rit = pendingRetrains.find(vecIdx.id().id());
          rit != pendingRetrains.end() && !stopToken.stop_requested()) {
        runRetrain(vocbase, coll, idx, stopToken);
        pendingRetrains.erase(rit);
      }
      return result;
    }
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
  auto indexPtr = std::static_pointer_cast<RocksDBIndex>(idx);
  VectorIndexBuilder builder(vecIdx);
  auto const res = builder.build(std::move(indexPtr), _trainingDuration,
                                 _ingestionDuration, stopToken);
  _trainingOngoingCount.fetch_sub(1);

  if (res.fail()) {
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

namespace {

/// Build the VPack definition for a fresh shadow vector index, copying
/// everything from the old index's internal definition except its IndexId
/// and objectId (which will be re-generated when the shadow is
/// instantiated).
velocypack::Builder buildShadowDefinition(RocksDBVectorIndex const& old) {
  velocypack::Builder oldInfo;
  old.toVelocyPack(oldInfo, Index::makeFlags(Index::Serialize::Internals));

  velocypack::Builder shadow;
  {
    VPackObjectBuilder ob(&shadow);
    for (auto pair : VPackObjectIterator(oldInfo.slice())) {
      auto key = pair.key.stringView();
      // Skip fields that must be regenerated or that describe transient
      // state we don't want to carry over.
      if (key == StaticStrings::IndexId || key == StaticStrings::ObjectId ||
          key == StaticStrings::IndexTrainingState ||
          key == StaticStrings::IndexResolvedNLists ||
          key == StaticStrings::Error || key == StaticStrings::ErrorMessage ||
          key == StaticStrings::ErrorNum) {
        continue;
      }
      shadow.add(key, pair.value);
    }
    // IndexId is passed separately to the factory's instantiate() call;
    // objectId is auto-generated inside RocksDBIndex when absent from
    // the slice.
    shadow.add(StaticStrings::IndexId,
               VPackValue(std::to_string(Index::generateId().id())));
  }
  return shadow;
}

}  // namespace

void VectorIndexBuildManager::runRetrain(TRI_vocbase_t& vocbase,
                                         LogicalCollection& coll,
                                         std::shared_ptr<Index> const& oldIdx,
                                         std::stop_token const& stopToken) {
  TRI_ASSERT(oldIdx != nullptr);
  TRI_ASSERT(oldIdx->type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);

  auto const oldIndexId = oldIdx->id();
  // Release the retrain waiters on exit. fulfillRetrainWaiters() also
  // removes the id from _activeRetrains, admitting new retrain requests
  // for this index.
  Result retrainResult;
  auto finishGuard = scopeGuard(
      [&]() noexcept { fulfillRetrainWaiters(oldIndexId, retrainResult); });

  auto& oldVec = static_cast<RocksDBVectorIndex&>(*oldIdx);
  auto* rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());
  TRI_ASSERT(rcoll != nullptr);

  if (rcoll->meta().numberDocuments() < oldVec.trainingThreshold()) {
    retrainResult = Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                           "collection has fewer documents than the vector "
                           "index training threshold requires"};
    return;
  }

  // Construct the shadow index. Its objectId is auto-generated inside
  // RocksDBIndex's constructor because the slice does not carry one.
  auto shadowDef = buildShadowDefinition(oldVec);

  std::shared_ptr<Index> shadow;
  try {
    auto& engine = vocbase.engine<RocksDBEngine>();
    shadow = engine.indexFactory().prepareIndexFromSlice(
        shadowDef.slice(), /*generateKey*/ false, coll,
        /*isClusterConstructor*/ false);
  } catch (std::exception const& ex) {
    retrainResult = Result{TRI_ERROR_ARANGO_INDEX_CREATION_FAILED,
                           std::string{"failed to construct retrain shadow "
                                       "vector index: "} +
                               ex.what()};
    return;
  }
  TRI_ASSERT(shadow != nullptr);
  TRI_ASSERT(shadow->type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
  TRI_ASSERT(shadow->id() != oldIndexId);

  auto shadowRocks = std::static_pointer_cast<RocksDBIndex>(shadow);
  auto& shadowVec = static_cast<RocksDBVectorIndex&>(*shadow);

  // Insert the shadow into the collection's in-memory index set. No
  // Definitions CF write happens here — the shadow is only persisted at
  // the atomic swap step below, when dropIndex(oldIndexId) rewrites the
  // collection marker with the shadow included.
  rcoll->addShadowIndex(shadow);

  LOG_TOPIC("e172b", INFO, Logger::ENGINES)
      << "[shard=" << coll.name() << ", oldIndex=" << oldIndexId.id()
      << ", shadow=" << shadow->id().id() << "] Starting vector index retrain.";

  _trainingOngoingCount.fetch_add(1);
  VectorIndexBuilder builder(shadowVec);
  auto buildRes = builder.build(shadowRocks, _trainingDuration,
                                _ingestionDuration, stopToken);
  _trainingOngoingCount.fetch_sub(1);

  if (buildRes.fail()) {
    LOG_TOPIC("e173b", ERR, Logger::ENGINES)
        << "[shard=" << coll.name() << ", oldIndex=" << oldIndexId.id()
        << ", shadow=" << shadow->id().id()
        << "] Vector index retrain build failed: " << buildRes.errorMessage();
    rcoll->abortShadowIndex(shadow);
    retrainResult = buildRes;
    return;
  }

  // Shadow is kReady and in _indexes. Drop the old atomically via the
  // existing drop-index path: this removes old from _indexes, writes a
  // single Definitions CF marker containing the shadow (since shadow is
  // currently in _indexes when the marker is serialized), writes an
  // IndexDrop WAL log value, and range-deletes all entries under the
  // old objectId.
  auto dropRes = coll.getPhysical()->dropIndex(oldIndexId);
  if (dropRes.fail()) {
    LOG_TOPIC("e174b", ERR, Logger::ENGINES)
        << "[shard=" << coll.name() << ", oldIndex=" << oldIndexId.id()
        << "] Failed to drop old vector index after successful retrain "
           "build: "
        << dropRes.errorMessage()
        << ". Shadow will remain alongside the old index until the next "
           "retrain or server restart.";
    retrainResult = dropRes;
    return;
  }

  LOG_TOPIC("e175b", INFO, Logger::ENGINES)
      << "[shard=" << coll.name() << ", oldIndex=" << oldIndexId.id()
      << ", shadow=" << shadow->id().id() << "] Vector index retrain complete.";
}

}  // namespace arangodb::vector
