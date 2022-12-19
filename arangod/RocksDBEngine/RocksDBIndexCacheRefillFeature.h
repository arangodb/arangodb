////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Result.h"
#include "Containers/FlatHashSet.h"
#include "RestServer/MetricsFeature.h"
#include "VocBase/Identifiers/IndexId.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <string>
#include <vector>

namespace arangodb {
class DatabaseFeature;
class LogicalCollection;
class RocksDBIndexCacheRefillThread;

class RocksDBIndexCacheRefillFeature final
    : public application_features::ApplicationFeature {
 public:
  explicit RocksDBIndexCacheRefillFeature(
      application_features::ApplicationServer& server);

  ~RocksDBIndexCacheRefillFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void beginShutdown() override;
  void start() override;
  void stop() override;

  // track the refill of the specified keys. returns true if the keys
  // were taken, false otherwise
  bool trackRefill(std::shared_ptr<LogicalCollection> const& collection,
                   IndexId iid, containers::FlatHashSet<std::string>& keys);

  // schedule the refill of the full index
  void scheduleFullIndexRefill(std::string const& database,
                               std::string const& collection, IndexId iid);

  // wait until the background thread has applied all operations
  void waitForCatchup();

  // maximum capacity for tracking per-key refills
  size_t maxCapacity() const noexcept;

  // auto-refill in-memory cache after every insert/update/replace operation
  bool autoRefill() const noexcept;

  // auto-refill in-memory cache also on followers
  bool autoRefillOnFollowers() const noexcept;

  // refill in foreground if background threads' queues are full
  bool refillInForegroundIfQueueFull() const noexcept;

  // auto-fill in-memory caches on startup
  bool fillOnStartup() const noexcept;

  void increaseTotalNumQueued(uint64_t value) noexcept;
  void increaseTotalNumDropped(uint64_t value) noexcept;
  void increaseTotalNumForeground(uint64_t value) noexcept;
  void increaseTotalNumLoaded(uint64_t value) noexcept;

 private:
  void stopThreads();

  // build the initial data in _indexFillTasks
  void buildStartupIndexRefillTasks();

  // post as many as possible index fill tasks to the scheduler.
  // note: this will only post up to at most _maxConcurrentIndexFillTasks
  // to the scheduler. the method may post index fill tasks to the scheduler
  // that will at their end call scheduleIndexRefillTasks() again, i.e.
  // the method can indirectly schedule itself.
  void scheduleIndexRefillTasks();

  // actually fill the specified index cache
  Result warmupIndex(std::string const& database, std::string const& collection,
                     IndexId iid);

  DatabaseFeature& _databaseFeature;

  // index refill threads used for auto-refilling after insert/update/replace
  // (not used for initial filling at startup)
  std::vector<std::unique_ptr<RocksDBIndexCacheRefillThread>>
      _backgroundThreads;

  // currently responsible background thread (for receiving new keys)
  std::atomic<uint64_t> _currentBackgroundThreadIdx;

  // maximum capacity of queue used for automatic refilling of in-memory index
  // caches after insert/update/replace (not used for initial filling at
  // startup)
  uint64_t _maxCapacity;

  // number of background threads available for refilling
  uint64_t _numBackgroundThreads;

  // maximum concurrent index fill tasks that we are allowed to run to fill
  // indexes during startup
  uint64_t _maxConcurrentIndexFillTasks;

  // whether or not in-memory cache values for indexes are automatically
  // refilled upon insert/update/replace
  bool _autoRefill;

  // whether or not in-memory cache values for indexes are automatically
  // refilled on followers
  bool _autoRefillOnFollowers;

  // move refilling to foreground if background threads' queues are full.
  bool _refillInForegroundIfQueueFull;

  // whether or not in-memory cache values for indexes are automatically
  // populated on server start
  bool _fillOnStartup;

  // total number of full index refills completed
  Counter& _totalFullIndexRefills;

  // total number of items ever queued
  Counter& _totalNumQueued;

  // total number of items ever dropped (because of queue full)
  Counter& _totalNumDropped;

  // total number of items processed in foreground (because of queue full)
  Counter& _totalNumForeground;

  // total number of items reloaded
  Counter& _totalNumLoaded;

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
}  // namespace arangodb
