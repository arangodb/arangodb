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

#include <cstdint>

#include "Cache/TransactionalCache.h"

#include "Basics/SpinLocker.h"
#include "Basics/voc-errors.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Cache/TransactionalBucket.h"
#include "Cache/VPackKeyHasher.h"
#include "Random/RandomGenerator.h"

namespace arangodb::cache {

using SpinLocker = ::arangodb::basics::SpinLocker;

template<typename Hasher>
Finding TransactionalCache<Hasher>::find(void const* key,
                                         std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  Finding result;
  Table::BucketHash hash{Hasher::hashKey(key, keySize)};

  ::ErrorCode status = TRI_ERROR_NO_ERROR;
  Table::BucketLocker guard;
  std::tie(status, guard) = getBucket(hash, Cache::triesFast, false);
  if (status != TRI_ERROR_NO_ERROR) {
    recordMiss();
    result.reportError(status);
  } else {
    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    result.set(bucket.find<Hasher>(hash.value, key, keySize));
    if (result.found()) {
      recordHit();
    } else {
      recordMiss();
      result.reportError(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    }
  }
  return result;
}

template<typename Hasher>
::ErrorCode TransactionalCache<Hasher>::insert(CachedValue* value) {
  TRI_ASSERT(value != nullptr);
  bool maybeMigrate = false;
  Table::BucketHash hash{Hasher::hashKey(value->key(), value->keySize())};

  ::ErrorCode status = TRI_ERROR_NO_ERROR;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesFast, false);
    if (status != TRI_ERROR_NO_ERROR) {
      return status;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    source = guard.source();
    bool allowed = !bucket.isBanished(hash.value);
    if (allowed) {
      std::int64_t change = static_cast<std::int64_t>(value->size());
      CachedValue* candidate =
          bucket.find<Hasher>(hash.value, value->key(), value->keySize());

      if (candidate == nullptr && bucket.isFull()) {
        candidate = bucket.evictionCandidate();
        if (candidate == nullptr) {
          allowed = false;
          status = TRI_ERROR_ARANGO_BUSY;
        }
      }

      if (allowed) {
        if (candidate != nullptr) {
          change -= static_cast<std::int64_t>(candidate->size());
        }

        {
          SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
          allowed = _metadata.adjustUsageIfAllowed(change);
        }

        if (allowed) {
          bool eviction = false;
          if (candidate != nullptr) {
            bucket.evict(candidate);
            if (!Hasher::sameKey(candidate->key(), candidate->keySize(),
                                 value->key(), value->keySize())) {
              eviction = true;
            }
            freeValue(candidate);
          }
          bucket.insert(hash.value, value);
          if (!eviction) {
            TRI_ASSERT(source != nullptr);
            maybeMigrate = source->slotFilled();
          }
          maybeMigrate |= reportInsert(eviction);
        } else {
          requestGrow();  // let function do the hard work
          status = TRI_ERROR_RESOURCE_LIMIT;
        }
      }
    } else {
      status = TRI_ERROR_ARANGO_CONFLICT;
    }
  }

  if (maybeMigrate) {
    TRI_ASSERT(source != nullptr);
    // caution: calling idealSize() can have side effects
    // and trigger a table growth!
    requestMigrate(source->idealSize());  // let function do the hard work
  }

  return status;
}

template<typename Hasher>
::ErrorCode TransactionalCache<Hasher>::remove(void const* key,
                                               std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool maybeMigrate = false;
  Table::BucketHash hash{Hasher::hashKey(key, keySize)};

  ::ErrorCode status = TRI_ERROR_NO_ERROR;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesSlow, false);
    if (status != TRI_ERROR_NO_ERROR) {
      return status;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    source = guard.source();
    CachedValue* candidate = bucket.remove<Hasher>(hash.value, key, keySize);

    if (candidate != nullptr) {
      std::int64_t change = -static_cast<std::int64_t>(candidate->size());
      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
        bool allowed = _metadata.adjustUsageIfAllowed(change);
        TRI_ASSERT(allowed);
      }

      freeValue(candidate);
      TRI_ASSERT(source != nullptr);
      maybeMigrate = source->slotEmptied();
    }
  }

  if (maybeMigrate) {
    TRI_ASSERT(source != nullptr);
    // caution: calling idealSize() can have side effects
    // and trigger a table growth!
    requestMigrate(source->idealSize());
  }

  return status;
}

