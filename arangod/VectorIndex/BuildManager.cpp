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
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/BuildManager.h"

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
#include "Metrics/IRegistry.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "Scheduler/Scheduler.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>

#include <format>
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

BuildManager::BuildManager(DatabaseFeature& dbFeature,
                           MaintenanceFeature& maintenance,
                           metrics::IRegistry& metricsRegistry,
                           Scheduler& scheduler,
                           std::chrono::duration<double> retryBackoff)
    : _dbFeature(dbFeature),
      _maintenance(maintenance),
      _scheduler(scheduler),
      _retryBackoff(retryBackoff),
      _resourceMonitor(GlobalResourceMonitor::instance()),
      _untrainedCount(metricsRegistry.add(arangodb_vector_index_unusable{})),
      _trainingOngoingCount(
          metricsRegistry.add(arangodb_vector_index_training_ongoing{})),
      _trainingDuration(
          metricsRegistry.add(arangodb_vector_index_training_duration{})),
      _ingestionDuration(
          metricsRegistry.add(arangodb_vector_index_ingestion_duration{})) {}

void BuildManager::start() {
  _thread = std::jthread([this](std::stop_token stopToken) { run(stopToken); });
}

void BuildManager::beginShutdown() { _thread.request_stop(); }

void BuildManager::stop() {
  beginShutdown();

  if (_thread.joinable()) {
    _thread.join();
  }
}

futures::Future<Result> BuildManager::waitForIndexReady(IndexId indexId) {
  futures::Promise<Result> promise;
  auto future = promise.getFuture();
  {
    std::lock_guard lock(_waitersMutex);
    _waiters[indexId.id()].push_back(std::move(promise));
  }
  return future;
}

void BuildManager::fulfillWaiters(IndexId indexId, Result const& result) {
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
  // Post setValue onto the scheduler so continuations don't run on the
  // build manager thread (which could cause deadlocks).
  for (auto& p : waiters) {
    _scheduler.queue(RequestLane::CONTINUATION,
                     [promise = std::move(p), result]() mutable {
                       promise.setValue(result);
                     });
  }
}

void BuildManager::fulfillAllWaiters(Result const& result) {
  std::unordered_map<IndexId::BaseType, std::vector<futures::Promise<Result>>>
      waiters;
  {
    std::lock_guard lock(_waitersMutex);
    waiters = std::move(_waiters);
    _waiters.clear();
  }
  for (auto& [_, promises] : waiters) {
    for (auto& p : promises) {
      _scheduler.queue(RequestLane::CONTINUATION,
                       [promise = std::move(p), result]() mutable {
                         promise.setValue(result);
                       });
    }
  }
}

bool BuildManager::shouldSkipRetry(FailedBuildsMap const& failedBuilds,
                                   std::uint64_t objectId,
                                   std::uint64_t currentDocCount) const {
  auto const it = failedBuilds.find(objectId);
  if (it == failedBuilds.end()) {
    return false;
  }
  auto const& info = it->second;
  bool backoffElapsed =
      std::chrono::steady_clock::now() - info.failedAt >= _retryBackoff;
  bool docCountChanged = currentDocCount != info.documentCount;
  // Retry if either the backoff has elapsed or the document count has
  // changed. Using OR avoids permanently blocking retries when the doc
  // count stays the same (e.g. after a restore with all data present).
  return !(backoffElapsed || docCountChanged);
}

void BuildManager::run(std::stop_token stopToken) {
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(pthread_self(), "VecIdxBuild");
#endif

  // failed builds has to persists across multiple scans so we know when to
  // backoff
  FailedBuildsMap failedBuilds;

  auto const fulfillOnExit = scopeGuard([this]() noexcept {
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
          << "BuildManager scan error: " << ex.what();
    }
  }
}

