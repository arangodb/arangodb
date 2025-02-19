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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Basics/SharedCounter.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Manager.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include <absl/base/call_once.h>

namespace arangodb::cache {

template<typename Hasher>
class PlainCache;
template<typename Hasher>
class TransactionalCache;

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
    template<typename Hasher>
    friend class PlainCache;
    template<typename Hasher>
    friend class TransactionalCache;
  };

  Cache(Manager* manager, std::uint64_t id, Metadata&& metadata,
        std::shared_ptr<Table> table, bool enableWindowedStats,
        std::function<Table::BucketClearer(Cache*, Metadata*)> bucketClearer,
        std::size_t slotsPerBucket);

 public:
  virtual ~Cache();

  using StatBuffer = FrequencyBuffer<std::uint8_t>;

  static constexpr std::uint64_t kMinSize = 16384;
  static constexpr std::uint64_t kMinLogSize = 14;
  // reporting granularity for memory usage increases/decreases by each cache.
  // only when the value of |_memoryUsageDiff| exceeds this value, the extra
  // memory used by the cache will be reported to the manager. smaller values
  // here mean more eager reporting, but this can lead to higher contention on
  // the cache's global lock. thus by default we only report memory usage
  // increases/ decreases if a cache has buffered allocations/deallocations >=
  // this threshold. the other allocations/deallocations by the cache will still
  // be tracked locally. however, they will only be reported to the manager once
  // they have reached the threshold value.
  static constexpr std::int64_t kMemoryReportGranularity = 4096;

  static constexpr std::uint64_t triesGuarantee =
      std::numeric_limits<std::uint64_t>::max();
  static constexpr std::uint64_t triesFast = 200;
  static constexpr std::uint64_t triesSlow = 10000;

  // primary functionality; documented in derived classes
  virtual Finding find(void const* key, std::uint32_t keySize) = 0;
  virtual ::ErrorCode insert(CachedValue* value) = 0;
  virtual ::ErrorCode remove(void const* key, std::uint32_t keySize) = 0;
  virtual ::ErrorCode banish(void const* key, std::uint32_t keySize) = 0;

  // inform the manager about additional (global) memory usage.
  // this is necessary so that the cache does not only count its own memory,
  // but also feeds back larger allocations/deallocations to the manager,
  // so that the manager can accurately track the global memory usage (by all
  // caches combined)
  void adjustGlobalAllocation(std::int64_t value, bool force) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the ID for this cache.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t id() const noexcept { return _id; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the maximum size for cached values
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t maxCacheValueSize() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the total memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] std::uint64_t size() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the limit on data memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] std::uint64_t usageLimit() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the current data memory usage for this cache in bytes.
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] std::uint64_t usage() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the current allocated size and data memory usage for this
  /// cache in bytes. The values are fetched under the same lock, so they will
  /// be consistent.
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> sizeAndUsage()
      const noexcept;

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
  /// @brief Check whether the cache is currently in the process of resizing
  /// or shutting down.
  //////////////////////////////////////////////////////////////////////////////
  bool isResizing() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of resizing.
  //////////////////////////////////////////////////////////////////////////////
  bool isResizingFlagSet() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of migrating
  /// or shutting down.
  //////////////////////////////////////////////////////////////////////////////
  bool isMigrating() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of migrating.
  //////////////////////////////////////////////////////////////////////////////
  bool isMigratingFlagSet() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache is currently in the process of resizing
  /// or migrating.
  //////////////////////////////////////////////////////////////////////////////
  bool isResizingOrMigratingFlagSet() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Check whether the cache has begun the process of shutting down.
  //////////////////////////////////////////////////////////////////////////////
  bool isShutdown() const noexcept { return _shutdown.load(); }

  // helper struct that takes care of inserting into the cache during
  // object construction. The insertion is not guaranteed to work. To
  // check whether the insertion succeeded, check the "status" member!
  template<typename CacheType>
  struct Inserter {
    Inserter(CacheType& cache, void const* key, std::size_t keySize,
             void const* value, std::size_t valueSize) {
      std::unique_ptr<CachedValue> cv{
          CachedValue::construct(key, keySize, value, valueSize)};
      if (ADB_LIKELY(cv)) {
        status = cache.insert(cv.get());
        if (status == TRI_ERROR_NO_ERROR) {
          cv.release();
        }
      } else {
        status = TRI_ERROR_OUT_OF_MEMORY;
      }
    }

    Inserter(Inserter const& other) = delete;
    Inserter& operator=(Inserter const& other) = delete;

    ::ErrorCode status = TRI_ERROR_NO_ERROR;
  };

  // same as Cache::Inserter, but more lightweight. Does not provide
  // any indication about whether the insertion succeeded.
  template<typename CacheType>
  struct SimpleInserter {
    SimpleInserter(CacheType& cache, void const* key, std::size_t keySize,
                   void const* value, std::size_t valueSize) {
      std::unique_ptr<CachedValue> cv{
          CachedValue::construct(key, keySize, value, valueSize)};
      if (ADB_LIKELY(cv) && cache.insert(cv.get()) == TRI_ERROR_NO_ERROR) {
        cv.release();
      }
    }

    SimpleInserter(SimpleInserter const& other) = delete;
    SimpleInserter& operator=(SimpleInserter const& other) = delete;
  };

 protected:
  void requestGrow();
  void requestMigrate(Table* table, std::uint32_t requestedLogSize,
                      std::uint32_t currentLogSize);

  static void freeValue(CachedValue* value) noexcept;
  bool reclaimMemory(std::uint64_t size) noexcept;

  void recordHit();
  void recordMiss();

  bool reportInsert(Table* table, bool hadEviction);

  // management
  Metadata& metadata();
  std::shared_ptr<Table> table() const;
  void shutdown();
  [[nodiscard]] bool canResize() noexcept;

  // called by FreeMemory async task
  // precondition: metadata's isResizing() flag must be set
  // postcondition: metadata's isResizing() flag is still set
  bool freeMemory();

  // called by Migrate async task
  // precondition: metadata's isMigrating() flag must be set
  // postcondition: metadata's isMigrating() flag is not set
  bool migrate(std::shared_ptr<Table> newTable);

  // free memory while callback returns true
  virtual bool freeMemoryWhile(
      std::function<bool(std::uint64_t)> const& cb) = 0;

  virtual void migrateBucket(Table* table, void* sourcePtr,
                             std::unique_ptr<Table::Subtable> targets,
                             Table& newTable) = 0;

  void ensureFindStats();

  basics::ReadWriteSpinLock _taskLock;
  std::atomic<bool> _shutdown;

  // allow communication with manager
  Manager* _manager;
  std::uint64_t const _id;
  Metadata _metadata;

  // local buffer for tracking allocations/deallocations by this cache.
  // this value will be modified locally until the value of |_memoryUsageDiff|
  // exceeds the reporting granularity threshold.
  // once the threshold is exceeded, the current value of _memoryUsageDiff will
  // be reported to the manager, and the value of _memoryUsageDiff will be reset
  // to zero. that means caches may report their memory usage in a delayed
  // fashion. additionally, the last kMemoryReportGranularity bytes that were
  // already used by the cache may not have been reported to the manager. these
  // bytes will only be missing temporarily though. the missing bytes can be
  // reported later, when there are additional allocations or deallocations,
  // or when the cache gets destroyed. they will only be not be missing
  // completely.
  std::atomic<std::int64_t> _memoryUsageDiff;

 private:
  void ensureEvictionStats();

  // manage the actual table
  // note: access to the _table pointer must be guarded by _tableLock!
  mutable basics::ReadWriteSpinLock _tableLock;
  std::shared_ptr<Table> _table;

  Table::BucketClearer _bucketClearer;
  std::size_t const _slotsPerBucket;

  // this struct is allocated on the heap only lazily
  struct FindStats {
    mutable basics::SharedCounter<64> findHits;
    mutable basics::SharedCounter<64> findMisses;
    std::unique_ptr<StatBuffer> findStats;
  };

  // manage eviction rate
  struct EvictionStats {
    basics::SharedCounter<64> insertsTotal;
    basics::SharedCounter<64> insertEvictions;
  };

  // this is a control variable that ensures that the _findStats
  // are created lazily and exactly once per Cache object.
  absl::once_flag _findStatsOnceFlag;
  // this variable flips from false to true only once when the
  // _findStats object is lazily created.
  std::atomic<bool> _findStatsCreated = false;
  // the actual find stats object
  std::unique_ptr<FindStats> _findStats;

  // this is a control variable that ensures that the _evictionStats
  // are created lazily and exactly once per Cache object.
  absl::once_flag _evictionStatsOnceFlag;
  // this variable flips from false to true only once when the
  // _evictionStats object is lazily created.
  std::atomic<bool> _evictionStatsCreated = false;
  // the actual eviction stats object
  std::unique_ptr<EvictionStats> _evictionStats;

  // times to wait until requesting is allowed again
  std::atomic<Manager::time_point::rep> _migrateRequestTime;
  std::atomic<Manager::time_point::rep> _resizeRequestTime;

  bool const _enableWindowedStats;

  static constexpr std::uint64_t kEvictionMask =
      4095;  // check roughly every 4096 insertions
  static constexpr double kEvictionRateThreshold =
      0.01;  // if more than 1%
             // evictions in past kEvictionMask
             // inserts, migrate

  // friend class manager and tasks
  friend class FreeMemoryTask;
  friend class Manager;
  friend class MigrateTask;
};

}  // end namespace arangodb::cache
