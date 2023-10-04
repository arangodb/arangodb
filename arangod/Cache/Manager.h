////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ReadWriteSpinLock.h"
#include "Basics/SharedCounter.h"
#include "Basics/SpinLocker.h"
#include "Cache/CacheOptionsProvider.h"
#include "Cache/Common.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionManager.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stack>
#include <utility>

namespace arangodb {
class SharedPRNGFeature;
}

namespace arangodb::cache {

class Cache;
class FreeMemoryTask;
class MigrateTask;
class Rebalancer;

////////////////////////////////////////////////////////////////////////////////
/// @brief Coordinates a system of caches all sharing a single memory pool.
///
/// Allows clients to create and destroy both transactional and
/// non-transactional caches with individual usage limits, but all subject to a
/// combined global limit. Re-uses memory from old, destroyed caches if possible
/// when allocating new ones to allow fast creation and destruction of
/// short-lived caches.
///
/// The global limit may be adjusted, and compliance may be achieved through
/// asynchronous background tasks. The manager periodically rebalances the
/// allocations across the pool of caches to allow more frequently used ones to
/// have more space.
///
/// There should be a single Manager instance exposed via
/// CacheManagerFeature::manager() --- use this unless you are very certain you
/// need a different instance.
////////////////////////////////////////////////////////////////////////////////
class Manager {
 protected:
  using PostFn = std::function<bool(std::function<void()>)>;

 public:
  struct MemoryStats {
    std::uint64_t globalLimit = 0;
    std::uint64_t globalAllocation = 0;
    std::uint64_t peakGlobalAllocation = 0;
    std::uint64_t spareAllocation = 0;
    std::uint64_t peakSpareAllocation = 0;
    std::uint64_t activeTables = 0;
    std::uint64_t spareTables = 0;
    std::uint64_t migrateTasks = 0;
    std::uint64_t freeMemoryTasks = 0;
    std::uint64_t migrateTasksDuration = 0;     // total, micros
    std::uint64_t freeMemoryTasksDuration = 0;  // total, micros
  };

  static constexpr std::size_t kFindStatsCapacity = 8192;
  static constexpr std::uint64_t kMinSize = 1024 * 1024;
  static constexpr std::uint64_t kMaxSpareTablesTotal = 16;
  // use sizeof(uint64_t) + sizeof(std::shared_ptr<Cache>) + 64 for upper bound
  // on size of std::set<std::shared_ptr<Cache>> node -- should be valid for
  // most libraries
  static constexpr std::uint64_t kCacheRecordOverhead =
      sizeof(std::shared_ptr<Cache>) + 64;

  using AccessStatBuffer = FrequencyBuffer<std::uint64_t>;
  using FindStatBuffer = FrequencyBuffer<std::uint8_t>;
  using PriorityList = std::vector<std::pair<std::shared_ptr<Cache>&, double>>;
  using time_point = std::chrono::time_point<std::chrono::steady_clock>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize the manager with a scheduler post method and global
  /// usage limit.
  //////////////////////////////////////////////////////////////////////////////
  Manager(SharedPRNGFeature& sharedPRNG, PostFn schedulerPost,
          CacheOptions const& options);

  Manager(Manager const&) = delete;
  Manager& operator=(Manager const&) = delete;

  ~Manager();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Creates an individual cache.
  ///
  /// The type must be specified. It is possible that the cache cannot be
  /// created (e.g. in situations of extreme memory pressure), in which case the
  /// returned pointer will be nullptr. If the second parameter is true, then
  /// windowed stats will be collected. This incurs some memory and overhead and
  /// but only a slight performance hit. The windowed stats refer to only a
  /// recent window in time, rather than over the full lifetime of the cache.
  /// The third parameter controls the maximum size of the cache over its
  /// lifetime. It should likely only be set to a non-default value for
  /// infrequently accessed or short-lived caches.
  //////////////////////////////////////////////////////////////////////////////
  template<typename Hasher>
  std::shared_ptr<Cache> createCache(
      CacheType type, bool enableWindowedStats = false,
      std::uint64_t maxSize = std::numeric_limits<std::uint64_t>::max());

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destroy the given cache.
  //////////////////////////////////////////////////////////////////////////////
  static void destroyCache(std::shared_ptr<Cache>&& cache);

