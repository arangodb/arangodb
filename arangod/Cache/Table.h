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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_TABLE_H
#define ARANGODB_CACHE_TABLE_H

#include "Basics/ReadWriteSpinLock.h"
#include "Cache/BucketState.h"
#include "Cache/Common.h"

#include <cstdint>
#include <limits>
#include <memory>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Class to manage operations on a table of buckets.
////////////////////////////////////////////////////////////////////////////////
class Table : public std::enable_shared_from_this<Table> {
 public:
  static constexpr double idealLowerRatio = 0.04;
  static constexpr double idealUpperRatio = 0.25;
  static const std::uint32_t minLogSize;
  static const std::uint32_t maxLogSize;
  static constexpr std::uint32_t standardLogSizeAdjustment = 6;
  static constexpr std::uint64_t triesGuarantee =
      std::numeric_limits<std::uint64_t>::max();
  static constexpr std::uint64_t padding = BUCKET_SIZE;

  typedef std::function<void(void*)> BucketClearer;

 private:
  struct GenericBucket {
    BucketState _state;
    static constexpr std::size_t paddingSize = BUCKET_SIZE - sizeof(BucketState);
    std::uint8_t _padding[paddingSize];
    GenericBucket();
    bool lock(std::uint64_t maxTries);
    void unlock();
    void clear();
    bool isMigrated() const;
  };
  static_assert(sizeof(GenericBucket) == BUCKET_SIZE,
                "Expected sizeof(GenericBucket) == BUCKET_SIZE.");

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Helper class for RAII-style bucket locking
  //////////////////////////////////////////////////////////////////////////////
  struct BucketLocker {
    BucketLocker();
    BucketLocker(void* bucket, Table* source, std::uint64_t maxAttempts);
    BucketLocker(BucketLocker&&) noexcept;
    ~BucketLocker();
    BucketLocker& operator=(BucketLocker&&) noexcept;

    BucketLocker(BucketLocker const&) = delete;             // no copy
    BucketLocker& operator=(BucketLocker const&) = delete;  // no copy

    bool isValid() const;
    bool isLocked() const;

    Table* source() const;

    template <typename BucketType>
    BucketType& bucket() const;

    void release();

   private:
    void steal(BucketLocker&&);

   private:
    GenericBucket* _bucket;
    Table* _source;
    bool _locked;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Helper class for migration.
  //////////////////////////////////////////////////////////////////////////////
  struct Subtable {
    Subtable(std::shared_ptr<Table> source, GenericBucket* buckets,
             std::uint64_t size, std::uint32_t mask, std::uint32_t shift);
    void* fetchBucket(std::uint32_t hash);

    std::vector<BucketLocker> lockAllBuckets();

    template <typename BucketType>
    bool applyToAllBuckets(std::function<bool(BucketType&)> cb);

   private:
    std::shared_ptr<Table> _source;
    GenericBucket* _buckets;
    std::uint64_t _size;
    std::uint32_t _mask;
    std::uint32_t _shift;
  };

 public:
  Table() = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Construct a new table of size 2^(logSize) in disabled state.
  //////////////////////////////////////////////////////////////////////////////
  explicit Table(std::uint32_t logSize);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destroy the table
  //////////////////////////////////////////////////////////////////////////////
  ~Table();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the memory usage for a table with specified logSize
  //////////////////////////////////////////////////////////////////////////////
  static std::uint64_t allocationSize(std::uint32_t logSize);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the memory usage of the table.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t memoryUsage() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the number of buckets in the table.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t size() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the logSize of the table. (2^(logSize()) == size())
  //////////////////////////////////////////////////////////////////////////////
  std::uint32_t logSize() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Fetches a pointer to the bucket mapped by the given hash, and locks
  /// it.
  ///
  /// Returns nullptrs if it could not lock the bucket within maxTries
  /// attempts. If maxTries is negative, it will not limit the number of
  /// attempts. If the primary bucket is migrated, it will attempt a lookup in
  /// the auxiliary table. The second member of the returned pair is the source
  /// table for the bucket returned as the first member.
  //////////////////////////////////////////////////////////////////////////////
  BucketLocker fetchAndLockBucket(std::uint32_t hash,
                                  std::uint64_t maxTries = std::numeric_limits<std::uint64_t>::max());

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Sets the auxiliary table.
  ///
  /// If the parameter is non-null, then the return value will be null if
  /// successful and equal to the parameter otherwise. If the parameter is null,
  /// then the return value will be the existing auxiliary table (possibly
  /// null).
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<Table> setAuxiliary(std::shared_ptr<Table> const& table);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a pointer to the specified bucket in the primary table,
  /// regardless of migration status.
  //////////////////////////////////////////////////////////////////////////////
  void* primaryBucket(std::uint64_t index);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a subtable in the auxiliary index which corresponds to the
  /// specified bucket in the primary table.
  //////////////////////////////////////////////////////////////////////////////
  std::unique_ptr<Table::Subtable> auxiliaryBuckets(std::uint32_t index);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set cache-type-specific members.
  //////////////////////////////////////////////////////////////////////////////
  void setTypeSpecifics(BucketClearer clearer, std::size_t slotsPerBucket);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset table to fully empty state. Disables table.
  //////////////////////////////////////////////////////////////////////////////
  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Enables table.
  //////////////////////////////////////////////////////////////////////////////
  void enable();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report that a slot was filled.
  ///
  /// If this causes the fill ratio to exceed the ideal upper limit, the return
  /// value will be true, and the cache should request migration to a larger
  /// table.
  //////////////////////////////////////////////////////////////////////////////
  bool slotFilled();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report that a slot was emptied.
  ///
  /// If this causes the fill ratio to fall below the ideal lower limit, the
  /// return value will be true, and the cache should request migration to a
  /// smaller table.
  //////////////////////////////////////////////////////////////////////////////
  bool slotEmptied();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Report that there have been too many evictions.
  ///
  /// Will force a subsequent idealSize() call to return a larger table size.
  //////////////////////////////////////////////////////////////////////////////
  void signalEvictions();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the ideal size of the table based on fill ratio.
  //////////////////////////////////////////////////////////////////////////////
  uint32_t idealSize();

 private:
  basics::ReadWriteSpinLock _lock;
  bool _disabled;
  bool _evictions;

  std::uint32_t _logSize;
  std::uint64_t _size;
  std::uint32_t _shift;
  std::uint32_t _mask;
  std::unique_ptr<std::uint8_t[]> _buffer;
  GenericBucket* _buckets;

  std::shared_ptr<Table> _auxiliary;

  BucketClearer _bucketClearer;

  uint64_t _slotsTotal;
  std::atomic<std::uint64_t> _slotsUsed;

 private:
  void disable();
  bool isEnabled(std::uint64_t maxTries = triesGuarantee);
  static void defaultClearer(void* ptr);
};

};  // end namespace cache
};  // end namespace arangodb

#endif
