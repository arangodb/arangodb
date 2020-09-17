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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <list>
#include <thread>

#include "Cache/Cache.h"

#include "Basics/SharedPRNG.h"
#include "Basics/SpinLocker.h"
#include "Basics/SpinUnlocker.h"
#include "Basics/fasthash.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Random/RandomGenerator.h"

namespace arangodb::cache {

using SpinLocker = ::arangodb::basics::SpinLocker;
using SpinUnlocker = ::arangodb::basics::SpinUnlocker;

const std::uint64_t Cache::minSize = 16384;
const std::uint64_t Cache::minLogSize = 14;

std::uint64_t Cache::_findStatsCapacity = 16384;

Cache::Cache(ConstructionGuard guard, Manager* manager, std::uint64_t id,
             Metadata&& metadata, std::shared_ptr<Table> table, bool enableWindowedStats,
             std::function<Table::BucketClearer(Metadata*)> bucketClearer,
             std::size_t slotsPerBucket)
    : _taskLock(),
      _shutdown(false),
      _enableWindowedStats(enableWindowedStats),
      _findStats(nullptr),
      _findHits(),
      _findMisses(),
      _manager(manager),
      _id(id),
      _metadata(std::move(metadata)),
      _tableShrdPtr(std::move(table)),
      _table(_tableShrdPtr.get()),
      _bucketClearer(bucketClearer(&_metadata)),
      _slotsPerBucket(slotsPerBucket),
      _insertsTotal(),
      _insertEvictions(),
      _migrateRequestTime(std::chrono::steady_clock::now().time_since_epoch().count()),
      _resizeRequestTime(std::chrono::steady_clock::now().time_since_epoch().count()) {
  _tableShrdPtr->setTypeSpecifics(_bucketClearer, _slotsPerBucket);
  _tableShrdPtr->enable();
  if (_enableWindowedStats) {
    try {
      _findStats.reset(new StatBuffer(_findStatsCapacity));
    } catch (std::bad_alloc const&) {
      _findStats.reset(nullptr);
      _enableWindowedStats = false;
    }
  }
}

std::uint64_t Cache::size() const {
  if (isShutdown()) {
    return 0;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.allocatedSize;
}

std::uint64_t Cache::usageLimit() const {
  if (isShutdown()) {
    return 0;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.softUsageLimit;
}

std::uint64_t Cache::usage() const {
  if (isShutdown()) {
    return false;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.usage;
}

void Cache::sizeHint(uint64_t numElements) {
  if (isShutdown()) {
    return;
  }

  std::uint64_t numBuckets = static_cast<std::uint64_t>(
      static_cast<double>(numElements) /
      (static_cast<double>(_slotsPerBucket) * Table::idealUpperRatio));
  std::uint32_t requestedLogSize = 0;
  for (; (static_cast<std::uint64_t>(1) << requestedLogSize) < numBuckets; requestedLogSize++) {
  }
  requestMigrate(requestedLogSize);
}

std::pair<double, double> Cache::hitRates() {
  double lifetimeRate = std::nan("");
  double windowedRate = std::nan("");

  std::uint64_t currentMisses = _findMisses.value(std::memory_order_relaxed);
  std::uint64_t currentHits = _findHits.value(std::memory_order_relaxed);
  if (currentMisses + currentHits > 0) {
    lifetimeRate = 100 * (static_cast<double>(currentHits) /
                          static_cast<double>(currentHits + currentMisses));
  }

  if (_enableWindowedStats && _findStats) {
    auto stats = _findStats->getFrequencies();
    if (stats.size() == 1) {
      if (stats[0].first == static_cast<std::uint8_t>(Stat::findHit)) {
        windowedRate = 100.0;
      } else {
        windowedRate = 0.0;
      }
    } else if (stats.size() == 2) {
      if (stats[0].first == static_cast<std::uint8_t>(Stat::findHit)) {
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

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.isResizing();
}

bool Cache::isMigrating() {
  if (isShutdown()) {
    return false;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.isMigrating();
}

bool Cache::isBusy() {
  if (isShutdown()) {
    return false;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.isResizing() || _metadata.isMigrating();
}

void Cache::destroy(std::shared_ptr<Cache> const& cache) {
  if (cache != nullptr) {
    cache->shutdown();
  }
}

void Cache::requestGrow() {
  // fail fast if inside banned window
  if (isShutdown() || std::chrono::steady_clock::now().time_since_epoch().count() <=
                          _resizeRequestTime.load()) {
    return;
  }

  SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock,
                       static_cast<std::size_t>(Cache::triesSlow));
  if (taskGuard.isLocked()) {
    if (std::chrono::steady_clock::now().time_since_epoch().count() >
        _resizeRequestTime.load()) {
      bool ok = false;
      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
        ok = !_metadata.isResizing();
      }
      if (ok) {
        auto result = _manager->requestGrow(this);
        _resizeRequestTime.store(result.second.time_since_epoch().count());
      }
    }
  }
}

void Cache::requestMigrate(std::uint32_t requestedLogSize) {
  // fail fast if inside banned window
  if (isShutdown() || std::chrono::steady_clock::now().time_since_epoch().count() <=
                          _migrateRequestTime.load()) {
    return;
  }

  SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock);
  if (std::chrono::steady_clock::now().time_since_epoch().count() >
      _migrateRequestTime.load()) {
    cache::Table* table = _table.load(std::memory_order_relaxed);
    TRI_ASSERT(table != nullptr);

    bool ok = false;
    {
      SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
      ok = !_metadata.isMigrating() && (requestedLogSize != table->logSize());
    }
    if (ok) {
      auto result = _manager->requestMigrate(this, requestedLogSize);
      _migrateRequestTime.store(result.second.time_since_epoch().count());
    }
  }
}

void Cache::freeValue(CachedValue* value) {
  while (!value->isFreeable()) {
    std::this_thread::yield();
  }

  delete value;
}

bool Cache::reclaimMemory(std::uint64_t size) {
  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  _metadata.adjustUsageIfAllowed(-static_cast<std::int64_t>(size));
  return (_metadata.softUsageLimit >= _metadata.usage);
}

std::uint32_t Cache::hashKey(void const* key, std::size_t keySize) const {
  return (std::max)(static_cast<std::uint32_t>(1), fasthash32(key, keySize, 0xdeadbeefUL));
}

void Cache::recordStat(Stat stat) {
  if ((basics::SharedPRNG::rand() & static_cast<unsigned long>(7)) != 0) {
    return;
  }

  switch (stat) {
    case Stat::findHit: {
      _findHits.add(1, std::memory_order_relaxed);
      if (_enableWindowedStats && _findStats) {
        _findStats->insertRecord(static_cast<std::uint8_t>(Stat::findHit));
      }
      _manager->reportHitStat(Stat::findHit);
      break;
    }
    case Stat::findMiss: {
      _findMisses.add(1, std::memory_order_relaxed);
      if (_enableWindowedStats && _findStats) {
        _findStats->insertRecord(static_cast<std::uint8_t>(Stat::findMiss));
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
    std::uint64_t total = _insertsTotal.value(std::memory_order_relaxed);
    std::uint64_t evictions = _insertEvictions.value(std::memory_order_relaxed);
    if (total > 0 && total > evictions &&
        ((static_cast<double>(evictions) / static_cast<double>(total)) > _evictionRateThreshold)) {
      shouldMigrate = true;
      cache::Table* table = _table.load(std::memory_order_relaxed);
      TRI_ASSERT(table != nullptr);
      table->signalEvictions();
    }
    _insertEvictions.reset(std::memory_order_relaxed);
    _insertsTotal.reset(std::memory_order_relaxed);
  }

  return shouldMigrate;
}

Metadata& Cache::metadata() { return _metadata; }

std::shared_ptr<Table> Cache::table() const {
  return std::atomic_load(&_tableShrdPtr);
}

void Cache::shutdown() {
  SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock);
  auto handle = shared_from_this();  // hold onto self-reference to prevent
                                     // pre-mature shared_ptr destruction
  TRI_ASSERT(handle.get() == this);
  if (!_shutdown.exchange(true)) {
    while (true) {
      {
        SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
        if (!_metadata.isMigrating() && !_metadata.isResizing()) {
          break;
        }
      }

      SpinUnlocker taskUnguard(SpinUnlocker::Mode::Write, _taskLock);
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    cache::Table* table = _table.load(std::memory_order_relaxed);
    if (table != nullptr) {
      std::shared_ptr<Table> extra = table->setAuxiliary(std::shared_ptr<Table>(nullptr));
      if (extra) {
        extra->clear();
        _manager->reclaimTable(extra, false);
      }
      table->clear();
    }

    _manager->reclaimTable(std::atomic_load(&_tableShrdPtr), false);
    {
      SpinLocker metaGuard(SpinLocker::Mode::Write, _metadata.lock());
      _metadata.changeTable(0);
    }
    _manager->unregisterCache(_id);
    _table.store(nullptr, std::memory_order_relaxed);
  }
}

bool Cache::canResize() {
  if (isShutdown()) {
    return false;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return !(_metadata.isResizing() || _metadata.isMigrating());
}

bool Cache::canMigrate() {
  if (isShutdown()) {
    return false;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return !_metadata.isMigrating();
}

bool Cache::freeMemory() {
  if (isShutdown()) {
    return false;
  }

  bool underLimit = reclaimMemory(0ULL);
  std::uint64_t failures = 0;
  while (!underLimit) {
    // pick a random bucket
    std::uint32_t randomHash =
        RandomGenerator::interval(std::numeric_limits<std::uint32_t>::max());
    std::uint64_t reclaimed = freeMemoryFrom(randomHash);

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

  cache::Table* table = _table.load(std::memory_order_relaxed);
  TRI_ASSERT(table != nullptr);
  table->setAuxiliary(newTable);

  // do the actual migration
  for (std::uint32_t i = 0; i < table->size(); i++) {
    migrateBucket(table->primaryBucket(i), table->auxiliaryBuckets(i), newTable);
  }

  // swap tables
  std::shared_ptr<Table> oldTable;
  {
    SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock);
    _table.store(newTable.get(), std::memory_order_relaxed);
    oldTable = std::atomic_exchange(&_tableShrdPtr, newTable);
    std::shared_ptr<Table> confirm =
        oldTable->setAuxiliary(std::shared_ptr<Table>(nullptr));
  }

  // clear out old table and release it
  oldTable->clear();
  _manager->reclaimTable(oldTable, false);

  // unmarking migrating flag
  {
    SpinLocker metaGuard(SpinLocker::Mode::Write, _metadata.lock());
    _metadata.changeTable(table->memoryUsage());
    _metadata.toggleMigrating();
  }

  return true;
}

}  // namespace arangodb::cache
