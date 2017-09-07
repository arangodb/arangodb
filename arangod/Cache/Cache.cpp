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

#include "Cache/Cache.h"
#include "Basics/Common.h"
#include "Basics/SharedPRNG.h"
#include "Basics/fasthash.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Random/RandomGenerator.h"

#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <list>
#include <thread>

using namespace arangodb::cache;

const uint64_t Cache::minSize = 16384;
const uint64_t Cache::minLogSize = 14;

uint64_t Cache::_findStatsCapacity = 16384;

Cache::ConstructionGuard::ConstructionGuard() {}

Cache::Cache(ConstructionGuard guard, Manager* manager, uint64_t id, Metadata metadata,
             std::shared_ptr<Table> table, bool enableWindowedStats,
             std::function<Table::BucketClearer(Metadata*)> bucketClearer,
             size_t slotsPerBucket)
    : _taskLock(),
      _shutdown(false),
      _enableWindowedStats(enableWindowedStats),
      _findStats(nullptr),
      _findHits(),
      _findMisses(),
      _manager(manager),
      _id(id),
      _metadata(metadata),
      _tableShrdPtr(table),
      _table(table.get()),
      _bucketClearer(bucketClearer(&_metadata)),
      _slotsPerBucket(slotsPerBucket),
      _insertsTotal(),
      _insertEvictions(),
      _migrateRequestTime(std::chrono::steady_clock::now()),
      _resizeRequestTime(std::chrono::steady_clock::now()) {
  _table->setTypeSpecifics(_bucketClearer, _slotsPerBucket);
  _table->enable();
  if (_enableWindowedStats) {
    try {
      _findStats.reset(new StatBuffer(_findStatsCapacity));
    } catch (std::bad_alloc) {
      _findStats.reset(nullptr);
      _enableWindowedStats = false;
    }
  }
}

uint64_t Cache::size() const {
  if (isShutdown()) {
    return 0;
  }

  _metadata.readLock();
  uint64_t size = _metadata.allocatedSize;
  _metadata.readUnlock();

  return size;
}

uint64_t Cache::usageLimit() const {
  if (isShutdown()) {
    return 0;
  }

  _metadata.readLock();
  uint64_t limit = _metadata.softUsageLimit;
  _metadata.readUnlock();

  return limit;
}

uint64_t Cache::usage() const {
  if (isShutdown()) {
    return false;
  }

  _metadata.readLock();
  uint64_t usage = _metadata.usage;
  _metadata.readUnlock();

  return usage;
}

void Cache::sizeHint(uint64_t numElements) {
  if (isShutdown()) {
    return;
  }

  uint64_t numBuckets = static_cast<uint64_t>(static_cast<double>(numElements)
    / (static_cast<double>(_slotsPerBucket) * Table::idealUpperRatio));
  uint32_t requestedLogSize = 0;
  for (; (static_cast<uint64_t>(1) << requestedLogSize) < numBuckets;
    requestedLogSize++) {}
  requestMigrate(requestedLogSize);
}

std::pair<double, double> Cache::hitRates() {
  double lifetimeRate = std::nan("");
  double windowedRate = std::nan("");

  uint64_t currentMisses = _findMisses.value(std::memory_order_relaxed);
  uint64_t currentHits = _findHits.value(std::memory_order_relaxed);
  if (currentMisses + currentHits > 0) {
    lifetimeRate = 100 * (static_cast<double>(currentHits) /
                          static_cast<double>(currentHits + currentMisses));
  }

  if (_enableWindowedStats && _findStats) {
    auto stats = _findStats->getFrequencies();
    if (stats.size() == 1) {
      if (stats[0].first == static_cast<uint8_t>(Stat::findHit)) {
        windowedRate = 100.0;
      } else {
        windowedRate = 0.0;
      }
    } else if (stats.size() == 2) {
      if (stats[0].first == static_cast<uint8_t>(Stat::findHit)) {
        currentHits = stats[0].second;
        currentMisses = stats[1].second;
      } else {
        currentHits = stats[1].second;
        currentMisses = stats[0].second;
      }
      if (currentHits + currentMisses > 0) {
        windowedRate = 100 * (static_cast<double>(currentHits) /
                              static_cast<double>(currentHits + currentMisses));
      }
    }
  }

  return std::pair<double, double>(lifetimeRate, windowedRate);
}

