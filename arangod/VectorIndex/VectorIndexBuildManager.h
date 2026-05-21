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
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Basics/ResourceUsage.h"
#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Futures/Promise.h"
#include "Metrics/Fwd.h"
#include "VocBase/Identifiers/IndexId.h"

struct TRI_vocbase_t;

namespace arangodb {
class DatabaseFeature;
class Index;
class LogicalCollection;
class MaintenanceFeature;
class RocksDBCollection;
class RocksDBVectorIndex;
class Scheduler;
}  // namespace arangodb

namespace arangodb::vector {

/// Single background thread that periodically scans for vector indexes
/// in the kUnusable state and trains them one at a time. Index
/// replacements register a shadow index in the same kUnusable state, so
/// the same scan picks them up alongside fresh builds.
class VectorIndexBuildManager {
 public:
  static constexpr auto kScanInterval = std::chrono::seconds(5);
  static constexpr auto kSleepGranularity = std::chrono::seconds(1);

  explicit VectorIndexBuildManager(DatabaseFeature& dbFeature,
                                   MaintenanceFeature& maintenance,
                                   metrics::MetricsFeature& metrics,
                                   Scheduler& scheduler);

  void start();
  void beginShutdown();
  void stop();

  // Register a waiter for a specific index. The scan loop wakes up
  // immediately when waiters are pending. The returned future is fulfilled
  // when the index reaches the ready state or the build fails.
  futures::Future<Result> waitForIndexReady(IndexId indexId);

 private:
  static constexpr auto kRetryBackoff = std::chrono::minutes(10);

  struct FailedBuildInfo {
    std::chrono::steady_clock::time_point failedAt;
    std::uint64_t documentCount;
  };

  using FailedBuildsMap = std::unordered_map<std::uint64_t, FailedBuildInfo>;
  using BuildWaiterMap =
      std::unordered_map<IndexId::BaseType,
                         std::vector<futures::Promise<Result>>>;

  // Scratch state populated during a single scan pass and consumed by
  // the post-scan cleanup in scanAndProcess.
  struct ScanState {
    std::unordered_set<std::uint64_t> seenObjectIds;
    std::unordered_set<IndexId::BaseType> seenIndexIds;
    std::unordered_set<IndexId::BaseType> skippedWaiters;
    std::uint64_t unusableIndexesCount = 0;
  };

  // Per-index contribution returned by processVectorIndex. The scan
  // loop merges these into ScanState without the helper needing to see
  // accumulated state from other indexes.
  struct IndexScanResult {
    IndexId indexId;
    // Set when the index was in kUnusable state during this scan pass;
    // value is its objectId.
    std::optional<std::uint64_t> unusableObjectId;
    // True when the index was unusable but no build was started this
    // pass (below threshold or in retry backoff).
    bool skippedWaiter = false;
    // True when an unusable index transitioned to kReady during this
    // pass — the caller publishes the decremented metric.
    bool buildCompleted = false;
  };

  static bool shouldSkipRetry(FailedBuildsMap const& failedBuilds,
                              std::uint64_t objectId,
                              std::uint64_t currentDocCount);

  void run(std::stop_token stopToken);

  void scanAndProcess(std::stop_token const& stopToken,
                      FailedBuildsMap& failedBuilds);

  // Per-index work inside the scan: resolves build waiters and kicks off
  // deferred builds for kUnusable indexes that have reached the training
  // threshold.
  IndexScanResult processVectorIndex(FailedBuildsMap& failedBuilds,
                                     TRI_vocbase_t& vocbase,
                                     LogicalCollection& coll,
                                     std::shared_ptr<Index> const& idx,
                                     std::stop_token const& stopToken);

  // Resolve all pending build waiters registered for `indexId`.
  void fulfillBuildWaiters(IndexId indexId, Result const& result);

  // Drain all build waiters on shutdown.
  void fulfillAllWaitersOnShutdown(Result const& result);

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

  // Mutex guarding the build-waiter map below.
  std::mutex _mutex;
  BuildWaiterMap _buildWaiters;
};

}  // namespace arangodb::vector
