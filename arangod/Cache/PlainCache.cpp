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

#include "Cache/PlainCache.h"
#include "Basics/Common.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/PlainBucket.h"
#include "Cache/State.h"
#include "Random/RandomGenerator.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <list>

using namespace arangodb::cache;

static constexpr int64_t TRIES_FAST = 50LL;
static constexpr int64_t TRIES_SLOW = 10000LL;
static constexpr int64_t TRIES_GUARANTEE = -1LL;

Cache::Finding PlainCache::find(void const* key, uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  Finding result(nullptr);
  uint32_t hash = hashKey(key, keySize);

  bool ok;
  PlainBucket* bucket;
  std::tie(ok, bucket) = getBucket(hash, TRIES_FAST);

  if (ok) {
    result.reset(bucket->find(hash, key, keySize));
    recordStat(result.found() ? Stat::findHit : Stat::findMiss);
    bucket->unlock();
    endOperation();
  }

  return result;
}

bool PlainCache::insert(CachedValue* value) {
  TRI_ASSERT(value != nullptr);
  bool inserted = false;
  uint32_t hash = hashKey(value->key(), value->keySize);

  bool ok;
  PlainBucket* bucket;
  std::tie(ok, bucket) = getBucket(hash, TRIES_FAST);

  if (ok) {
    bool allowed = true;
    bool eviction = false;
    int64_t change = value->size();
    CachedValue* candidate = bucket->find(hash, value->key(), value->keySize);

    if (candidate == nullptr && bucket->isFull()) {
      candidate = bucket->evictionCandidate();
      if (candidate == nullptr) {
        allowed = false;
      } else {
        eviction = true;
      }
    }

    if (allowed) {
      if (candidate != nullptr) {
        change -= candidate->size();
      }

      _metadata->lock();
      allowed = _metadata->adjustUsageIfAllowed(change);
      _metadata->unlock();

      if (allowed) {
        if (candidate != nullptr) {
          bucket->evict(candidate, true);
          freeValue(candidate);
        }
        recordStat(eviction ? Stat::insertEviction : Stat::insertNoEviction);
        bucket->insert(hash, value);
        inserted = true;
      } else {
        requestResize();  // let function do the hard work
      }
    }

    bucket->unlock();
    if (inserted) {
      requestMigrate();  // let function do the hard work
    }
    endOperation();
  }

  return inserted;
}

bool PlainCache::remove(void const* key, uint32_t keySize) {
  TRI_ASSERT(key != nullptr);
  bool removed = false;
  uint32_t hash = hashKey(key, keySize);

  bool ok;
  PlainBucket* bucket;
  std::tie(ok, bucket) = getBucket(hash, TRIES_SLOW);

  if (ok) {
    CachedValue* candidate = bucket->remove(hash, key, keySize);

    if (candidate != nullptr) {
      int64_t change = -static_cast<int64_t>(candidate->size());

      _metadata->lock();
      bool allowed = _metadata->adjustUsageIfAllowed(change);
      TRI_ASSERT(allowed);
      _metadata->unlock();

      freeValue(candidate);
    }

    removed = true;
    bucket->unlock();
    endOperation();
  }

  return removed;
}

bool PlainCache::blacklist(void const* key, uint32_t keySize) { return false; }

uint64_t PlainCache::allocationSize(bool enableWindowedStats) {
  return sizeof(PlainCache) +
         StatBuffer::allocationSize(_evictionStatsCapacity) +
         (enableWindowedStats ? (sizeof(StatBuffer) +
                                 StatBuffer::allocationSize(_findStatsCapacity))
                              : 0);
}

std::shared_ptr<Cache> PlainCache::create(Manager* manager,
                                          Manager::MetadataItr metadata,
                                          bool allowGrowth,
                                          bool enableWindowedStats) {
  return std::make_shared<PlainCache>(Cache::ConstructionGuard(), manager,
                                      metadata, allowGrowth,
                                      enableWindowedStats);
}

