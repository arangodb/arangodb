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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Result.h"
#include "Metrics/Fwd.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillThread.h"
#include "VocBase/Identifiers/IndexId.h"

#include <memory>
#include <mutex>
#include <string_view>
#include <string>
#include <vector>

namespace arangodb {
class RocksDBEngine;
class BootstrapFeature;

class ClusterFeature;
class DatabaseFeature;
class LogicalCollection;
class RocksDBIndexCacheRefillThread;

namespace metrics {
class MetricsFeature;
}

class RocksDBIndexCacheRefillFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "RocksDBIndexCacheRefill";
  }

  template<typename Server>
  explicit RocksDBIndexCacheRefillFeature(
      Server& server, DatabaseFeature& databaseFeature,
      ClusterFeature* clusterFeature, metrics::MetricsFeature& metricsFeature);

  ~RocksDBIndexCacheRefillFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void beginShutdown() override;
  void start() override;
  void stop() override;

  // track the refill of the specified keys
  void trackRefill(std::shared_ptr<LogicalCollection> const& collection,
                   IndexId iid, std::vector<std::string> keys);

  // schedule the refill of the full index
  void scheduleFullIndexRefill(std::string const& database,
                               std::string const& collection, IndexId iid);

  // wait until the background thread has applied all operations
  void waitForCatchup();

  // maximum capacity for tracking per-key refills
  size_t maxCapacity() const noexcept;

  // auto-refill in-memory cache after every insert/update/replace operation
  bool autoRefill() const noexcept;

  // auto-fill in-memory caches on startup
  bool fillOnStartup() const noexcept;

  // auto-refill in-memory cache also on followers
  bool autoRefillOnFollowers() const noexcept;

 private:
  void stopThread();

  // build the initial data in _indexFillTasks
  void buildStartupIndexRefillTasks();

  // post as many as possible index fill tasks to the scheduler.
  // note: this will only post up to at most _maxConcurrentIndexFillTasks
  // to the scheduler. the method may post index fill tasks to the scheduler
  // that will at their end call scheduleIndexRefillTasks() again, i.e.
  // the method can indirectly schedule itself.
  void scheduleIndexRefillTasks();

  static metrics::Counter& addTotalFullIndexRefills(
      metrics::MetricsFeature& metrics);

  static size_t defaultConcurrentIndexFillTasks();

  // actually fill the specified index cache
  Result warmupIndex(std::string const& database, std::string const& collection,
                     IndexId iid);

  DatabaseFeature& _databaseFeature;
  ClusterFeature* _clusterFeature{};
  metrics::MetricsFeature& _metricsFeature;

  // index refill thread used for auto-refilling after insert/update/replace
  // (not used for initial filling at startup)
  std::unique_ptr<RocksDBIndexCacheRefillThread> _refillThread;

  // maximum capacity of queue used for automatic refilling of in-memory index
  // caches after insert/update/replace (not used for initial filling at
  // startup)
  size_t _maxCapacity;

  // maximum concurrent index fill tasks that we are allowed to run to fill
  // indexes during startup
  size_t _maxConcurrentIndexFillTasks;

  // whether or not in-memory cache values for indexes are automatically
  // refilled upon insert/update/replace
  bool _autoRefill;

  // whether or not in-memory cache values for indexes are automatically
  // populated on server start
  bool _fillOnStartup;

  // whether or not in-memory cache values for indexes are automatically
  // refilled on followers
  bool _autoRefillOnFollowers;

  // total number of full index refills completed
  metrics::Counter& _totalFullIndexRefills;

  // protects _indexFillTasks and _currentlyRunningIndexFillTasks
  std::mutex _indexFillTasksMutex;

  struct IndexFillTask {
    std::string database;
    std::string collection;
    IndexId iid;
  };
  std::vector<IndexFillTask> _indexFillTasks;

  size_t _currentlyRunningIndexFillTasks;
};

template<typename Server>
RocksDBIndexCacheRefillFeature::RocksDBIndexCacheRefillFeature(
    Server& server, DatabaseFeature& databaseFeature,
    ClusterFeature* clusterFeature, metrics::MetricsFeature& metricsFeature)
    : ApplicationFeature{server, *this},
      _databaseFeature(databaseFeature),
      _clusterFeature(clusterFeature),
      _metricsFeature(metricsFeature),
      _maxCapacity(128 * 1024),
      _maxConcurrentIndexFillTasks(defaultConcurrentIndexFillTasks()),
      _autoRefill(false),
      _fillOnStartup(false),
      _autoRefillOnFollowers(true),
      _totalFullIndexRefills(addTotalFullIndexRefills(metricsFeature)),
      _currentlyRunningIndexFillTasks(0) {
  setOptional(true);

  // we want to be late in the startup sequence
  startsAfter<BootstrapFeature, Server>();
  startsAfter<DatabaseFeature, Server>();
  startsAfter<RocksDBEngine, Server>();

  // default value must be at least 1, as the minimum allowed value is also 1.
  TRI_ASSERT(_maxConcurrentIndexFillTasks >= 1);
}

}  // namespace arangodb