bool Cache::isResizing() {
  if (isShutdown()) {
    return false;
  }

  _metadata.readLock();
  bool resizing = _metadata.isResizing();
  _metadata.readUnlock();

  return resizing;
}

bool Cache::isMigrating() {
  if (isShutdown()) {
    return false;
  }

  _metadata.readLock();
  bool migrating = _metadata.isMigrating();
  _metadata.readUnlock();

  return migrating;
}

bool Cache::isBusy() {
  if (isShutdown()) {
    return false;
  }

  _metadata.readLock();
  bool busy = _metadata.isResizing() ||
              _metadata.isMigrating();
  _metadata.readUnlock();

  return busy;
}

void Cache::destroy(std::shared_ptr<Cache> cache) {
  if (cache) {
    cache->shutdown();
  }
}

void Cache::requestGrow() {
  // fail fast if inside banned window
  if (std::chrono::steady_clock::now() <= _resizeRequestTime) {
    return;
  }

  bool ok = _taskLock.writeLock(Cache::triesSlow);
  if (ok) {
    if (!isShutdown() && (std::chrono::steady_clock::now() > _resizeRequestTime)) {
      _metadata.readLock();
      ok = !_metadata.isResizing();
      _metadata.readUnlock();
      if (ok) {
        std::tie(ok, _resizeRequestTime) =
            _manager->requestGrow(this);
      }
    }
    _taskLock.writeUnlock();
  }
}

void Cache::requestMigrate(uint32_t requestedLogSize) {
  // fail fast if inside banned window
  if (std::chrono::steady_clock::now() <= _migrateRequestTime) {
    return;
  }

  bool ok = _taskLock.writeLock(Cache::triesGuarantee);
  if (ok) {
    if (!isShutdown() && std::chrono::steady_clock::now() > _migrateRequestTime) {
      _metadata.readLock();
      ok = !_metadata.isMigrating() &&
           (requestedLogSize != _table->logSize());
      _metadata.readUnlock();
      if (ok) {
        std::tie(ok, _migrateRequestTime) =
            _manager->requestMigrate(this, requestedLogSize);
      }
    }
    _taskLock.writeUnlock();
  }
}

void Cache::freeValue(CachedValue* value) {
  while (!value->isFreeable()) {
    std::this_thread::yield();
  }

  delete value;
}

bool Cache::reclaimMemory(uint64_t size) {
  _metadata.readLock();
  _metadata.adjustUsageIfAllowed(-static_cast<int64_t>(size));
  bool underLimit = (_metadata.softUsageLimit >= _metadata.usage);
  _metadata.readUnlock();

  return underLimit;
}

uint32_t Cache::hashKey(void const* key, size_t keySize) const {
  return (std::max)(static_cast<uint32_t>(1),
                    fasthash32(key, keySize, 0xdeadbeefUL));
}

void Cache::recordStat(Stat stat) {
  if ((basics::SharedPRNG::rand() & static_cast<unsigned long>(7)) != 0) {
    return;
  }

  switch (stat) {
    case Stat::findHit: {
      _findHits.add(1, std::memory_order_relaxed);
      if (_enableWindowedStats && _findStats) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findHit));
      }
      _manager->reportHitStat(Stat::findHit);
      break;
    }
    case Stat::findMiss: {
      _findMisses.add(1, std::memory_order_relaxed);
      if (_enableWindowedStats && _findStats) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findMiss));
      }
      _manager->reportHitStat(Stat::findMiss);
      break;
    }
    default: { break; }
  }
}

