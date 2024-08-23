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

#include "Cache/Cache.h"

#include "Basics/ScopeGuard.h"
#include "Basics/SpinLocker.h"
#include "Basics/SpinUnlocker.h"
#include "Basics/voc-errors.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"
#include "Cache/PlainCache.h"
#include "Cache/Table.h"
#include "Cache/TransactionalCache.h"
#include "RestServer/SharedPRNGFeature.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <thread>

namespace arangodb::cache {

using SpinLocker = ::arangodb::basics::SpinLocker;
using SpinUnlocker = ::arangodb::basics::SpinUnlocker;

Cache::Cache(
    Manager* manager, std::uint64_t id, Metadata&& metadata,
    std::shared_ptr<Table> table, bool enableWindowedStats,
    std::function<Table::BucketClearer(Cache*, Metadata*)> bucketClearer,
    std::size_t slotsPerBucket)
    : _shutdown(false),
      _manager(manager),
      _id(id),
      _metadata(std::move(metadata)),
      _memoryUsageDiff(0),
      _table(std::move(table)),
      _bucketClearer(bucketClearer(this, &_metadata)),
      _slotsPerBucket(slotsPerBucket),
      _migrateRequestTime(
          std::chrono::steady_clock::now().time_since_epoch().count()),
      _resizeRequestTime(
          std::chrono::steady_clock::now().time_since_epoch().count()),
      _enableWindowedStats(enableWindowedStats) {
  TRI_ASSERT(_table != nullptr);
  _table->setTypeSpecifics(_bucketClearer, _slotsPerBucket);
  _table->enable();
}

Cache::~Cache() {
  // derived classes should have called shutdown() in their dtors,
  // so no memory usage diff should be left here now.
  TRI_ASSERT(_memoryUsageDiff == 0);

  // now subtract potential memory usages for find stats and
  // eviction stats
  std::uint64_t memoryUsage = 0;

  if (_findStatsCreated.load(std::memory_order_acquire)) {
    TRI_ASSERT(_findStats != nullptr);
    memoryUsage += sizeof(decltype(_findStats)::element_type);
    if (_findStats->findStats) {
      memoryUsage += _findStats->findStats->memoryUsage();
    }
  }

  if (_evictionStatsCreated.load(std::memory_order_acquire)) {
    TRI_ASSERT(_evictionStats != nullptr);
    memoryUsage += sizeof(decltype(_evictionStats)::element_type);
  }

  _manager->adjustGlobalAllocation(-static_cast<std::int64_t>(memoryUsage));
}

void Cache::adjustGlobalAllocation(std::int64_t value, bool force) noexcept {
  // if value is 0 but force is true, we still want to set _memoryUsageDiff
  // to 0 and report our current "debt" to the manager, so that we can still
  // set our own value to 0 and be fine.
  if (value != 0 || force) {
    auto expected =
        _memoryUsageDiff.fetch_add(value, std::memory_order_relaxed) + value;
    // only increment global memory usage if our own |delta| is >= 1kb.
    // we do this to release lock pressure on the manager's mutex, which must
    // be acquired to update the global allocation value
    static_assert(kMemoryReportGranularity > 0);
    force |= (expected >= kMemoryReportGranularity ||
              expected <= -kMemoryReportGranularity);

    if (force) {
      do {
        if (_memoryUsageDiff.compare_exchange_weak(expected, 0,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
          if (expected != 0) {
            // only inform manager if there is an actual change in memory usage.
            // the other code above which resets _memoryUsageDiff should be
            // executed even if value was 0 (and the force flag was set).
            _manager->adjustGlobalAllocation(expected);
          }
          break;
        }
      } while (true);
    }
  }
}

std::uint64_t Cache::maxCacheValueSize() const noexcept {
  TRI_ASSERT(_manager != nullptr);
  return _manager->maxCacheValueSize();
}

std::uint64_t Cache::size() const noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return 0;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.allocatedSize;
}

std::uint64_t Cache::usageLimit() const noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return 0;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.softUsageLimit;
}

std::uint64_t Cache::usage() const noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return 0;
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.usage;
}

std::pair<std::uint64_t, std::uint64_t> Cache::sizeAndUsage() const noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return {0, 0};
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return {_metadata.allocatedSize, _metadata.usage};
}

