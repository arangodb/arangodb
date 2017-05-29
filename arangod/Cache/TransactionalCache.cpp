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

#include "Cache/TransactionalCache.h"
#include "Basics/Common.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/State.h"
#include "Cache/Table.h"
#include "Cache/TransactionalBucket.h"
#include "Logger/Logger.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <list>

using namespace arangodb::cache;

Finding TransactionalCache::find(void const* key, uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  Finding result;
  uint32_t hash = hashKey(key, keySize);

  bool ok;
  TransactionalBucket* bucket;
  std::shared_ptr<Table> source;
  std::tie(ok, bucket, source) = getBucket(hash, Cache::triesFast);

  if (ok) {
    result.set(bucket->find(hash, key, keySize));
    recordStat(result.found() ? Stat::findHit : Stat::findMiss);
    bucket->unlock();
    endOperation();
  }

  return result;
}

bool TransactionalCache::insert(CachedValue* value) {
  TRI_ASSERT(value != nullptr);
  bool inserted = false;
  uint32_t hash = hashKey(value->key(), value->keySize);

  bool ok;
  TransactionalBucket* bucket;
  std::shared_ptr<Table> source;
  std::tie(ok, bucket, source) = getBucket(hash, Cache::triesFast);

  if (ok) {
    bool maybeMigrate = false;
    bool allowed = !bucket->isBlacklisted(hash);
    if (allowed) {
      int64_t change = value->size();
      CachedValue* candidate = bucket->find(hash, value->key(), value->keySize);

      if (candidate == nullptr && bucket->isFull()) {
        candidate = bucket->evictionCandidate();
        if (candidate == nullptr) {
          allowed = false;
        }
      }

      if (allowed) {
        if (candidate != nullptr) {
          change -= candidate->size();
        }

        _metadata.lock();
        allowed = _metadata.adjustUsageIfAllowed(change);
        _metadata.unlock();

        if (allowed) {
          bool eviction = false;
          if (candidate != nullptr) {
            bucket->evict(candidate, true);
            if (!candidate->sameKey(value->key(), value->keySize)) {
              eviction = true;
            }
            freeValue(candidate);
          }
          bucket->insert(hash, value);
          inserted = true;
          if (!eviction) {
            maybeMigrate = source->slotFilled();
          }
          maybeMigrate |= reportInsert(eviction);
        } else {
          requestGrow();  // let function do the hard work
        }
      }
    }

    bucket->unlock();
    if (maybeMigrate) {
      requestMigrate(_table->idealSize());  // let function do the hard work
    }
    endOperation();
  }

  return inserted;
}

bool TransactionalCache::remove(void const* key, uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool removed = false;
  uint32_t hash = hashKey(key, keySize);

  bool ok;
  TransactionalBucket* bucket;
  std::shared_ptr<Table> source;
  std::tie(ok, bucket, source) = getBucket(hash, Cache::triesSlow);

  if (ok) {
    bool maybeMigrate = false;
    CachedValue* candidate = bucket->remove(hash, key, keySize);

    if (candidate != nullptr) {
      int64_t change = -static_cast<int64_t>(candidate->size());

      _metadata.lock();
      bool allowed = _metadata.adjustUsageIfAllowed(change);
      TRI_ASSERT(allowed);
      _metadata.unlock();

      freeValue(candidate);
      maybeMigrate = source->slotEmptied();
    }

    removed = true;
    bucket->unlock();
    if (maybeMigrate) {
      requestMigrate(_table->idealSize());
    }
    endOperation();
  }

  return removed;
}

