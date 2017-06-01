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

#ifndef ARANGODB_CACHE_CACHE_H
#define ARANGODB_CACHE_CACHE_H

#include "Basics/Common.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Manager.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/State.h"
#include "Cache/Table.h"

#include <stdint.h>
#include <list>
#include <memory>

namespace arangodb {
namespace cache {

class PlainCache;          // forward declaration
class TransactionalCache;  // forward declaration

////////////////////////////////////////////////////////////////////////////////
/// @brief The common structure of all caches managed by Manager.
///
/// Any pure virtual functions are documented in derived classes implementing
/// them.
////////////////////////////////////////////////////////////////////////////////
class Cache : public std::enable_shared_from_this<Cache> {
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief A dummy class to restrict constructor access.
  //////////////////////////////////////////////////////////////////////////////
  class ConstructionGuard {
   private:
    ConstructionGuard();
    friend class PlainCache;
    friend class TransactionalCache;
  };

 public:
  typedef FrequencyBuffer<uint8_t> StatBuffer;

  static const uint64_t minSize;
  static const uint64_t minLogSize;

 public:
  Cache(ConstructionGuard guard, Manager* manager, Metadata metadata,
        std::shared_ptr<Table> table, bool enableWindowedStats,
        std::function<Table::BucketClearer(Metadata*)> bucketClearer,
        size_t slotsPerBucket);
  virtual ~Cache() = default;

  // primary functionality; documented in derived classes
  virtual Finding find(void const* key, uint32_t keySize) = 0;
  virtual bool insert(CachedValue* value) = 0;
  virtual bool remove(void const* key, uint32_t keySize) = 0;
  virtual bool blacklist(void const* key, uint32_t keySize) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the total memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t size();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the limit on data memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t usageLimit();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the current data memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t usage();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Gives hint to attempt to preallocate space for an incoming load.
  ///
  /// The parameter specifies an expected number of elements to be inserted.
  /// This allows for migration to an appropriately-sized table.
  //////////////////////////////////////////////////////////////////////////////
  void sizeHint(uint64_t numElements);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the cache hit-rates.
  ///
  /// The first return value is the lifetime hit-rate for this cache. The second
  /// is the "windowed" hit-rate, that is the hit-rate when only considering the
  /// past several thousand find operations. If windowed stats are not enabled,
  /// this will be NaN.
  //////////////////////////////////////////////////////////////////////////////
  std::pair<double, double> hitRates();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of resizing.
  //////////////////////////////////////////////////////////////////////////////
  bool isResizing();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of migrating.
  //////////////////////////////////////////////////////////////////////////////
  bool isMigrating();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache has begin the process of shutting down.
  //////////////////////////////////////////////////////////////////////////////
  bool isShutdown();

 protected:
  static constexpr int64_t triesFast = 10000;
  static constexpr int64_t triesSlow = 100000;
  static constexpr int64_t triesGuarantee = -1;

 protected:
  State _state;

  static uint64_t _findStatsCapacity;
  bool _enableWindowedStats;
  std::unique_ptr<StatBuffer> _findStats;
  std::atomic<uint64_t> _findHits;
  std::atomic<uint64_t> _findMisses;

  // allow communication with manager
  Manager* _manager;
  Metadata _metadata;

  // manage the actual table
  std::shared_ptr<Table> _table;
  Table::BucketClearer _bucketClearer;
  size_t _slotsPerBucket;

  // manage eviction rate
  std::atomic<uint64_t> _insertsTotal;
  std::atomic<uint64_t> _insertEvictions;
  static constexpr uint64_t _evictionMask = 1023; // check every 1024 insertions
  static constexpr uint64_t _evictionThreshold = 10;  // if more than 10
                                                      // evictions in past 1024
                                                      // inserts, migrate

  // keep track of number of open operations to allow clean shutdown
  std::atomic<uint32_t> _openOperations;

  // times to wait until requesting is allowed again
  Manager::time_point _migrateRequestTime;
  Manager::time_point _resizeRequestTime;

  // friend class manager and tasks
  friend class FreeMemoryTask;
  friend class Manager;
  friend class MigrateTask;

 protected:
  // shutdown cache and let its memory be reclaimed
  static void destroy(std::shared_ptr<Cache> cache);

  bool isOperational() const;
  void startOperation();
  void endOperation();

  bool isMigratingLocked() const;
  void requestGrow();
  void requestMigrate(uint32_t requestedLogSize = 0);

  static void freeValue(CachedValue* value);
  bool reclaimMemory(uint64_t size);

  uint32_t hashKey(void const* key, uint32_t keySize) const;
  void recordStat(Stat stat);

  bool reportInsert(bool hadEviction);

  // management
  Metadata* metadata();
  std::shared_ptr<Table> table();
  void beginShutdown();
  void shutdown();
  bool canResize();
  bool canMigrate();
  bool freeMemory();
  bool migrate(std::shared_ptr<Table> newTable);

  virtual uint64_t freeMemoryFrom(uint32_t hash) = 0;
  virtual void migrateBucket(void* sourcePtr,
                             std::unique_ptr<Table::Subtable> targets,
                             std::shared_ptr<Table> newTable) = 0;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
