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

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Basics/ResourceUsage.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Futures/Promise.h"
#include "Metrics/Fwd.h"
#include "VectorIndex/AutoTuner.h"
#include "VocBase/Identifiers/IndexId.h"

struct TRI_vocbase_t;

namespace arangodb {
class DatabaseFeature;
class LogicalCollection;
class MaintenanceFeature;
class RocksDBVectorIndex;
class Scheduler;
}  // namespace arangodb

namespace arangodb::vector {

/// Single background thread that periodically scans for untrained vector
/// indexes and builds them one at a time. The same thread scans and builds.
class VectorIndexBuildManager {
 public:
  // The persisted operating-point table on success.
  using AutoTuneResult = ResultT<OperatingPointTable>;

  static constexpr auto kScanInterval = std::chrono::seconds(5);
  static constexpr auto kSleepGranularity = std::chrono::seconds(1);

  explicit VectorIndexBuildManager(DatabaseFeature& dbFeature,
                                   MaintenanceFeature& maintenance,
                                   metrics::IRegistry& metricsRegistry,
                                   Scheduler& scheduler);

  void start();
  void beginShutdown();
  void stop();

  // Register a waiter for a specific index. The scan loop wakes up
  // immediately when waiters are pending. The returned future is fulfilled
  // when the index reaches the ready state or the build fails.
  futures::Future<Result> waitForIndexReady(IndexId indexId);

  // Queue an on-demand autotune for an already-built index. The request runs
  // on this manager's single build thread, so it never overlaps a build (or
  // another autotune). The future resolves with the operating-point table.
  futures::Future<AutoTuneResult> requestAutoTune(
      std::shared_ptr<LogicalCollection> collection, IndexId indexId,
      AutotuneParams params);

 private:
  static constexpr auto kRetryBackoff = std::chrono::minutes(10);

  struct AutoTuneRequest {
    std::shared_ptr<LogicalCollection> collection;
    IndexId indexId;
    AutotuneParams params;
    futures::Promise<AutoTuneResult> promise;
  };

  struct FailedBuildInfo {
    std::chrono::steady_clock::time_point failedAt;
    std::uint64_t documentCount;
  };

  using FailedBuildsMap = std::unordered_map<std::uint64_t, FailedBuildInfo>;

  static bool shouldSkipRetry(FailedBuildsMap const& failedBuilds,
                              std::uint64_t objectId,
                              std::uint64_t currentDocCount);

  void run(std::stop_token stopToken);

  void scanAndBuild(std::stop_token const& stopToken,
                    FailedBuildsMap& failedBuilds);

  void fulfillWaiters(IndexId indexId, Result const& result);

  void fulfillAllWaiters(Result const& result);

  // Drain and run all queued autotune requests on this thread (so they never
  // overlap a build). Each request's promise is fulfilled with its outcome.
  void processAutoTuneRequests(std::stop_token const& stopToken);

  AutoTuneResult runAutoTuneRequest(AutoTuneRequest const& request,
                                    std::stop_token const& stopToken);

  void fulfillAutoTune(futures::Promise<AutoTuneResult> promise,
                       AutoTuneResult result);

  void reportIndexError(TRI_vocbase_t const& vocbase,
                        LogicalCollection const& coll,
                        RocksDBVectorIndex const& vecIdx, Result const& error);

  void clearIndexError(TRI_vocbase_t const& vocbase,
                       LogicalCollection const& coll,
                       RocksDBVectorIndex const& vecIdx);

  DatabaseFeature& _dbFeature;
  MaintenanceFeature& _maintenance;
  Scheduler& _scheduler;
  std::jthread _thread;

  ResourceMonitor _resourceMonitor;

  metrics::Gauge<uint64_t>& _untrainedCount;
  metrics::Gauge<uint64_t>& _trainingOngoingCount;
  metrics::Histogram<metrics::LogScale<double>>& _trainingDuration;
  metrics::Histogram<metrics::LogScale<double>>& _ingestionDuration;

  std::mutex _waitersMutex;
  std::unordered_map<IndexId::BaseType, std::vector<futures::Promise<Result>>>
      _waiters;

  std::mutex _autoTuneMutex;
  std::vector<AutoTuneRequest> _autoTuneQueue;
};

}  // namespace arangodb::vector