bool TransactionalCache::blacklist(void const* key, uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool blacklisted = false;
  uint32_t hash = hashKey(key, keySize);

  bool ok;
  TransactionalBucket* bucket;
  std::shared_ptr<Table> source;
  std::tie(ok, bucket, source) = getBucket(hash, Cache::triesSlow);

  if (ok) {
    bool maybeMigrate = false;
    CachedValue* candidate = bucket->blacklist(hash, key, keySize);
    blacklisted = true;

    if (candidate != nullptr) {
      int64_t change = -static_cast<int64_t>(candidate->size());

      _metadata.lock();
      bool allowed = _metadata.adjustUsageIfAllowed(change);
      TRI_ASSERT(allowed);
      _metadata.unlock();

      freeValue(candidate);
      maybeMigrate = source->slotEmptied();
    }

    bucket->unlock();
    if (maybeMigrate) {
      requestMigrate(_table->idealSize());
    }
    endOperation();
  }

  return blacklisted;
}

uint64_t TransactionalCache::allocationSize(bool enableWindowedStats) {
  return sizeof(TransactionalCache) +
         (enableWindowedStats ? (sizeof(StatBuffer) +
                                 StatBuffer::allocationSize(_findStatsCapacity))
                              : 0);
}

std::shared_ptr<Cache> TransactionalCache::create(Manager* manager,
                                                  Metadata metadata,
                                                  std::shared_ptr<Table> table,
                                                  bool enableWindowedStats) {
  return std::make_shared<TransactionalCache>(Cache::ConstructionGuard(),
                                              manager, metadata, table,
                                              enableWindowedStats);
}

TransactionalCache::TransactionalCache(Cache::ConstructionGuard guard,
                                       Manager* manager, Metadata metadata,
                                       std::shared_ptr<Table> table,
                                       bool enableWindowedStats)
    : Cache(guard, manager, metadata, table, enableWindowedStats,
            TransactionalCache::bucketClearer, TransactionalBucket::slotsData) {
}

TransactionalCache::~TransactionalCache() {
  _state.lock();
  if (!_state.isSet(State::Flag::shutdown)) {
    _state.unlock();
    shutdown();
  }
  if (_state.isLocked()) {
    _state.unlock();
  }
}

uint64_t TransactionalCache::freeMemoryFrom(uint32_t hash) {
  uint64_t reclaimed = 0;
  bool ok;
  TransactionalBucket* bucket;
  std::shared_ptr<Table> source;
  std::tie(ok, bucket, source) = getBucket(hash, Cache::triesFast, false);

  if (ok) {
    bool maybeMigrate = false;
    // evict LRU freeable value if exists
    CachedValue* candidate = bucket->evictionCandidate();

    if (candidate != nullptr) {
      reclaimed = candidate->size();
      bucket->evict(candidate);
      freeValue(candidate);
      maybeMigrate = source->slotEmptied();
    }

    bucket->unlock();
    if (maybeMigrate) {
      requestMigrate(_table->idealSize());
    }
  }

  return reclaimed;
}

