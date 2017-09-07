////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_MANAGER_H
#define ARANGODB_CACHE_MANAGER_H

#include "Basics/Common.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Basics/SharedAtomic.h"
#include "Basics/SharedCounter.h"
#include "Basics/asio-helper.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionManager.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <stack>
#include <utility>

namespace arangodb {
namespace cache {

class Cache;           // forward declaration
class FreeMemoryTask;  // forward declaration
class MigrateTask;     // forward declaration
class Rebalancer;      // forward declaration

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
/// CacheManagerFeature::MANAGER --- use this unless you are very certain you
/// need a different instance.
////////////////////////////////////////////////////////////////////////////////
class Manager {
 protected:
  typedef std::function<bool(std::function<void()>)> PostFn;

 public:
  static const uint64_t minSize;
  typedef FrequencyBuffer<uint64_t> AccessStatBuffer;
  typedef FrequencyBuffer<uint8_t> FindStatBuffer;
  typedef std::vector<std::pair<std::shared_ptr<Cache>&, double>> PriorityList;
  typedef std::chrono::time_point<std::chrono::steady_clock> time_point;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize the manager with a scheduler post method and global
  /// usage limit.
  //////////////////////////////////////////////////////////////////////////////
  Manager(PostFn schedulerPost, uint64_t globalLimit,
          bool enableWindowedStats = true);
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
  std::shared_ptr<Cache> createCache(CacheType type,
                                     bool enableWindowedStats = false,
                                     uint64_t maxSize = UINT64_MAX);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destroy the given cache.
  //////////////////////////////////////////////////////////////////////////////
  void destroyCache(std::shared_ptr<Cache> cache);

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
  bool resize(uint64_t newGlobalLimit);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report the current global usage limit.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t globalLimit();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report the current amoutn of memory allocated to all caches.
  ///
  /// This serves as an upper bound on the current memory usage of all caches.
  /// The actual global usage is not recorded, as this would require significant
  /// additional synchronization between the caches and slow things down
  /// considerably.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t globalAllocation();

  std::pair<double, double> globalHitRates();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Open a new transaction.
  ///
  /// The transaction is considered read-only if it is guaranteed not to write
  /// to the backing store. A read-only transaction may, however, write to the
  /// cache.
  //////////////////////////////////////////////////////////////////////////////
  Transaction* beginTransaction(bool readOnly);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the end of a transaction. Deletes the passed Transaction.
  //////////////////////////////////////////////////////////////////////////////
  void endTransaction(Transaction* tx);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Post a function to the scheduler
  //////////////////////////////////////////////////////////////////////////////
  bool post(std::function<void()> fn);

 private:
  // use sizeof(uint64_t) + sizeof(std::shared_ptr<Cache>) + 64 for upper bound
  // on size of std::set<std::shared_ptr<Cache>> node -- should be valid for
  // most libraries
  static constexpr uint64_t cacheRecordOverhead =
      sizeof(std::shared_ptr<Cache>) + 64;
  // assume at most 16 slots in each stack -- TODO: check validity
  static constexpr uint64_t tableListsOverhead =
      32 * 16 * sizeof(std::shared_ptr<Cache>);
  static constexpr uint64_t triesFast = 100;
  static constexpr uint64_t triesSlow = 1000;

  // simple state variables
  basics::ReadWriteSpinLock<64> _lock;
  bool _shutdown;
  bool _shuttingDown;
  bool _resizing;
  bool _rebalancing;

  // structure to handle access frequency monitoring
  Manager::AccessStatBuffer _accessStats;

  // structures to handle hit rate monitoring
  bool _enableWindowedStats;
  std::unique_ptr<Manager::FindStatBuffer> _findStats;
  basics::SharedCounter<64> _findHits;
  basics::SharedCounter<64> _findMisses;

  // registry to keep track of registered caches
  std::map<uint64_t, std::shared_ptr<Cache>> _caches;
  uint64_t _nextCacheId;

  // actual tables to lease out
  std::stack<std::shared_ptr<Table>> _tables[32];

  // global statistics
  uint64_t _globalSoftLimit;
  uint64_t _globalHardLimit;
  uint64_t _globalHighwaterMark;
  uint64_t _fixedAllocation;
  uint64_t _spareTableAllocation;
  uint64_t _globalAllocation;

  // transaction management
  TransactionManager _transactions;

  // task management
  enum TaskEnvironment { none, rebalancing, resizing };
  PostFn _schedulerPost;
  uint64_t _resizeAttempt;
  basics::SharedAtomic<uint64_t> _outstandingTasks;
  basics::SharedAtomic<uint64_t> _rebalancingTasks;
  basics::SharedAtomic<uint64_t> _resizingTasks;
  Manager::time_point _rebalanceCompleted;

  // friend class tasks and caches to allow access
  friend class Cache;
  friend class FreeMemoryTask;
  friend struct Metadata;
  friend class MigrateTask;
  friend class PlainCache;
  friend class Rebalancer;
  friend class TransactionalCache;

 private:  // used by caches
  // register and unregister individual caches
  std::tuple<bool, Metadata, std::shared_ptr<Table>> registerCache(
      uint64_t fixedSize, uint64_t maxSize);
  void unregisterCache(uint64_t id);

  // allow individual caches to request changes to their allocations
  std::pair<bool, Manager::time_point> requestGrow(Cache* cache);
  std::pair<bool, Manager::time_point> requestMigrate(
      Cache* cache, uint32_t requestedLogSize);

  // stat reporting
  void reportAccess(uint64_t id);
  void reportHitStat(Stat stat);

 private:  // used internally and by tasks
  static constexpr double highwaterMultiplier = 0.8;
  static const uint64_t minCacheAllocation;
  static const std::chrono::milliseconds rebalancingGracePeriod;

  // check if shutdown or shutting down
  bool isOperational() const;
  // check if there is already a global process running
  bool globalProcessRunning() const;

  // coordinate state with task lifecycles
  void prepareTask(TaskEnvironment environment);
  void unprepareTask(TaskEnvironment environment);

  // periodically run to rebalance allocations globally
  int rebalance(bool onlyCalculate = false);

  // helpers for global resizing
  void shrinkOvergrownCaches(TaskEnvironment environment);
  void freeUnusedTables();
  bool adjustGlobalLimitsIfAllowed(uint64_t newGlobalLimit);

  // methods to adjust individual caches
  void resizeCache(TaskEnvironment environment, Cache* cache,
                   uint64_t newLimit);
  void migrateCache(TaskEnvironment environment, Cache* cache,
                    std::shared_ptr<Table>& table);
  std::shared_ptr<Table> leaseTable(uint32_t logSize);
  void reclaimTable(std::shared_ptr<Table> table, bool internal = false);

  // helpers for individual allocations
  bool increaseAllowed(uint64_t increase, bool privileged = false) const;

  // helper for lr-accessed heuristics
  std::shared_ptr<PriorityList> priorityList();

  // helper for wait times
  Manager::time_point futureTime(uint64_t millisecondsFromNow);
  bool pastRebalancingGracePeriod() const;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
