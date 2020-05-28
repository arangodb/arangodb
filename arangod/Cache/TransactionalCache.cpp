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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>

#include "Cache/TransactionalCache.h"

#include "Basics/SpinLocker.h"
#include "Basics/voc-errors.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Cache/TransactionalBucket.h"
#include "Logger/Logger.h"

namespace arangodb::cache {

using SpinLocker = ::arangodb::basics::SpinLocker;

Finding TransactionalCache::find(void const* key, std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  Finding result;
  std::uint32_t hash = hashKey(key, keySize);

  Result status;
  Table::BucketLocker guard;
  std::tie(status, guard) = getBucket(hash, Cache::triesFast, false);
  if (status.fail()) {
    result.reportError(status);
    return result;
  }

  TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
  result.set(bucket.find(hash, key, keySize));
  if (result.found()) {
    recordStat(Stat::findHit);
  } else {
    recordStat(Stat::findMiss);
    status.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    result.reportError(status);
  }
  recordStat(result.found() ? Stat::findHit : Stat::findMiss);

  return result;
}

Result TransactionalCache::insert(CachedValue* value) {
  TRI_ASSERT(value != nullptr);
  bool maybeMigrate = false;
  std::uint32_t hash = hashKey(value->key(), value->keySize());

  Result status;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesFast, false);
    if (status.fail()) {
      return status;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    source = guard.source();
    bool allowed = !bucket.isBlacklisted(hash);
    if (allowed) {
      std::int64_t change = static_cast<std::int64_t>(value->size());
      CachedValue* candidate = bucket.find(hash, value->key(), value->keySize());

      if (candidate == nullptr && bucket.isFull()) {
        candidate = bucket.evictionCandidate();
        if (candidate == nullptr) {
          allowed = false;
          status.reset(TRI_ERROR_ARANGO_BUSY);
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
            bucket.evict(candidate, true);
            if (!candidate->sameKey(value->key(), value->keySize())) {
              eviction = true;
            }
            freeValue(candidate);
          }
          bucket.insert(hash, value);
          if (!eviction) {
            maybeMigrate = source->slotFilled();
          }
          maybeMigrate |= reportInsert(eviction);
        } else {
          requestGrow();  // let function do the hard work
          status.reset(TRI_ERROR_RESOURCE_LIMIT);
        }
      }
    } else {
      status.reset(TRI_ERROR_ARANGO_CONFLICT);
    }
  }

  if (maybeMigrate) {
    requestMigrate(source->idealSize());  // let function do the hard work
  }

  return status;
}

Result TransactionalCache::remove(void const* key, std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool maybeMigrate = false;
  std::uint32_t hash = hashKey(key, keySize);

  Result status;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesSlow, false);
    if (status.fail()) {
      return status;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    source = guard.source();
    CachedValue* candidate = bucket.remove(hash, key, keySize);

    if (candidate != nullptr) {
      std::int64_t change = -static_cast<std::int64_t>(candidate->size());
      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
        bool allowed = _metadata.adjustUsageIfAllowed(change);
        TRI_ASSERT(allowed);
      }

      freeValue(candidate);
      maybeMigrate = source->slotEmptied();
    }
  }

  if (maybeMigrate) {
    requestMigrate(source->idealSize());
  }

  return status;
}

Result TransactionalCache::blacklist(void const* key, std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool maybeMigrate = false;
  std::uint32_t hash = hashKey(key, keySize);

  Result status;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesSlow, false);
    if (status.fail()) {
      return status;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    source = guard.source();
    CachedValue* candidate = bucket.blacklist(hash, key, keySize);

    if (candidate != nullptr) {
      std::int64_t change = -static_cast<std::int64_t>(candidate->size());

      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
        bool allowed = _metadata.adjustUsageIfAllowed(change);
        TRI_ASSERT(allowed);
      }

      freeValue(candidate);
      maybeMigrate = source->slotEmptied();
    }
  }

  if (maybeMigrate) {
    requestMigrate(source->idealSize());
  }

  return status;
}

uint64_t TransactionalCache::allocationSize(bool enableWindowedStats) {
  return sizeof(TransactionalCache) +
         (enableWindowedStats
              ? (sizeof(StatBuffer) + StatBuffer::allocationSize(_findStatsCapacity))
              : 0);
}

std::shared_ptr<Cache> TransactionalCache::create(Manager* manager, std::uint64_t id,
                                                  Metadata&& metadata,
                                                  std::shared_ptr<Table> table,
                                                  bool enableWindowedStats) {
  return std::make_shared<TransactionalCache>(Cache::ConstructionGuard(),
                                              manager, id, std::move(metadata),
                                              table, enableWindowedStats);
}

TransactionalCache::TransactionalCache(Cache::ConstructionGuard guard, Manager* manager,
                                       std::uint64_t id, Metadata&& metadata,
                                       std::shared_ptr<Table> table, bool enableWindowedStats)
    : Cache(guard, manager, id, std::move(metadata), table, enableWindowedStats,
            TransactionalCache::bucketClearer, TransactionalBucket::slotsData) {}

TransactionalCache::~TransactionalCache() {
  if (!isShutdown()) {
    try {
      shutdown();
    } catch (...) {
      // no exceptions allowed here
    }
  }
}

uint64_t TransactionalCache::freeMemoryFrom(std::uint32_t hash) {
  std::uint64_t reclaimed = 0;
  bool maybeMigrate = false;

  Result status;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesFast, false);
    if (status.fail()) {
      return 0;
    }

    TransactionalBucket& bucket = guard.bucket<TransactionalBucket>();
    // evict LRU freeable value if exists
    CachedValue* candidate = bucket.evictionCandidate();

    if (candidate != nullptr) {
      reclaimed = candidate->size();
      bucket.evict(candidate);
      freeValue(candidate);
      maybeMigrate = guard.source()->slotEmptied();
    }
  }

  cache::Table* table = _table.load(std::memory_order_relaxed);
  if (table) {
    std::int32_t size = table->idealSize();
    if (maybeMigrate) {
      requestMigrate(size);
    }
  }

  return reclaimed;
}

