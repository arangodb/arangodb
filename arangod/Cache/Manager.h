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
#include "Basics/asio-helper.h"
#include "Cache/CachedValue.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/State.h"
#include "Cache/TransactionWindow.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <list>
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
 public:
  static uint64_t MINIMUM_SIZE;
  typedef FrequencyBuffer<std::shared_ptr<Cache>> StatBuffer;
  typedef std::vector<std::shared_ptr<Cache>> PriorityList;
  typedef std::chrono::time_point<std::chrono::steady_clock> time_point;
  typedef std::list<Metadata>::iterator MetadataItr;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize the manager with an io_service and global usage limit.
  //////////////////////////////////////////////////////////////////////////////
  Manager(boost::asio::io_service* ioService, uint64_t globalLimit);
  ~Manager();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Enum to specify which type of cache to create.
  //////////////////////////////////////////////////////////////////////////////
  enum CacheType { Plain, Transactional };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Creates an individual cache.
  ///
  /// The type must be specified. It is possible that the cache cannot be
  /// created (e.g. in situations of extreme memory pressure), in which case the
  /// returned pointer will be nullptr. If there isn't enough memory to create a
  /// cache with the requested limit, the actual limit may be smaller. If the
  /// third parameter is true, the cache will be allowed to grow if it becomes
  /// full and memory is available globally; otherwise the limit given to it by
  /// the manager is a hard upper limit which may only be adjusted downward.
  /// This parameter is true by default. It should likely only be set to be
  /// false for low-priority, short-lived caches.
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<Cache> createCache(Manager::CacheType type,
                                     uint64_t requestedLimit,
                                     bool allowGrowth = true);

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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the beginning of a transaction.
  //////////////////////////////////////////////////////////////////////////////
  void startTransaction();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the end of a transaction.
  //////////////////////////////////////////////////////////////////////////////
  void endTransaction();

 private:
  // simple state variable for locking and other purposes
  State _state;

  // structure to handle access frequency monitoring
  Manager::StatBuffer _accessStats;
  std::atomic<uint64_t> _accessCounter;

  // list of metadata objects to keep track of all the registered caches
  std::list<Metadata> _caches;

  // actual tables to lease out
  std::stack<uint8_t*> _tables[32];

  // global statistics
  uint64_t _globalSoftLimit;
  uint64_t _globalHardLimit;
  uint64_t _globalAllocation;

  // transaction management
  TransactionWindow _transactions;

  // task management
  enum TaskEnvironment { none, rebalancing, resizing };
  boost::asio::io_service* _ioService;
  uint64_t _resizeAttempt;
  std::atomic<uint64_t> _outstandingTasks;
  std::atomic<uint64_t> _rebalancingTasks;
  std::atomic<uint64_t> _resizingTasks;

  // friend class tasks and caches to allow access
  friend class Cache;
  friend class FreeMemoryTask;
  friend class MigrateTask;
  friend class PlainCache;
  friend class Rebalancer;
  friend class TransactionalCache;

 private:  // used by caches
  // register and unregister individual caches
  Manager::MetadataItr registerCache(Cache* cache, uint64_t requestedLimit,
                                     std::function<void(Cache*)> deleter);
  void unregisterCache(Manager::MetadataItr& metadata);

  // allow individual caches to request changes to their allocations
  std::pair<bool, Manager::time_point> requestResize(
      Manager::MetadataItr& metadata, uint64_t requestedLimit);
  std::pair<bool, Manager::time_point> requestMigrate(
      Manager::MetadataItr& metadata, uint32_t requestedLogSize);

  // method for lr-accessed heuristics
  void reportAccess(std::shared_ptr<Cache> cache);

 private:  // used internally and by tasks
  // check if shutdown or shutting down
  bool isOperational() const;
  // check if there is already a global process running
  bool globalProcessRunning() const;

  // expose io_service
  boost::asio::io_service* ioService();

  // coordinate state with task lifecycles
  void prepareTask(TaskEnvironment environment);
  void unprepareTask(TaskEnvironment environment);

  // periodically run to rebalance allocations globally
  bool rebalance();

  // helpers for global resizing
  void internalResize(uint64_t newGlobalLimit, bool firstAttempt);
  uint64_t resizeAllCaches(TaskEnvironment environment,
                           std::shared_ptr<PriorityList> cacheList,
                           bool noTasks, bool aggressive, uint64_t goal);
  uint64_t migrateAllCaches(TaskEnvironment environment,
                            std::shared_ptr<PriorityList> cacheList,
                            uint64_t goal);
  void freeUnusedTables();
  bool adjustGlobalLimitsIfAllowed(uint64_t newGlobalLimit);

  // methods to adjust individual caches
  void resizeCache(TaskEnvironment environment, Manager::MetadataItr& metadata,
                   uint64_t newLimit);
  void migrateCache(TaskEnvironment environment, Manager::MetadataItr& metadata,
                    uint32_t logSize);
  void leaseTable(Manager::MetadataItr& metadata, uint32_t logSize);
  void reclaimTables(Manager::MetadataItr& metadata,
                     bool auxiliaryOnly = false);

  // helpers for individual allocations
  bool increaseAllowed(uint64_t increase) const;
  uint64_t tableSize(uint32_t logSize) const;

  // helper for lr-accessed heuristics
  std::shared_ptr<PriorityList> priorityList();

  // helper for wait times
  Manager::time_point futureTime(uint64_t secondsFromNow);
};

};  // end namespace cache
};  // end namespace arangodb

#endif