void TransactionalCache::migrateBucket(void* sourcePtr,
                                       std::unique_ptr<Table::Subtable> targets,
                                       std::shared_ptr<Table> newTable) {
  uint64_t term = _manager->_transactions.term();

  // lock current bucket
  auto source = reinterpret_cast<TransactionalBucket*>(sourcePtr);
  source->lock(Cache::triesGuarantee);
  term = std::max(term, source->_blacklistTerm);

  // lock target bucket(s)
  int64_t tries = Cache::triesGuarantee;
  targets->applyToAllBuckets([&term, tries](void* ptr) -> bool {
    auto targetBucket = reinterpret_cast<TransactionalBucket*>(ptr);
    bool locked = targetBucket->lock(tries);
    term = std::max(term, targetBucket->_blacklistTerm);
    return locked;
  });

  // update all buckets to maximum term found (guaranteed at most the current)
  source->updateBlacklistTerm(term);
  targets->applyToAllBuckets([&term](void* ptr) -> bool {
    auto targetBucket = reinterpret_cast<TransactionalBucket*>(ptr);
    targetBucket->updateBlacklistTerm(term);
    return true;
  });
  // now actually migrate any relevant blacklist terms
  if (source->isFullyBlacklisted()) {
    targets->applyToAllBuckets([](void* ptr) -> bool {
      auto targetBucket = reinterpret_cast<TransactionalBucket*>(ptr);
      if (!targetBucket->isFullyBlacklisted()) {
        targetBucket->_state.toggleFlag(State::Flag::blacklisted);
      }
      return true;
    });
  } else {
    for (size_t j = 0; j < TransactionalBucket::slotsBlacklist; j++) {
      uint32_t hash = source->_blacklistHashes[j];
      if (hash != 0) {
        auto targetBucket =
            reinterpret_cast<TransactionalBucket*>(targets->fetchBucket(hash));
        CachedValue* candidate = targetBucket->blacklist(hash, nullptr, 0);
        if (candidate != nullptr) {
          uint64_t size = candidate->size();
          freeValue(candidate);
          reclaimMemory(size);
          newTable->slotEmptied();
        }
        source->_blacklistHashes[j] = 0;
      }
    }
  }

  // migrate actual values
  for (size_t j = 0; j < TransactionalBucket::slotsData; j++) {
    size_t k = TransactionalBucket::slotsData - (j + 1);
    if (source->_cachedData[k] != nullptr) {
      uint32_t hash = source->_cachedHashes[k];
      CachedValue* value = source->_cachedData[k];

      auto targetBucket =
          reinterpret_cast<TransactionalBucket*>(targets->fetchBucket(hash));
      if (targetBucket->isBlacklisted(hash)) {
        uint64_t size = value->size();
        freeValue(value);
        reclaimMemory(size);
      } else {
        bool haveSpace = true;
        if (targetBucket->isFull()) {
          CachedValue* candidate = targetBucket->evictionCandidate();
          if (candidate != nullptr) {
            targetBucket->evict(candidate, true);
            uint64_t size = candidate->size();
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
          uint64_t size = value->size();
          freeValue(value);
          reclaimMemory(size);
        }
      }

      source->_cachedHashes[k] = 0;
      source->_cachedData[k] = nullptr;
    }
  }

  // unlock targets
  targets->applyToAllBuckets([](void* ptr) -> bool {
    auto bucket = reinterpret_cast<TransactionalBucket*>(ptr);
    bucket->unlock();
    return true;
  });

  // finish up this bucket's migration
  source->_state.toggleFlag(State::Flag::migrated);
  source->unlock();
}

std::tuple<bool, TransactionalBucket*, std::shared_ptr<Table>>
TransactionalCache::getBucket(uint32_t hash, int64_t maxTries,
                              bool singleOperation) {
  TransactionalBucket* bucket = nullptr;
  std::shared_ptr<Table> source(nullptr);

  bool ok = _state.lock(maxTries);
  if (ok) {
    bool started = false;
    ok = isOperational();
    if (ok) {
      if (singleOperation) {
        startOperation();
        started = true;
        _manager->reportAccess(shared_from_this());
      }

      uint64_t term = _manager->_transactions.term();
      auto pair = _table->fetchAndLockBucket(hash, maxTries);
      bucket = reinterpret_cast<TransactionalBucket*>(pair.first);
      source = pair.second;
      ok = (bucket != nullptr);
      if (ok) {
        bucket->updateBlacklistTerm(term);
      }
    }
    if (!ok && started) {
      endOperation();
    }
    _state.unlock();
  }

  return std::make_tuple(ok, bucket, source);
}

Table::BucketClearer TransactionalCache::bucketClearer(Metadata* metadata) {
  int64_t tries = Cache::triesGuarantee;
  return [metadata, tries](void* ptr) -> void {
    auto bucket = reinterpret_cast<TransactionalBucket*>(ptr);
    bucket->lock(tries);
    for (size_t j = 0; j < TransactionalBucket::slotsData; j++) {
      if (bucket->_cachedData[j] != nullptr) {
        uint64_t size = bucket->_cachedData[j]->size();
        freeValue(bucket->_cachedData[j]);
        metadata->lock();
        metadata->adjustUsageIfAllowed(-static_cast<int64_t>(size));
        metadata->unlock();
      }
    }
    bucket->clear();
  };
}