void TransactionalCache::migrateBucket(void* sourcePtr,
                                       std::unique_ptr<Table::Subtable> targets,
                                       std::shared_ptr<Table> newTable) {
  std::uint64_t term = _manager->_transactions.term();

  // lock current bucket
  Table::BucketLocker sourceGuard(sourcePtr, _table.load(), Cache::triesGuarantee);
  TransactionalBucket& source = sourceGuard.bucket<TransactionalBucket>();
  term = std::max(term, source._blacklistTerm);

  {
    // lock target bucket(s)
    std::vector<Table::BucketLocker> targetGuards = targets->lockAllBuckets();

    targets->applyToAllBuckets<TransactionalBucket>([&term](TransactionalBucket& bucket) -> bool {
      term = std::max(term, bucket._blacklistTerm);
      return true;
    });

    // update all buckets to maximum term found (guaranteed at most the current)
    source.updateBlacklistTerm(term);
    targets->applyToAllBuckets<TransactionalBucket>([&term](TransactionalBucket& bucket) -> bool {
      bucket.updateBlacklistTerm(term);
      return true;
    });

    // now actually migrate any relevant blacklist terms
    if (source.isFullyBlacklisted()) {
      targets->applyToAllBuckets<TransactionalBucket>([](TransactionalBucket& bucket) -> bool {
        if (!bucket.isFullyBlacklisted()) {
          bucket._state.toggleFlag(BucketState::Flag::blacklisted);
        }
        return true;
      });
    } else {
      for (std::size_t j = 0; j < TransactionalBucket::slotsBlacklist; j++) {
        std::uint32_t hash = source._blacklistHashes[j];
        if (hash != 0) {
          auto targetBucket =
              reinterpret_cast<TransactionalBucket*>(targets->fetchBucket(hash));
          CachedValue* candidate = targetBucket->blacklist(hash, nullptr, 0);
          if (candidate != nullptr) {
            std::uint64_t size = candidate->size();
            freeValue(candidate);
            reclaimMemory(size);
            newTable->slotEmptied();
          }
          source._blacklistHashes[j] = 0;
        }
      }
    }

    // migrate actual values
    for (std::size_t j = 0; j < TransactionalBucket::slotsData; j++) {
      std::size_t k = TransactionalBucket::slotsData - (j + 1);
      if (source._cachedData[k] != nullptr) {
        std::uint32_t hash = source._cachedHashes[k];
        CachedValue* value = source._cachedData[k];

        auto targetBucket =
            reinterpret_cast<TransactionalBucket*>(targets->fetchBucket(hash));
        if (targetBucket->isBlacklisted(hash)) {
          std::uint64_t size = value->size();
          freeValue(value);
          reclaimMemory(size);
        } else {
          bool haveSpace = true;
          if (targetBucket->isFull()) {
            CachedValue* candidate = targetBucket->evictionCandidate();
            if (candidate != nullptr) {
              targetBucket->evict(candidate, true);
              std::uint64_t size = candidate->size();
              freeValue(candidate);
              reclaimMemory(size);
              newTable->slotEmptied();
            } else {
              haveSpace = false;
            }
          }
          if (haveSpace) {
            targetBucket->insert(hash, value);
            newTable->slotFilled();
          } else {
            std::uint64_t size = value->size();
            freeValue(value);
            reclaimMemory(size);
          }
        }

        source._cachedHashes[k] = 0;
        source._cachedData[k] = nullptr;
      }
    }
  }

  // finish up this bucket's migration
  source._state.toggleFlag(BucketState::Flag::migrated);
}

std::tuple<Result, Table::BucketLocker> TransactionalCache::getBucket(
    std::uint32_t hash, std::uint64_t maxTries, bool singleOperation) {
  Result status;
  Table::BucketLocker guard;

  Table* table = _table.load(std::memory_order_relaxed);
  if (isShutdown() || table == nullptr) {
    status.reset(TRI_ERROR_SHUTTING_DOWN);
    return std::make_tuple(std::move(status), std::move(guard));
  }

  if (singleOperation) {
    _manager->reportAccess(_id);
  }

  std::uint64_t term = _manager->_transactions.term();
  guard = table->fetchAndLockBucket(hash, maxTries);
  if (guard.isLocked()) {
    guard.bucket<TransactionalBucket>().updateBlacklistTerm(term);
  } else {
    status.reset(TRI_ERROR_LOCK_TIMEOUT);
  }

  return std::make_tuple(std::move(status), std::move(guard));
}

Table::BucketClearer TransactionalCache::bucketClearer(Metadata* metadata) {
  return [metadata](void* ptr) -> void {
    auto bucket = reinterpret_cast<TransactionalBucket*>(ptr);
    bucket->lock(Cache::triesGuarantee);
    for (std::size_t j = 0; j < TransactionalBucket::slotsData; j++) {
      if (bucket->_cachedData[j] != nullptr) {
        std::uint64_t size = bucket->_cachedData[j]->size();
        freeValue(bucket->_cachedData[j]);
        SpinLocker metaGuard(SpinLocker::Mode::Read, metadata->lock());
        metadata->adjustUsageIfAllowed(-static_cast<int64_t>(size));
      }
    }
    bucket->clear();
  };
}

}  // namespace arangodb::cache
