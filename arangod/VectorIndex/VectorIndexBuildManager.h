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
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Futures/Promise.h"
#include "Metrics/Fwd.h"
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
    std::int64_t documentCount;
  };

  using FailedBuildsMap = std::unordered_map<std::uint64_t, FailedBuildInfo>;

  static bool shouldSkipRetry(FailedBuildsMap const& failedBuilds,
                              std::uint64_t objectId,
                              std::int64_t currentDocCount);

  void run(std::stop_token stopToken);

  void scanAndBuild(std::stop_token const& stopToken,
                    FailedBuildsMap& failedBuilds);

  void fulfillWaiters(IndexId indexId, Result const& result);

  void fulfillAllWaiters(Result const& result);

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

  metrics::Gauge<uint64_t>& _untrainedCount;
  metrics::Gauge<uint64_t>& _trainingOngoingCount;
  metrics::Histogram<metrics::LogScale<double>>& _trainingDuration;
  metrics::Histogram<metrics::LogScale<double>>& _ingestionDuration;

  std::mutex _waitersMutex;
  std::unordered_map<IndexId::BaseType, std::vector<futures::Promise<Result>>>
      _waiters;
};

}  // namespace arangodb::vector
