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

#include "Cache/PlainCache.h"

#include "Basics/Common.h"
#include "Basics/voc-errors.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/PlainBucket.h"
#include "Cache/Table.h"

namespace arangodb::cache {

using SpinLocker = ::arangodb::basics::SpinLocker;

Finding PlainCache::find(void const* key, std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  Finding result;
  std::uint32_t hash = hashKey(key, keySize);

  Result status;
  Table::BucketLocker guard;
  std::tie(status, guard) = getBucket(hash, Cache::triesFast);
  if (status.fail()) {
    result.reportError(status);
    return result;
  }

  PlainBucket& bucket = guard.bucket<PlainBucket>();
  ;
  result.set(bucket.find(hash, key, keySize));
  if (result.found()) {
    recordStat(Stat::findHit);
  } else {
    recordStat(Stat::findMiss);
    status.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    result.reportError(status);
  }

  return result;
}

Result PlainCache::insert(CachedValue* value) {
  TRI_ASSERT(value != nullptr);
  bool maybeMigrate = false;
  std::uint32_t hash = hashKey(value->key(), value->keySize());

  Result status;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesFast);
    if (status.fail()) {
      return status;
    }

    PlainBucket& bucket = guard.bucket<PlainBucket>();
    source = guard.source();
    bool allowed = true;
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
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());  // special case
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
  }

  if (maybeMigrate) {
    requestMigrate(source->idealSize());  // let function do the hard work
  }

  return status;
}

Result PlainCache::remove(void const* key, std::uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool maybeMigrate = false;
  std::uint32_t hash = hashKey(key, keySize);

  Result status;
  Table* source;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesSlow);
    if (status.fail()) {
      return status;
    }
    PlainBucket& bucket = guard.bucket<PlainBucket>();
    source = guard.source();
    CachedValue* candidate = bucket.remove(hash, key, keySize);

    if (candidate != nullptr) {
      std::int64_t change = -static_cast<std::int64_t>(candidate->size());

      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());  // special case
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

Result PlainCache::blacklist(void const* key, std::uint32_t keySize) {
  return {TRI_ERROR_NOT_IMPLEMENTED};
}

std::uint64_t PlainCache::allocationSize(bool enableWindowedStats) {
  return sizeof(PlainCache) +
         (enableWindowedStats
              ? (sizeof(StatBuffer) + StatBuffer::allocationSize(_findStatsCapacity))
              : 0);
}

std::shared_ptr<Cache> PlainCache::create(Manager* manager, std::uint64_t id,
                                          Metadata&& metadata, std::shared_ptr<Table> table,
                                          bool enableWindowedStats) {
  return std::make_shared<PlainCache>(Cache::ConstructionGuard(), manager, id,
                                      std::move(metadata), table, enableWindowedStats);
}

PlainCache::PlainCache(Cache::ConstructionGuard guard, Manager* manager,
                       std::uint64_t id, Metadata&& metadata,
                       std::shared_ptr<Table> table, bool enableWindowedStats)
    : Cache(guard, manager, id, std::move(metadata), table, enableWindowedStats,
            PlainCache::bucketClearer, PlainBucket::slotsData) {}

PlainCache::~PlainCache() {
  if (!isShutdown()) {
    try {
      shutdown();
    } catch (...) {
      // no exceptions allowed here
    }
  }
}

std::uint64_t PlainCache::freeMemoryFrom(std::uint32_t hash) {
  std::uint64_t reclaimed = 0;
  bool maybeMigrate = false;
  Result status;
  {
    Table::BucketLocker guard;
    std::tie(status, guard) = getBucket(hash, Cache::triesFast, false);
    if (status.fail()) {
      return 0;
    }

    PlainBucket& bucket = guard.bucket<PlainBucket>();
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

void PlainCache::migrateBucket(void* sourcePtr, std::unique_ptr<Table::Subtable> targets,
                               std::shared_ptr<Table> newTable) {
  // lock current bucket
  Table::BucketLocker sourceGuard(sourcePtr, _table.load(), Cache::triesGuarantee);
  PlainBucket& source = sourceGuard.bucket<PlainBucket>();

  {
    // lock target bucket(s)
    std::vector<Table::BucketLocker> targetGuards = targets->lockAllBuckets();

    for (std::size_t j = 0; j < PlainBucket::slotsData; j++) {
      std::size_t k = PlainBucket::slotsData - (j + 1);
      if (source._cachedHashes[k] != 0) {
        std::uint32_t hash = source._cachedHashes[k];
        CachedValue* value = source._cachedData[k];

        auto targetBucket = reinterpret_cast<PlainBucket*>(targets->fetchBucket(hash));
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

        source._cachedHashes[k] = 0;
        source._cachedData[k] = nullptr;
      }
    }
  }

  // finish up this bucket's migration
  source._state.toggleFlag(BucketState::Flag::migrated);
}

std::pair<Result, Table::BucketLocker> PlainCache::getBucket(std::uint32_t hash,
                                                             std::uint64_t maxTries,
                                                             bool singleOperation) {
  Result status;
  Table::BucketLocker guard;

  Table* table = _table.load(std::memory_order_relaxed);
  if (isShutdown() || table == nullptr) {
    status.reset(TRI_ERROR_SHUTTING_DOWN);
    return std::make_pair(std::move(status), std::move(guard));
  }

  if (singleOperation) {
    _manager->reportAccess(_id);
  }

  guard = table->fetchAndLockBucket(hash, maxTries);
  if (!guard.isLocked()) {
    status.reset(TRI_ERROR_LOCK_TIMEOUT);
  }

  return std::make_pair(std::move(status), std::move(guard));
}

Table::BucketClearer PlainCache::bucketClearer(Metadata* metadata) {
  return [metadata](void* ptr) -> void {
    auto bucket = reinterpret_cast<PlainBucket*>(ptr);
    bucket->lock(Cache::triesGuarantee);
    for (std::size_t j = 0; j < PlainBucket::slotsData; j++) {
      if (bucket->_cachedData[j] != nullptr) {
        std::uint64_t size = bucket->_cachedData[j]->size();
        freeValue(bucket->_cachedData[j]);
        SpinLocker metaGuard(SpinLocker::Mode::Read, metadata->lock());  // special case
        metadata->adjustUsageIfAllowed(-static_cast<int64_t>(size));
      }
    }
    bucket->clear();
  };
}

}  // namespace arangodb::cache