PlainCache::PlainCache(Cache::ConstructionGuard guard, Manager* manager,
                       Manager::MetadataItr metadata, bool allowGrowth,
                       bool enableWindowedStats)
    : Cache(guard, manager, metadata, allowGrowth, enableWindowedStats),
      _table(nullptr),
      _logSize(0),
      _tableSize(1),
      _maskShift(32),
      _bucketMask(0),
      _auxiliaryTable(nullptr),
      _auxiliaryLogSize(0),
      _auxiliaryTableSize(1),
      _auxiliaryMaskShift(32),
      _auxiliaryBucketMask(0) {
  _state.lock();
  if (isOperational()) {
    _metadata->lock();
    _table = reinterpret_cast<PlainBucket*>(_metadata->table());
    _logSize = _metadata->logSize();
    _tableSize = (1ULL << _logSize);
    _maskShift = 32 - _logSize;
    _bucketMask = (_tableSize - 1) << _maskShift;
    _metadata->unlock();
  }
  _state.unlock();
}

PlainCache::~PlainCache() {
  _state.lock();
  if (!_state.isSet(State::Flag::shutdown)) {
    _state.unlock();
    shutdown();
  }
  if (_state.isLocked()) {
    _state.unlock();
  }
}

bool PlainCache::freeMemory() {
  _state.lock();
  if (!isOperational()) {
    _state.unlock();
    return false;
  }
  startOperation();
  _state.unlock();

  bool underLimit = reclaimMemory(0ULL);
  uint64_t failures = 0;
  while (!underLimit) {
    // pick a random bucket
    uint32_t randomHash = RandomGenerator::interval(UINT32_MAX);
    bool ok;
    PlainBucket* bucket;
    std::tie(ok, bucket) = getBucket(randomHash, TRIES_FAST, false);

    if (ok) {
      failures = 0;
      // evict LRU freeable value if exists
      CachedValue* candidate = bucket->evictionCandidate();

      if (candidate != nullptr) {
        uint64_t size = candidate->size();
        bucket->evict(candidate);
        freeValue(candidate);

        underLimit = reclaimMemory(size);
      }

      bucket->unlock();
    } else {
      failures++;
      if (failures > 100) {
        _state.lock();
        bool shouldQuit = !isOperational();
        _state.unlock();

        if (shouldQuit) {
          break;
        } else {
          failures = 0;
        }
      }
    }
  }

  endOperation();
  return true;
}

bool PlainCache::migrate() {
  _state.lock();
  if (!isOperational()) {
    _state.unlock();
    return false;
  }
  startOperation();
  _metadata->lock();
  if (_metadata->table() == nullptr || _metadata->auxiliaryTable() == nullptr) {
    _metadata->unlock();
    _state.unlock();
    endOperation();
    return false;
  }
  _auxiliaryTable = reinterpret_cast<PlainBucket*>(_metadata->auxiliaryTable());
  _auxiliaryLogSize = _metadata->auxiliaryLogSize();
  _auxiliaryTableSize = (1ULL << _auxiliaryLogSize);
  _auxiliaryMaskShift = (32 - _auxiliaryLogSize);
  _auxiliaryBucketMask = (_auxiliaryTableSize - 1) << _auxiliaryMaskShift;
  _metadata->unlock();
  _state.toggleFlag(State::Flag::migrating);
  _state.unlock();

  for (uint32_t i = 0; i < _tableSize; i++) {
    // lock current bucket
    PlainBucket* bucket = &(_table[i]);
    bucket->lock(-1LL);

    // collect target bucket(s)
    std::vector<PlainBucket*> targets;
    if (_logSize > _auxiliaryLogSize) {
      uint32_t targetIndex = (i << _maskShift) >> _auxiliaryMaskShift;
      targets.emplace_back(&(_auxiliaryTable[targetIndex]));
    } else {
      uint32_t baseIndex = (i << _maskShift) >> _auxiliaryMaskShift;
      for (size_t j = 0; j < (1U << (_auxiliaryLogSize - _logSize)); j++) {
        uint32_t targetIndex = baseIndex + j;
        targets.emplace_back(&(_auxiliaryTable[targetIndex]));
      }
    }
    // lock target bucket(s)
    for (PlainBucket* targetBucket : targets) {
      targetBucket->lock(TRIES_GUARANTEE);
    }

    for (size_t j = 0; j < PlainBucket::SLOTS_DATA; j++) {
      size_t k = PlainBucket::SLOTS_DATA - (j + 1);
      if ((*bucket)._cachedHashes[k] != 0) {
        uint32_t hash = bucket->_cachedHashes[k];
        CachedValue* value = bucket->_cachedData[k];

        uint32_t targetIndex =
            (hash & _auxiliaryBucketMask) >> _auxiliaryMaskShift;
        PlainBucket* targetBucket = &(_auxiliaryTable[targetIndex]);
        bool haveSpace = true;
        if (targetBucket->isFull()) {
          CachedValue* candidate = targetBucket->evictionCandidate();
          if (candidate != nullptr) {
            targetBucket->evict(candidate, true);
            uint64_t size = candidate->size();
            freeValue(candidate);
            reclaimMemory(size);
          } else {
            haveSpace = false;
          }
        }
        if (haveSpace) {
          targetBucket->insert(hash, value);
        } else {
          uint64_t size = value->size();
          freeValue(value);
          reclaimMemory(size);
        }

        bucket->_cachedHashes[k] = 0;
        bucket->_cachedData[k] = nullptr;
      }
    }

    // unlock targets
    for (PlainBucket* targetBucket : targets) {
      targetBucket->unlock();
    }

    // finish up this bucket's migration
    bucket->_state.toggleFlag(State::Flag::migrated);
    bucket->unlock();
  }

  // swap tables and unmark local migrating flag
  _state.lock();
  std::swap(_table, _auxiliaryTable);
  std::swap(_logSize, _auxiliaryLogSize);
  std::swap(_tableSize, _auxiliaryTableSize);
  std::swap(_maskShift, _auxiliaryMaskShift);
  std::swap(_bucketMask, _auxiliaryBucketMask);
  _state.toggleFlag(State::Flag::migrating);
  _state.unlock();

  // clear out old table
  clearTable(_auxiliaryTable, _auxiliaryTableSize);

  // release references to old table
  _state.lock();
  _auxiliaryTable = nullptr;
  _auxiliaryLogSize = 0;
  _auxiliaryTableSize = 1;
  _auxiliaryMaskShift = 32;
  _auxiliaryBucketMask = 0;
  _state.unlock();

  // swap table in metadata
  _metadata->lock();
  _metadata->swapTables();
  _metadata->unlock();

  endOperation();
  return true;
}