template<typename Hasher>
::ErrorCode TransactionalCache<Hasher>::banish(void const* key,
                                               std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool maybeMigrate = false;
  Table::BucketHash hash{Hasher::hashKey(key, keySize)};

  ::ErrorCode status = TRI_ERROR_NO_ERROR;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesSlow, false);
    if (status != TRI_ERROR_NO_ERROR) {
      return status;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    source = guard.source();
    CachedValue* candidate = bucket.banish<Hasher>(hash.value, key, keySize);

    if (candidate != nullptr) {
      std::int64_t change = -static_cast<std::int64_t>(candidate->size());

      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
        bool allowed = _metadata.adjustUsageIfAllowed(change);
        TRI_ASSERT(allowed);
      }

      freeValue(candidate);
      TRI_ASSERT(source != nullptr);
      maybeMigrate = source->slotEmptied();
    }
  }

  if (maybeMigrate) {
    TRI_ASSERT(source != nullptr);
    // caution: calling idealSize() can have side effects
    // and trigger a table growth!
    requestMigrate(source->idealSize());
  }

  return status;
}

/// @brief returns the hasher name
template<typename Hasher>
std::string_view TransactionalCache<Hasher>::hasherName() const noexcept {
  return Hasher::name();
}

template<typename Hasher>
std::shared_ptr<Cache> TransactionalCache<Hasher>::create(
    Manager* manager, std::uint64_t id, Metadata&& metadata,
    std::shared_ptr<Table> table, bool enableWindowedStats) {
  return std::make_shared<TransactionalCache<Hasher>>(
      Cache::ConstructionGuard(), manager, id, std::move(metadata),
      std::move(table), enableWindowedStats);
}

template<typename Hasher>
TransactionalCache<Hasher>::TransactionalCache(
    Cache::ConstructionGuard /*guard*/, Manager* manager, std::uint64_t id,
    Metadata&& metadata, std::shared_ptr<Table> table, bool enableWindowedStats)
    : Cache(manager, id, std::move(metadata), std::move(table),
            enableWindowedStats, TransactionalCache::bucketClearer,
            TransactionalBucket::kSlotsData) {}

template<typename Hasher>
TransactionalCache<Hasher>::~TransactionalCache() {
  if (!isShutdown()) {
    try {
      shutdown();
    } catch (...) {
      // no exceptions allowed here
    }
  }
}

template<typename Hasher>
bool TransactionalCache<Hasher>::freeMemoryWhile(
    std::function<bool(std::uint64_t)> const& cb) {
  std::shared_ptr<cache::Table> table = this->table();
  if (!table) {
    return false;
  }

  std::size_t const n = table->size();
  if (n == 0) {
    return false;
  }

  // pick a random start bucket for scanning, so that we don't
  // prefer some buckets over others
  std::uint64_t offset = RandomGenerator::interval(uint64_t(n));

  bool maybeMigrate = false;
  for (std::size_t i = 0; i < n; ++i) {
    std::uint64_t index = (offset + i) % n;

    auto [status, guard] = getBucket(Table::BucketId{index}, Cache::triesFast,
                                     /*singleOperation*/ false);

    if (status != TRI_ERROR_NO_ERROR) {
      continue;
    }

    TransactionalBucket& bucket = guard.template bucket<TransactionalBucket>();
    // evict LRU freeable value if exists
    std::uint64_t reclaimed = bucket.evictCandidate();

    if (reclaimed > 0) {
      maybeMigrate |= guard.source()->slotEmptied();
    }

    if (!cb(reclaimed)) {
      break;
    }
  }

  if (maybeMigrate) {
    // caution: calling idealSize() can have side effects
    // and trigger a table growth!
    requestMigrate(table->idealSize());
  }

  return maybeMigrate;
}