void Cache::sizeHint(uint64_t numElements) {
  if (ADB_UNLIKELY(isShutdown())) {
    return;
  }

  std::uint64_t numBuckets = static_cast<std::uint64_t>(
      static_cast<double>(numElements) /
      (static_cast<double>(_slotsPerBucket) * _manager->idealUpperFillRatio()));
  std::uint32_t requestedLogSize = 0;
  for (; (static_cast<std::uint64_t>(1) << requestedLogSize) < numBuckets &&
         requestedLogSize < Table::kMaxLogSize;
       requestedLogSize++) {
  }

  std::shared_ptr<Table> table = this->table();
  requestedLogSize = std::min(requestedLogSize, Table::kMaxLogSize);
  requestMigrate(table.get(), requestedLogSize, table->logSize());
}

std::pair<double, double> Cache::hitRates() {
  double lifetimeRate = std::nan("");
  double windowedRate = std::nan("");

  if (_findStatsCreated.load(std::memory_order_acquire)) {
    TRI_ASSERT(_findStats != nullptr);

    std::uint64_t currentMisses =
        _findStats->findMisses.value(std::memory_order_relaxed);
    std::uint64_t currentHits =
        _findStats->findHits.value(std::memory_order_relaxed);
    if (currentMisses + currentHits > 0) {
      lifetimeRate = 100 * (static_cast<double>(currentHits) /
                            static_cast<double>(currentHits + currentMisses));
    }

    if (_findStats->findStats) {
      auto stats = _findStats->findStats->getFrequencies();
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
          windowedRate =
              100 * (static_cast<double>(currentHits) /
                     static_cast<double>(currentHits + currentMisses));
        }
      }
    }
  }

  return std::pair<double, double>(lifetimeRate, windowedRate);
}

bool Cache::isResizing() const noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return false;
  }

  return isResizingFlagSet();
}

bool Cache::isResizingFlagSet() const noexcept {
  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.isResizing();
}

bool Cache::isMigrating() const noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return false;
  }

  return isMigratingFlagSet();
}

bool Cache::isMigratingFlagSet() const noexcept {
  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.isMigrating();
}

bool Cache::isResizingOrMigratingFlagSet() const noexcept {
  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  return _metadata.isResizing() || _metadata.isMigrating();
}

