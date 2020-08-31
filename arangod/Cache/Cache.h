////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_CACHE_CACHE_H
#define ARANGODB_CACHE_CACHE_H

#include "Basics/ReadWriteSpinLock.h"
#include "Basics/Result.h"
#include "Basics/SharedCounter.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Manager.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"

#include <cstdint>
#include <limits>
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
    ConstructionGuard() = default;
    friend class PlainCache;
    friend class TransactionalCache;
  };

 public:
  typedef FrequencyBuffer<uint8_t> StatBuffer;

  static const std::uint64_t minSize;
  static const std::uint64_t minLogSize;

  static constexpr std::uint64_t triesGuarantee =
      std::numeric_limits<std::uint64_t>::max();

 public:
  Cache(ConstructionGuard guard, Manager* manager, std::uint64_t id,
        Metadata&& metadata, std::shared_ptr<Table> table, bool enableWindowedStats,
        std::function<Table::BucketClearer(Metadata*)> bucketClearer,
        std::size_t slotsPerBucket);
  virtual ~Cache() = default;

  // primary functionality; documented in derived classes
  virtual Finding find(void const* key, std::uint32_t keySize) = 0;
  virtual Result insert(CachedValue* value) = 0;
  virtual Result remove(void const* key, std::uint32_t keySize) = 0;
  virtual Result banish(void const* key, std::uint32_t keySize) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the ID for this cache.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t id() const { return _id; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the total memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t size() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the limit on data memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t usageLimit() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the current data memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t usage() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Gives hint to attempt to preallocate space for an incoming load.
  ///
  /// The parameter specifies an expected number of elements to be inserted.
  /// This allows for migration to an appropriately-sized table.
  //////////////////////////////////////////////////////////////////////////////
  void sizeHint(std::uint64_t numElements);

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
  /// @brief Chedk whether the cache is currently migrating or resizing.
  //////////////////////////////////////////////////////////////////////////////
  bool isBusy();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache has begun the process of shutting down.
  //////////////////////////////////////////////////////////////////////////////
  inline bool isShutdown() const { return _shutdown.load(); }

 protected:
  static constexpr std::uint64_t triesFast = 200;
  static constexpr std::uint64_t triesSlow = 10000;

 protected:
  basics::ReadWriteSpinLock _taskLock;
  std::atomic<bool> _shutdown;

  static std::uint64_t _findStatsCapacity;
  bool _enableWindowedStats;
  std::unique_ptr<StatBuffer> _findStats;
  mutable basics::SharedCounter<64> _findHits;
  mutable basics::SharedCounter<64> _findMisses;

  // allow communication with manager
  Manager* _manager;
  std::uint64_t _id;
  Metadata _metadata;

  // manage the actual table
  std::shared_ptr<Table> _tableShrdPtr;
  /// keep a pointer to the current table, which can be atomically set
  std::atomic<Table*> _table;

  Table::BucketClearer _bucketClearer;
  std::size_t _slotsPerBucket;

  // manage eviction rate
  basics::SharedCounter<64> _insertsTotal;
  basics::SharedCounter<64> _insertEvictions;
  static constexpr std::uint64_t _evictionMask = 4095;  // check roughly every 4096 insertions
  static constexpr double _evictionRateThreshold = 0.01;  // if more than 1%
                                                          // evictions in past 4096
                                                          // inserts, migrate

  // times to wait until requesting is allowed again
  std::atomic<Manager::time_point::rep> _migrateRequestTime;
  std::atomic<Manager::time_point::rep> _resizeRequestTime;

  // friend class manager and tasks
  friend class FreeMemoryTask;
  friend class Manager;
  friend class MigrateTask;

 protected:
  // shutdown cache and let its memory be reclaimed
  static void destroy(std::shared_ptr<Cache> const& cache);

  void requestGrow();
  void requestMigrate(std::uint32_t requestedLogSize = 0);

  static void freeValue(CachedValue* value);
  bool reclaimMemory(std::uint64_t size);

  std::uint32_t hashKey(void const* key, std::size_t keySize) const;
  void recordStat(Stat stat);

  bool reportInsert(bool hadEviction);

  // management
  Metadata& metadata();
  std::shared_ptr<Table> table() const;
  void shutdown();
  bool canResize();
  bool canMigrate();
  bool freeMemory();
  bool migrate(std::shared_ptr<Table> newTable);

  virtual std::uint64_t freeMemoryFrom(std::uint32_t hash) = 0;
  virtual void migrateBucket(void* sourcePtr, std::unique_ptr<Table::Subtable> targets,
                             std::shared_ptr<Table> newTable) = 0;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