void BuildManager::scanAndBuild(std::stop_token const& stopToken,
                                FailedBuildsMap& failedBuilds) {
  // we use these ones to prune failedBuilds(for dropped indexes)
  std::unordered_set<std::uint64_t> seenObjectIds;
  // we use this one to prune waiters(for dropped indexes)
  std::unordered_set<IndexId::BaseType> seenIndexIds;
  // Unusable indexes with pending waiters that could not be built this scan.
  std::unordered_set<IndexId::BaseType> skippedWaiters;
  uint64_t unusableIndexesCount = 0;
  bool scanCompletedFully = false;

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
        LOG_TOPIC("e175b", DEBUG, Logger::ENGINES)
            << "[shard=" << coll->name() << ", index=" << idx->id().id()
            << "] Vector index build scan found index with trainingState="
            << trainingStateToString(vecIdx.trainingState());

        seenIndexIds.insert(vecIdx.id().id());
        switch (vecIdx.trainingState()) {
          case VectorIndexTrainingState::kReady:
            fulfillWaiters(vecIdx.id(), Result{});
            continue;
          case VectorIndexTrainingState::kTraining:
          case VectorIndexTrainingState::kIngesting:
            // kTraining or kIngesting: keep waiters pending until it finishes.
            LOG_TOPIC("e177b", DEBUG, Logger::ENGINES) << std::format(
                "[shard={}, index={}] Vector index build already in progress "
                "(trainingState={}); not starting a new build this scan.",
                vecIdx.collection().name(), vecIdx.id().id(),
                trainingStateToString(vecIdx.trainingState()));
            continue;
          case VectorIndexTrainingState::kUnusable:
            // continue below
            break;
        }

        ++unusableIndexesCount;
        seenObjectIds.insert(vecIdx.objectId());

        auto const* rcoll =
            dynamic_cast<RocksDBCollection*>(coll->getPhysical());
        TRI_ASSERT(rcoll != nullptr) << "This should never happen";
        if (rcoll == nullptr) {
          skippedWaiters.insert(vecIdx.id().id());
          LOG_TOPIC("e176b", ERR, Logger::ENGINES) << std::format(
              "[shard={}, index={}] physical collection is not a "
              "RocksDBCollection; skipping vector index build.",
              vecIdx.collection().name(), vecIdx.id().id());
          continue;
        }
        auto const numDocs = rcoll->meta().numberDocuments();
        if (numDocs < vecIdx.trainingThreshold()) {
          // we still cannot train
          skippedWaiters.insert(vecIdx.id().id());
          LOG_TOPIC("e174b", INFO, Logger::ENGINES) << std::format(
              "[shard={}, index={}] Vector index below training threshold: "
              "meta().numberDocuments()={}, threshold={}. Skipping this scan.",
              vecIdx.collection().name(), vecIdx.id().id(), numDocs,
              vecIdx.trainingThreshold());

          auto errorMessageBelowThreshold = std::format(
              "not enough training data for vector "
              "index, need at least {} documents "
              "but only {} present",
              vecIdx.trainingThreshold(), numDocs);
          // Republish only when the message (which embeds the doc count)
          // changed; otherwise every scan would needlessly mark the db dirty.
          if (vecIdx.trainingError() != errorMessageBelowThreshold) {
            vecIdx.setTrainingError(errorMessageBelowThreshold);
            reportIndexError(vocbase, *coll, vecIdx,
                             Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                                    errorMessageBelowThreshold});
          }
          continue;
        }

        // Check to build or not
        if (shouldSkipRetry(failedBuilds, vecIdx.objectId(), numDocs)) {
          skippedWaiters.insert(vecIdx.id().id());
          auto const& info = failedBuilds.at(vecIdx.objectId());
          auto const remaining =
              std::chrono::duration_cast<std::chrono::duration<double>>(
                  _retryBackoff -
                  (std::chrono::steady_clock::now() - info.failedAt));
          LOG_TOPIC("e172b", INFO, Logger::ENGINES)
              << "[shard=" << vecIdx.collection().name()
              << ", index=" << vecIdx.id().id()
              << "] Vector index build in retry backoff, skipping retry for "
              << remaining.count() << "s more.";
          continue;
        }

        buildIndex(vocbase, *coll, idx, numDocs, unusableIndexesCount,
                   stopToken, failedBuilds);
      }
    }
  });

  scanCompletedFully = !stopToken.stop_requested();
  _untrainedCount.store(unusableIndexesCount, std::memory_order_relaxed);

  // Prune failed build entries for indexes that no longer exist.
  std::erase_if(failedBuilds, [&](auto const& entry) {
    return !seenObjectIds.contains(entry.first);
  });

  // Unblock waiters for indexes that were skipped this scan.
  for (auto indexId : skippedWaiters) {
    fulfillWaiters(IndexId{indexId},
                   Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                          "vector index not ready"});
  }

  // Unblock waiters for dropped indexes. Only safe after a full scan.
  if (scanCompletedFully) {
    std::vector<IndexId::BaseType> orphaned;
    {
      std::lock_guard lock(_waitersMutex);
      for (auto const& [id, _] : _waiters) {
        if (!seenIndexIds.contains(id)) {
          orphaned.push_back(id);
        }
      }
    }
    for (auto id : orphaned) {
      fulfillWaiters(IndexId{id}, Result{TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                         "index was dropped"});
    }
  }
}