void PlainCache::clearTables() {
  if (_table != nullptr) {
    clearTable(_table, _tableSize);
  }
  if (_auxiliaryTable != nullptr) {
    clearTable(_auxiliaryTable, _auxiliaryTableSize);
  }
}

std::pair<bool, PlainBucket*> PlainCache::getBucket(uint32_t hash,
                                                    int64_t maxTries,
                                                    bool singleOperation) {
  PlainBucket* bucket = nullptr;

  bool ok = _state.lock(maxTries);
  if (ok) {
    bool started = false;
    ok = isOperational();
    if (ok) {
      if (singleOperation) {
        startOperation();
        started = true;
        _metadata->lock();
        _manager->reportAccess(_metadata->cache());
        _metadata->unlock();
      }

      bucket = &(_table[getIndex(hash, false)]);
      ok = bucket->lock(maxTries);
      if (ok &&
          bucket->isMigrated()) {  // get bucket from auxiliary table instead
        bucket->unlock();
        bucket = &(_auxiliaryTable[getIndex(hash, true)]);
        ok = bucket->lock(maxTries);
        if (ok && bucket->isMigrated()) {
          ok = false;
          bucket->unlock();
        }
      }
    }
    if (!ok && started) {
      endOperation();
    }
    _state.unlock();
  }

  return std::pair<bool, PlainBucket*>(ok, bucket);
}

void PlainCache::clearTable(PlainBucket* table, uint64_t tableSize) {
  for (uint64_t i = 0; i < tableSize; i++) {
    PlainBucket* bucket = &(table[i]);
    bucket->lock(-1LL);
    for (size_t j = 0; j < PlainBucket::SLOTS_DATA; j++) {
      if (bucket->_cachedData[j] != nullptr) {
        uint64_t size = bucket->_cachedData[j]->size();
        freeValue(bucket->_cachedData[j]);
        reclaimMemory(size);
      }
    }
    bucket->clear();
  }
}

uint32_t PlainCache::getIndex(uint32_t hash, bool useAuxiliary) const {
  if (useAuxiliary) {
    return ((hash & _auxiliaryBucketMask) >> _auxiliaryMaskShift);
  }

  return ((hash & _bucketMask) >> _maskShift);
}