  void adjustGlobalAllocation(std::int64_t value) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Prepare for shutdown.
  //////////////////////////////////////////////////////////////////////////////
  void beginShutdown();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Actually shutdown the manager and all caches.
  //////////////////////////////////////////////////////////////////////////////
  void shutdown();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Change the global usage limit.
  //////////////////////////////////////////////////////////////////////////////
  bool resize(std::uint64_t newGlobalLimit);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report the current global usage limit.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t globalLimit() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report the current amount of memory allocated to all caches.
  ///
  /// This serves as an upper bound on the current memory usage of all caches.
  /// The actual global usage is not recorded, as this would require significant
  /// additional synchronization between the caches and slow things down
  /// considerably.
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] std::uint64_t globalAllocation() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return some statistics about available caches
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] std::optional<MemoryStats> memoryStats(
      std::uint64_t maxTries) const noexcept;

  [[nodiscard]] std::pair<double, double> globalHitRates();

  double idealLowerFillRatio() const noexcept;
  double idealUpperFillRatio() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Open a new transaction.
  ///
  /// The transaction is considered read-only if it is guaranteed not to write
  /// to the backing store. A read-only transaction may, however, write to the
  /// cache.
  //////////////////////////////////////////////////////////////////////////////
  void beginTransaction(Transaction& tx, bool readOnly) {
    return _transactions.begin(tx, readOnly);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the end of a transaction. Deletes the passed Transaction.
  //////////////////////////////////////////////////////////////////////////////
  void endTransaction(Transaction& tx) noexcept { _transactions.end(tx); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Post a function to the scheduler
  //////////////////////////////////////////////////////////////////////////////
  bool post(std::function<void()> fn);

  SharedPRNGFeature& sharedPRNG() const noexcept { return _sharedPRNG; }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  void freeUnusedTablesForTesting();
#endif

  // track duration of migrate task, in ms
  void trackMigrateTaskDuration(std::uint64_t duration) noexcept;
  // track duration of free memory task, in ms
  void trackFreeMemoryTaskDuration(std::uint64_t duration) noexcept;

 private:
  // assume at most 16 slots in each stack -- TODO: check validity
  static constexpr std::uint64_t kTableListsOverhead =
      32 * 16 * sizeof(std::shared_ptr<Cache>);
  static constexpr std::uint64_t triesFast = 100;
  static constexpr std::uint64_t triesSlow = 1000;

  SharedPRNGFeature& _sharedPRNG;
  CacheOptions const _options;

  // simple state variables
  mutable basics::ReadWriteSpinLock _lock;
  bool _shutdown;
  bool _shuttingDown;
  bool _resizing;
  bool _rebalancing;

  // structure to handle access frequency monitoring
  AccessStatBuffer _accessStats;

  // structures to handle hit rate monitoring
  std::unique_ptr<FindStatBuffer> _findStats;
  basics::SharedCounter<64> _findHits;
  basics::SharedCounter<64> _findMisses;

  // registry to keep track of registered caches
  std::map<std::uint64_t, std::shared_ptr<Cache>> _caches;
  std::uint64_t _nextCacheId;

  // actual tables to lease out
  std::array<
      std::stack<std::shared_ptr<Table>, std::vector<std::shared_ptr<Table>>>,
      32>
      _tables;

  // global statistics
  std::uint64_t _globalSoftLimit;
  std::uint64_t _globalHardLimit;
  std::uint64_t _globalHighwaterMark;
  std::uint64_t _fixedAllocation;
  std::uint64_t _spareTableAllocation;
  std::uint64_t _peakSpareTableAllocation;
  std::uint64_t _globalAllocation;
  std::uint64_t _peakGlobalAllocation;
  std::uint64_t _activeTables;
  std::uint64_t _spareTables;
  std::uint64_t _migrateTasks;
  std::uint64_t _freeMemoryTasks;
  std::uint64_t _migrateTasksDuration;     // total, micros
  std::uint64_t _freeMemoryTasksDuration;  // total, micros

  // transaction management
  TransactionManager _transactions;

  // task management
  enum TaskEnvironment { kNone, kRebalancing, kResizing };
  PostFn _schedulerPost;
  std::atomic<std::uint64_t> _outstandingTasks;
  std::atomic<std::uint64_t> _rebalancingTasks;
  std::atomic<std::uint64_t> _resizingTasks;
  time_point _rebalanceCompleted;

  // friend class tasks and caches to allow access
  friend class Cache;
  friend class FreeMemoryTask;
  friend struct Metadata;
  friend class MigrateTask;
  template<typename Hasher>
  friend class PlainCache;
  friend class Rebalancer;
  template<typename Hasher>
  friend class TransactionalCache;

  // used by caches

  // register and unregister individual caches
  std::tuple<bool, Metadata, std::shared_ptr<Table>> createTable(
      std::uint64_t fixedSize, std::uint64_t maxSize);
  void unregisterCache(std::uint64_t id);

  // allow individual caches to request changes to their allocations
  std::pair<bool, time_point> requestGrow(Cache* cache);
  std::pair<bool, time_point> requestMigrate(Cache* cache,
                                             std::uint32_t requestedLogSize);

  // stat reporting
  void reportAccess(std::uint64_t id) noexcept;
  void reportHit() noexcept;
  void reportMiss() noexcept;

  // ratio of caches for which a shrinking attempt will be made if we
  // reach the cache's high water mark (memory limit plus safety buffer)
  static constexpr double kCachesToShrinkRatio = 0.20;
  static constexpr std::chrono::milliseconds rebalancingGracePeriod{10};
  static const std::uint64_t minCacheAllocation;

  // check if shutdown or shutting down
  [[nodiscard]] bool isOperational() const noexcept;
  // check if there is already a global process running
  [[nodiscard]] bool globalProcessRunning() const noexcept;

  // coordinate state with task lifecycles
  void prepareTask(TaskEnvironment environment);
  void unprepareTask(TaskEnvironment environment, bool internal) noexcept;

  // periodically run to rebalance allocations globally
  ErrorCode rebalance(bool onlyCalculate = false);

  // helpers for global resizing
  void shrinkOvergrownCaches(TaskEnvironment environment,
                             PriorityList const& cacheList);
  void freeUnusedTables();
  bool adjustGlobalLimitsIfAllowed(std::uint64_t newGlobalLimit);

  // methods to adjust individual caches
  void resizeCache(TaskEnvironment environment, basics::SpinLocker&& metaGuard,
                   Cache* cache, std::uint64_t newLimit, bool allowShrinking);
  void migrateCache(TaskEnvironment environment, basics::SpinLocker&& metaGuard,
                    Cache* cache, std::shared_ptr<Table> table);
  std::shared_ptr<Table> leaseTable(std::uint32_t logSize);
  void reclaimTable(std::shared_ptr<Table>&& table, bool internal);

  // helpers for individual allocations
  [[nodiscard]] bool increaseAllowed(uint64_t increase,
                                     bool privileged = false) const noexcept;

  // helper for lr-accessed heuristics
  PriorityList priorityList();

  // helper for wait times
  [[nodiscard]] static Manager::time_point futureTime(
      std::uint64_t millisecondsFromNow);
  [[nodiscard]] bool pastRebalancingGracePeriod() const;
};

}  // end namespace arangodb::cache