void BuildManager::buildIndex(TRI_vocbase_t const& vocbase,
                              LogicalCollection const& coll,
                              std::shared_ptr<Index> const& idx,
                              std::uint64_t numDocs,
                              std::uint64_t unusableIndexesCount,
                              std::stop_token const& stopToken,
                              FailedBuildsMap& failedBuilds) {
  auto& vecIdx = static_cast<RocksDBVectorIndex&>(*idx);

  LOG_TOPIC("e171b", INFO, Logger::ENGINES)
      << "[shard=" << vecIdx.collection().name()
      << ", index=" << vecIdx.id().id() << "] Training threshold reached ("
      << vecIdx.trainingThreshold()
      << " documents). Starting deferred training.";

  _trainingOngoingCount.fetch_add(1);
  TRI_ASSERT(_resourceMonitor.current() == 0);
  auto indexPtr = std::static_pointer_cast<RocksDBIndex>(idx);
  VectorIndexBuilder builder(vecIdx, _resourceMonitor);

  // Catch throws (e.g. ResourceUsageScope overflow) so failure handling
  // below runs uniformly for both Result-failed and thrown errors.
  auto const res = std::invoke([&]() -> Result {
    try {
      return builder.build(std::move(indexPtr), _trainingDuration,
                           _ingestionDuration, stopToken);
    } catch (basics::Exception const& e) {
      return Result{e.code(), e.message()};
    } catch (std::exception const& e) {
      return Result{TRI_ERROR_INTERNAL, e.what()};
    }
  });
  _trainingOngoingCount.fetch_sub(1);

  if (res.fail()) {
    auto errorMessage = std::string{res.errorMessage()};
    // Republish only when the reported error changed; otherwise a persistent
    // identical failure would needlessly mark the db dirty on every retry.
    bool const errorChanged = vecIdx.trainingError() != errorMessage;
    // Set the error before flipping state so a concurrent REST reader never
    // observes kUnusable with an empty error.
    vecIdx.setTrainingError(std::move(errorMessage));
    vecIdx.resetTrainingState();
    fulfillWaiters(vecIdx.id(), res);
    if (res.is(TRI_ERROR_RESOURCE_LIMIT)) {
      LOG_TOPIC("e165b", ERR, Logger::ENGINES)
          << "[index=" << vecIdx.id().id()
          << "] Vector build aborted: training reservoir exceeded the "
             "configured memory limit. Lower numberOfDocsPerCentroid in the "
             "index definition or raise the global memory limit. Details: "
          << res.errorMessage();
    } else {
      LOG_TOPIC("e164b", ERR, Logger::ENGINES)
          << "[index=" << vecIdx.id().id()
          << "] Vector build failed: " << res.errorMessage();
    }
    failedBuilds[vecIdx.objectId()] = {std::chrono::steady_clock::now(),
                                       numDocs};
    LOG_TOPIC("e173b", INFO, Logger::ENGINES)
        << "[shard=" << vecIdx.collection().name()
        << ", index=" << vecIdx.id().id() << "] Backing off retries for "
        << std::chrono::duration_cast<std::chrono::duration<double>>(
               _retryBackoff)
               .count()
        << "s (or until the document count changes).";

    if (errorChanged) {
      reportIndexError(vocbase, coll, vecIdx, res);
    }
    return;
  }

  fulfillWaiters(vecIdx.id(), Result{});
  clearIndexError(vocbase, coll, vecIdx);
  failedBuilds.erase(vecIdx.objectId());

  _untrainedCount.store(unusableIndexesCount > 0 ? unusableIndexesCount - 1 : 0,
                        std::memory_order_relaxed);
}

void BuildManager::markDatabaseDirty(std::string const& database) {
  if (ServerState::instance()->isDBServer()) {
    _maintenance.addDirty(database);
  }
}

void BuildManager::reportIndexError(TRI_vocbase_t const& vocbase,
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
  // Wake the maintenance loop so the buffered error reaches Current promptly.
  markDatabaseDirty(database);
}

void BuildManager::clearIndexError(TRI_vocbase_t const& vocbase,
                                   LogicalCollection const& coll,
                                   RocksDBVectorIndex const& vecIdx) {
  auto const& database = vocbase.name();
  auto const collection = std::to_string(coll.planId().id());
  auto const& shard = coll.name();
  auto const indexId = std::to_string(vecIdx.id().id());
  _maintenance.clearIndexError(database, collection, shard, indexId);
  // Wake the maintenance loop so the cleared state reaches Current promptly.
  markDatabaseDirty(database);
}

}  // namespace arangodb::vector