bool Cache::reportInsert(bool hadEviction) {
  bool shouldMigrate = false;
  if (hadEviction) {
    _insertEvictions.add(1, std::memory_order_relaxed);
  }
  _insertsTotal.add(1, std::memory_order_relaxed);
  if ((basics::SharedPRNG::rand() & _evictionMask) == 0) {
    uint64_t total = _insertsTotal.value(std::memory_order_relaxed);
    uint64_t evictions = _insertEvictions.value(std::memory_order_relaxed);
    if ((static_cast<double>(evictions) / static_cast<double>(total))
        > _evictionRateThreshold) {
      shouldMigrate = true;
      _table->signalEvictions();
    }
    _insertEvictions.reset(std::memory_order_relaxed);
    _insertsTotal.reset(std::memory_order_relaxed);
  }

  return shouldMigrate;
}

Metadata* Cache::metadata() { return &_metadata; }

std::shared_ptr<Table> Cache::table() const {
  return std::atomic_load(&_tableShrdPtr);
}

void Cache::shutdown() {
  _taskLock.writeLock();
  auto handle = shared_from_this();  // hold onto self-reference to prevent
                                     // pre-mature shared_ptr destruction
  TRI_ASSERT(handle.get() == this);
  if (!_shutdown) {
    _shutdown = true;

    _metadata.readLock();
    while (true) {
      if (!_metadata.isMigrating() &&
          !_metadata.isResizing()) {
        break;
      }
      _metadata.readUnlock();
      _taskLock.writeUnlock();
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      _taskLock.writeLock();
      _metadata.readLock();
    }
    _metadata.readUnlock();

    std::shared_ptr<Table> extra =
        _table->setAuxiliary(std::shared_ptr<Table>(nullptr));
    if (extra) {
      extra->clear();
      _manager->reclaimTable(extra);
    }
    _table->clear();
    _manager->reclaimTable(std::atomic_load(&_tableShrdPtr));
    _manager->unregisterCache(_id);
    _table = nullptr;
  }
  _metadata.writeLock();
  _metadata.changeTable(0);
  _metadata.writeUnlock();

  _taskLock.writeUnlock();
}

bool Cache::canResize() {
  if (isShutdown()) {
    return false;
  }

  bool allowed = true;
  _metadata.readLock();
  if (_metadata.isResizing() ||
      _metadata.isMigrating()) {
    allowed = false;
  }
  _metadata.readUnlock();

  return allowed;
}

bool Cache::canMigrate() {
  if (isShutdown()) {
    return false;
  }

  bool allowed = true;
  _metadata.readLock();
  if (_metadata.isMigrating()) {
    allowed = false;
  }
  _metadata.readUnlock();

  return allowed;
}

bool Cache::freeMemory() {
  if (isShutdown()) {
    return false;
  }

  bool underLimit = reclaimMemory(0ULL);
  uint64_t failures = 0;
  while (!underLimit) {
    // pick a random bucket
    uint32_t randomHash = RandomGenerator::interval(UINT32_MAX);
    uint64_t reclaimed = freeMemoryFrom(randomHash);

    if (reclaimed > 0) {
      failures = 0;
      underLimit = reclaimMemory(reclaimed);
    } else {
      failures++;
      if (failures > 100) {
        if (isShutdown()) {
          break;
        } else {
          failures = 0;
        }
      }
    }
  }

  return true;
}

bool Cache::migrate(std::shared_ptr<Table> newTable) {
  if (isShutdown()) {
    return false;
  }

  newTable->setTypeSpecifics(_bucketClearer, _slotsPerBucket);
  newTable->enable();
  _table->setAuxiliary(newTable);

  // do the actual migration
  for (uint32_t i = 0; i < _table->size(); i++) {
    migrateBucket(_table->primaryBucket(i), _table->auxiliaryBuckets(i),
                            newTable);
  }

  // swap tables
  _taskLock.writeLock();
  _table = newTable.get();
  std::shared_ptr<Table> oldTable = std::atomic_exchange(&_tableShrdPtr, newTable);
  std::shared_ptr<Table> confirm =
      oldTable->setAuxiliary(std::shared_ptr<Table>(nullptr));
  _taskLock.writeUnlock();

  // clear out old table and release it
  oldTable->clear();
  _manager->reclaimTable(oldTable);

  // unmarking migrating flag
  _metadata.writeLock();
  _metadata.changeTable(_table->memoryUsage());
  _metadata.toggleMigrating();
  _metadata.writeUnlock();

  return true;
}