void Cache::requestGrow() {
  // fail fast if inside banned window
  if (isShutdown() ||
      std::chrono::steady_clock::now().time_since_epoch().count() <=
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

void Cache::requestMigrate(Table* table, std::uint32_t requestedLogSize,
                           std::uint32_t currentLogSize) {
  TRI_ASSERT(table != nullptr);
  if (requestedLogSize == currentLogSize) {
    // nothing to do. exit immediately.
    return;
  }
  // fail fast if inside banned window
  if (isShutdown() ||
      std::chrono::steady_clock::now().time_since_epoch().count() <=
          _migrateRequestTime.load()) {
    return;
  }

  SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock);
  if (std::chrono::steady_clock::now().time_since_epoch().count() >
      _migrateRequestTime.load()) {
    bool ok = false;
    {
      SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
      ok = !_metadata.isMigrating() && (requestedLogSize != table->logSize());
    }
    if (ok) {
      requestedLogSize = std::min(requestedLogSize, Table::kMaxLogSize);
      auto result = _manager->requestMigrate(this, requestedLogSize);
      _migrateRequestTime.store(result.second.time_since_epoch().count());
    }
  }
}

void Cache::freeValue(CachedValue* value) noexcept {
  while (!value->isFreeable()) {
    std::this_thread::yield();
  }

  delete value;
}

bool Cache::reclaimMemory(std::uint64_t size) noexcept {
  if (size != 0) {
    adjustGlobalAllocation(-static_cast<std::int64_t>(size), /*force*/ false);
  }

  SpinLocker metaGuard(SpinLocker::Mode::Read, _metadata.lock());
  _metadata.adjustUsageIfAllowed(-static_cast<std::int64_t>(size));
  return (_metadata.softUsageLimit >= _metadata.usage);
}

void Cache::recordHit() {
  if ((_manager->sharedPRNG().rand() & static_cast<unsigned long>(7)) != 0) {
    return;
  }

  try {
    ensureFindStats();
  } catch (...) {
    return;
  }

  TRI_ASSERT(_findStats != nullptr);

  _findStats->findHits.add(1, std::memory_order_relaxed);
  if (_findStats->findStats) {
    _findStats->findStats->insertRecord(
        static_cast<std::uint8_t>(Stat::findHit));
  }
  _manager->reportHit();
}

void Cache::recordMiss() {
  if ((_manager->sharedPRNG().rand() & static_cast<unsigned long>(7)) != 0) {
    return;
  }

  try {
    ensureFindStats();
  } catch (...) {
    return;
  }

  TRI_ASSERT(_findStats != nullptr);

  _findStats->findMisses.add(1, std::memory_order_relaxed);
  if (_findStats->findStats) {
    _findStats->findStats->insertRecord(
        static_cast<std::uint8_t>(Stat::findMiss));
  }
  _manager->reportMiss();
}

bool Cache::reportInsert(Table* table, bool hadEviction) {
  TRI_ASSERT(table != nullptr);
  try {
    ensureEvictionStats();
  } catch (...) {
    // in case we run out of memory and can't create the eviction stats,
    // we simply pretend that everything is ok, and don't record the
    // insert here. this situation will hopefully be fixed in one of the
    // following insert attempts
    return false;
  }

  TRI_ASSERT(_evictionStats != nullptr);

  bool shouldMigrate = false;
  if (hadEviction) {
    _evictionStats->insertEvictions.add(1, std::memory_order_relaxed);
  }
  _evictionStats->insertsTotal.add(1, std::memory_order_relaxed);
  if ((_manager->sharedPRNG().rand() & kEvictionMask) == 0) {
    std::uint64_t total =
        _evictionStats->insertsTotal.value(std::memory_order_relaxed);
    std::uint64_t evictions =
        _evictionStats->insertEvictions.value(std::memory_order_relaxed);
    if (total > 0 && total > evictions &&
        ((static_cast<double>(evictions) / static_cast<double>(total)) >
         kEvictionRateThreshold)) {
      shouldMigrate = true;
      table->signalEvictions();
    }
    _evictionStats->insertEvictions.reset(std::memory_order_relaxed);
    _evictionStats->insertsTotal.reset(std::memory_order_relaxed);
  }

  return shouldMigrate;
}

void Cache::ensureFindStats() {
  absl::call_once(_findStatsOnceFlag, [this]() {
    TRI_ASSERT(!_findStatsCreated.load(std::memory_order_relaxed));
    TRI_ASSERT(_findStats == nullptr);

    _findStats = std::make_unique<FindStats>();
    if (_enableWindowedStats) {
      _findStats->findStats = std::make_unique<StatBuffer>(
          _manager->sharedPRNG(), Manager::kFindStatsCapacity);
    }
    _manager->adjustGlobalAllocation(
        sizeof(decltype(_findStats)::element_type) +
        (_enableWindowedStats ? _findStats->findStats->memoryUsage() : 0));

    _findStatsCreated.store(true, std::memory_order_release);
  });
  TRI_ASSERT(_findStats != nullptr);
}

void Cache::ensureEvictionStats() {
  absl::call_once(_evictionStatsOnceFlag, [this]() {
    TRI_ASSERT(!_evictionStatsCreated.load(std::memory_order_relaxed));
    TRI_ASSERT(_evictionStats == nullptr);

    _evictionStats = std::make_unique<EvictionStats>();
    _manager->adjustGlobalAllocation(
        sizeof(decltype(_evictionStats)::element_type));

    _evictionStatsCreated.store(true, std::memory_order_release);
  });

  TRI_ASSERT(_evictionStats != nullptr);
}

Metadata& Cache::metadata() { return _metadata; }

std::shared_ptr<Table> Cache::table() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _manager->trackTableCall();
#endif
  SpinLocker guard(SpinLocker::Mode::Read, _tableLock);
  return _table;
}