template<typename Hasher>
void TransactionalCache<Hasher>::migrateBucket(
    void* sourcePtr, std::unique_ptr<Table::Subtable> targets,
    Table& newTable) {
  std::uint64_t term = _manager->_transactions.term();

  // lock current bucket
  std::shared_ptr<Table> table = this->table();

  Table::BucketLocker sourceGuard(sourcePtr, table.get(),
                                  Cache::triesGuarantee);
  TransactionalBucket& source = sourceGuard.bucket<TransactionalBucket>();
  term = std::max(term, source._banishTerm);

  {
    // lock target bucket(s)
    std::vector<Table::BucketLocker> targetGuards = targets->lockAllBuckets();

    targets->applyToAllBuckets<TransactionalBucket>(
        [&term](TransactionalBucket& bucket) -> bool {
          term = std::max(term, bucket._banishTerm);
          return true;
        });

    // update all buckets to maximum term found (guaranteed at most the current)
    source.updateBanishTerm(term);
    targets->applyToAllBuckets<TransactionalBucket>(
        [&term](TransactionalBucket& bucket) -> bool {
          bucket.updateBanishTerm(term);
          return true;
        });

    // now actually migrate any relevant banish terms
    if (source.isFullyBanished()) {
      targets->applyToAllBuckets<TransactionalBucket>(
          [](TransactionalBucket& bucket) -> bool {
            if (!bucket.isFullyBanished()) {
              bucket._state.toggleFlag(BucketState::Flag::banished);
            }
            return true;
          });
    } else {
      std::uint64_t totalSize = 0;
      std::uint64_t emptied = 0;
      for (std::size_t j = 0; j < TransactionalBucket::kSlotsBanish; j++) {
        std::uint32_t hash = source._banishHashes[j];
        if (hash != 0) {
          auto targetBucket =
              static_cast<TransactionalBucket*>(targets->fetchBucket(hash));
          CachedValue* candidate =
              targetBucket->banish<Hasher>(hash, nullptr, 0);
          if (candidate != nullptr) {
            std::uint64_t size = candidate->size();
            freeValue(candidate);
            totalSize += size;
            ++emptied;
          }
          source._banishHashes[j] = 0;
        }
      }
      reclaimMemory(totalSize);
      newTable.slotsEmptied(emptied);
    }

    // migrate actual values
    std::uint64_t totalSize = 0;
    std::uint64_t emptied = 0;
    std::uint64_t filled = 0;
    for (std::size_t j = 0; j < TransactionalBucket::kSlotsData; j++) {
      if (source._cachedData[j] != nullptr) {
        std::uint32_t hash = source._cachedHashes[j];
        CachedValue* value = source._cachedData[j];

        auto targetBucket =
            static_cast<TransactionalBucket*>(targets->fetchBucket(hash));
        if (targetBucket->isBanished(hash)) {
          std::uint64_t size = value->size();
          freeValue(value);
          totalSize += size;
        } else {
          bool haveSpace = true;
          if (targetBucket->isFull()) {
            std::uint64_t size = targetBucket->evictCandidate();
            if (size > 0) {
              totalSize += size;
              ++emptied;
            } else {
              haveSpace = false;
            }
          }
          if (haveSpace) {
            targetBucket->insert(hash, value);
            ++filled;
          } else {
            std::uint64_t size = value->size();
            freeValue(value);
            totalSize += size;
          }
        }

        source._cachedHashes[j] = 0;
        source._cachedData[j] = nullptr;
        TRI_ASSERT(source._slotsUsed > 0);
        --source._slotsUsed;
      }
    }
    reclaimMemory(totalSize);
    newTable.slotsFilled(filled);
    newTable.slotsEmptied(emptied);
  }

  // finish up this bucket's migration
  source._state.toggleFlag(BucketState::Flag::migrated);
}

template<typename Hasher>
std::tuple<::ErrorCode, Table::BucketLocker>
TransactionalCache<Hasher>::getBucket(Table::HashOrId bucket,
                                      std::uint64_t maxTries,
                                      bool singleOperation) {
  ::ErrorCode status = TRI_ERROR_NO_ERROR;
  Table::BucketLocker guard;

  std::shared_ptr<Table> table = this->table();
  if (ADB_UNLIKELY(isShutdown() || table == nullptr)) {
    status = TRI_ERROR_SHUTTING_DOWN;
  } else {
    if (singleOperation) {
      _manager->reportAccess(_id);
    }

    std::uint64_t term = _manager->_transactions.term();
    guard = table->fetchAndLockBucket(bucket, maxTries);
    if (guard.isLocked()) {
      guard.bucket<TransactionalBucket>().updateBanishTerm(term);
    } else {
      status = TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  return std::make_tuple(status, std::move(guard));
}

template<typename Hasher>
Table::BucketClearer TransactionalCache<Hasher>::bucketClearer(
    Manager* /*manager*/, Metadata* metadata) {
  return [metadata](void* ptr) -> void {
    auto bucket = static_cast<TransactionalBucket*>(ptr);
    std::uint64_t totalSize = 0;
    bucket->lock(Cache::triesGuarantee);
    for (std::size_t j = 0; j < TransactionalBucket::kSlotsData; j++) {
      if (bucket->_cachedData[j] != nullptr) {
        std::uint64_t size = bucket->_cachedData[j]->size();
        freeValue(bucket->_cachedData[j]);
        totalSize += size;
      }
    }
    {
      SpinLocker metaGuard(SpinLocker::Mode::Read, metadata->lock());
      metadata->adjustUsageIfAllowed(-static_cast<int64_t>(totalSize));
    }
    bucket->clear();
  };
}

// template class instantiations for TransactionalCachee
template class TransactionalCache<BinaryKeyHasher>;
template class TransactionalCache<VPackKeyHasher>;

}  // namespace arangodb::cache