void Cache::shutdown() {
  SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock);
  auto handle = shared_from_this();  // hold onto self-reference to prevent
                                     // pre-mature shared_ptr destruction
  TRI_ASSERT(handle.get() == this);
  if (!_shutdown.exchange(true)) {
    while (true) {
      if (!isResizingOrMigratingFlagSet()) {
        break;
      }

      SpinUnlocker taskUnguard(SpinUnlocker::Mode::Write, _taskLock);

      // sleep a bit without holding the locks
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    std::shared_ptr<cache::Table> table = this->table();
    if (table != nullptr) {
      std::shared_ptr<Table> extra =
          table->setAuxiliary(std::shared_ptr<Table>());
      if (extra != nullptr) {
        extra->clear();
        _manager->reclaimTable(std::move(extra), false);
      }
      table->clear();
      _manager->reclaimTable(std::move(table), false);
    }

    {
      SpinLocker metaGuard(SpinLocker::Mode::Write, _metadata.lock());
      _metadata.changeTable(0);
    }
    _manager->unregisterCache(_id);
    {
      SpinLocker tableGuard(SpinLocker::Mode::Write, _tableLock);
      _table.reset();
    }
  }

  taskGuard.release();

  // report memory usage diff to manager
  adjustGlobalAllocation(/*value*/ 0, /*force*/ true);
}

bool Cache::canResize() noexcept {
  if (ADB_UNLIKELY(isShutdown())) {
    return false;
  }

  return !isResizingOrMigratingFlagSet();
}

bool Cache::freeMemory() {
  TRI_ASSERT(isResizingFlagSet());

  if (ADB_UNLIKELY(isShutdown())) {
    return false;
  }

  bool underLimit = reclaimMemory(0ULL);
  if (!underLimit) {
    underLimit = freeMemoryWhile([this](std::uint64_t reclaimed) -> bool {
      TRI_ASSERT(reclaimed > 0);
      bool underLimit = reclaimMemory(reclaimed);
      // continue only if we are not under the limit yet (after reclamation)
      return !underLimit;
    });
  }

  return underLimit;
}

bool Cache::migrate(std::shared_ptr<Table> newTable) {
  TRI_ASSERT(isMigratingFlagSet());

  auto migratingGuard = scopeGuard([this]() noexcept {
    // unmarking migrating flag if necessary
    SpinLocker metaGuard(SpinLocker::Mode::Write, _metadata.lock());

    TRI_ASSERT(_metadata.isMigrating());
    _metadata.toggleMigrating();
    TRI_ASSERT(!_metadata.isMigrating());
  });

  if (ADB_UNLIKELY(isShutdown())) {
    // will trigger the scopeGuard
    return false;
  }

  newTable->setTypeSpecifics(_bucketClearer, _slotsPerBucket);
  newTable->enable();

  std::shared_ptr<cache::Table> table = this->table();
  TRI_ASSERT(table != nullptr);
  std::shared_ptr<Table> oldAuxiliary = table->setAuxiliary(newTable);
  TRI_ASSERT(oldAuxiliary == nullptr);

  // do the actual migration
  for (std::uint64_t i = 0; i < table->size();
       i++) {  // need uint64 for end condition
    migrateBucket(table.get(), table->primaryBucket(static_cast<uint32_t>(i)),
                  table->auxiliaryBuckets(static_cast<uint32_t>(i)), *newTable);
  }

  // swap tables
  std::shared_ptr<Table> oldTable;
  {
    SpinLocker taskGuard(SpinLocker::Mode::Write, _taskLock);
    oldTable = this->table();
    {
      SpinLocker tableGuard(SpinLocker::Mode::Write, _tableLock);
      _table = newTable;
    }
    oldTable->setAuxiliary(std::shared_ptr<Table>());
  }

  TRI_ASSERT(oldTable != nullptr);

  // unmarking migrating flag
  {
    SpinLocker metaGuard(SpinLocker::Mode::Write, _metadata.lock());
    _metadata.changeTable(newTable->memoryUsage());
    TRI_ASSERT(_metadata.isMigrating());
    _metadata.toggleMigrating();
    TRI_ASSERT(!_metadata.isMigrating());
  }
  migratingGuard.cancel();

  // clear out old table and release it
  oldTable->clear();
  _manager->reclaimTable(std::move(oldTable), false);

  return true;
}

}  // namespace arangodb::cache
