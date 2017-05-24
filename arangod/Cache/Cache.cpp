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
#include "Basics/fasthash.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"
#include "Cache/State.h"
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

Cache::Cache(ConstructionGuard guard, Manager* manager, Metadata metadata,
             std::shared_ptr<Table> table, bool enableWindowedStats,
             std::function<Table::BucketClearer(Metadata*)> bucketClearer,
             size_t slotsPerBucket)
    : _state(),
      _enableWindowedStats(enableWindowedStats),
      _findStats(nullptr),
      _findHits(0),
      _findMisses(0),
      _manager(manager),
      _metadata(metadata),
      _table(table),
      _bucketClearer(bucketClearer(&_metadata)),
      _slotsPerBucket(slotsPerBucket),
      _insertsTotal(0),
      _insertEvictions(0),
      _openOperations(0),
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

uint64_t Cache::size() {
  uint64_t size = 0;
  _state.lock();
  if (isOperational()) {
    _metadata.lock();
    size = _metadata.allocatedSize;
    _metadata.unlock();
  }
  _state.unlock();
  return size;
}

uint64_t Cache::usageLimit() {
  uint64_t limit = 0;
  _state.lock();
  if (isOperational()) {
    _metadata.lock();
    limit = _metadata.softUsageLimit;
    _metadata.unlock();
  }
  _state.unlock();
  return limit;
}

uint64_t Cache::usage() {
  uint64_t usage = 0;
  _state.lock();
  if (isOperational()) {
    _metadata.lock();
    usage = _metadata.usage;
    _metadata.unlock();
  }
  _state.unlock();
  return usage;
}

void Cache::sizeHint(uint64_t numElements) {
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

  uint64_t currentMisses = _findMisses.load();
  uint64_t currentHits = _findHits.load();
  if (currentMisses + currentHits > 0) {
    lifetimeRate = 100 * (static_cast<double>(currentHits) /
                          static_cast<double>(currentHits + currentMisses));
  }

  if (_enableWindowedStats && _findStats.get() != nullptr) {
    auto stats = _findStats->getFrequencies();
    if (stats->size() == 1) {
      if ((*stats)[0].first == static_cast<uint8_t>(Stat::findHit)) {
        windowedRate = 100.0;
      } else {
        windowedRate = 0.0;
      }
    } else if (stats->size() == 2) {
      if ((*stats)[0].first == static_cast<uint8_t>(Stat::findHit)) {
        currentHits = (*stats)[0].second;
        currentMisses = (*stats)[1].second;
      } else {
        currentHits = (*stats)[1].second;
        currentMisses = (*stats)[0].second;
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
  bool resizing = false;
  _state.lock();
  if (isOperational()) {
    _metadata.lock();
    resizing = _metadata.isSet(State::Flag::resizing);
    _metadata.unlock();
    _state.unlock();
  }

  return resizing;
}

bool Cache::isMigrating() {
  bool migrating = false;
  _state.lock();
  if (isOperational()) {
    _metadata.lock();
    migrating = _metadata.isSet(State::Flag::migrating);
    _metadata.unlock();
    _state.unlock();
  }

  return migrating;
}

bool Cache::isShutdown() {
  _state.lock();
  bool shutdown = !isOperational();
  _state.unlock();

  return shutdown;
}

void Cache::destroy(std::shared_ptr<Cache> cache) {
  if (cache.get() != nullptr) {
    cache->shutdown();
  }
}

bool Cache::isOperational() const {
  TRI_ASSERT(_state.isLocked());
  return (!_state.isSet(State::Flag::shutdown) &&
          !_state.isSet(State::Flag::shuttingDown));
}

void Cache::startOperation() { ++_openOperations; }

void Cache::endOperation() { --_openOperations; }

bool Cache::isMigratingLocked() const {
  TRI_ASSERT(_state.isLocked());
  return _state.isSet(State::Flag::migrating);
}

void Cache::requestGrow() {
  bool ok = canResize();
  if (ok) {
    ok = _state.lock(Cache::triesSlow);
    if (ok) {
      if (std::chrono::steady_clock::now() > _resizeRequestTime) {
        _metadata.lock();
        ok = !_metadata.isSet(State::Flag::resizing);
        _metadata.unlock();
        if (ok) {
          std::tie(ok, _resizeRequestTime) =
              _manager->requestGrow(shared_from_this());
        }
      }
      _state.unlock();
    }
  }
}

void Cache::requestMigrate(uint32_t requestedLogSize) {
  bool ok = _state.lock(Cache::triesGuarantee);
  if (ok) {
    if (!isMigratingLocked() &&
        (std::chrono::steady_clock::now() > _migrateRequestTime)) {
      _metadata.lock();
      ok = !_metadata.isSet(State::Flag::migrating) &&
           (requestedLogSize != _table->logSize());
      _metadata.unlock();
      if (ok) {
        std::tie(ok, _resizeRequestTime) =
            _manager->requestMigrate(shared_from_this(), requestedLogSize);
      }
    }
    _state.unlock();
  }
}

void Cache::freeValue(CachedValue* value) {
  while (value->refCount.load() > 0) {
    std::this_thread::yield();
  }

  delete value;
}

bool Cache::reclaimMemory(uint64_t size) {
  _metadata.lock();
  _metadata.adjustUsageIfAllowed(-static_cast<int64_t>(size));
  bool underLimit = (_metadata.softUsageLimit >= _metadata.usage);
  _metadata.unlock();

  return underLimit;
}

uint32_t Cache::hashKey(void const* key, uint32_t keySize) const {
  return (std::max)(static_cast<uint32_t>(1),
                    fasthash32(key, keySize, 0xdeadbeefUL));
}

void Cache::recordStat(Stat stat) {
  switch (stat) {
    case Stat::findHit: {
      _findHits++;
      if (_enableWindowedStats && _findStats.get() != nullptr) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findHit));
      }
      _manager->reportHitStat(Stat::findHit);
      break;
    }
    case Stat::findMiss: {
      _findMisses++;
      if (_enableWindowedStats && _findStats.get() != nullptr) {
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
    _insertEvictions++;
  }
  if (((++_insertsTotal) & _evictionMask) == 0) {
    if (_insertEvictions.load() > _evictionThreshold) {
      shouldMigrate = true;
      bool ok = _state.lock(triesGuarantee);
      if (ok) {
        _table->signalEvictions();
        _state.unlock();
      }
    }
    _insertEvictions = 0;
  }

  return shouldMigrate;
}

Metadata* Cache::metadata() { return &_metadata; }

std::shared_ptr<Table> Cache::table() { return _table; }

void Cache::beginShutdown() {
  _state.lock();
  if (!_state.isSet(State::Flag::shutdown, State::Flag::shuttingDown)) {
    _state.toggleFlag(State::Flag::shuttingDown);
  }
  _state.unlock();
}

void Cache::shutdown() {
  _state.lock();
  auto handle = shared_from_this();  // hold onto self-reference to prevent
                                     // pre-mature shared_ptr destruction
  TRI_ASSERT(handle.get() == this);
  if (!_state.isSet(State::Flag::shutdown)) {
    if (!_state.isSet(State::Flag::shuttingDown)) {
      _state.toggleFlag(State::Flag::shuttingDown);
    }

    while (_openOperations.load() > 0) {
      _state.unlock();
      std::this_thread::yield();
      _state.lock();
    }

    _state.clear();
    _state.toggleFlag(State::Flag::shutdown);
    std::shared_ptr<Table> extra =
        _table->setAuxiliary(std::shared_ptr<Table>(nullptr));
    if (extra.get() != nullptr) {
      extra->clear();
      _manager->reclaimTable(extra);
    }
    _table->clear();
    _manager->reclaimTable(_table);
    _manager->unregisterCache(shared_from_this());
  }
  _metadata.lock();
  _metadata.changeTable(0);
  _metadata.unlock();
  _state.unlock();
}

bool Cache::canResize() {
  bool allowed = _state.lock(Cache::triesSlow);
  if (allowed) {
    if (isOperational()) {
      _metadata.lock();
      if (_metadata.isSet(State::Flag::resizing) ||
          _metadata.isSet(State::Flag::migrating)) {
        allowed = false;
      }
      _metadata.unlock();
    }
    _state.unlock();
  }

  return allowed;
}

bool Cache::canMigrate() {
  bool allowed = (_manager->ioService() != nullptr);
  if (allowed) {
    allowed = _state.lock(Cache::triesSlow);
    if (allowed) {
      if (isOperational()) {
        if (_state.isSet(State::Flag::migrating)) {
          allowed = false;
        } else {
          _metadata.lock();
          if (_metadata.isSet(State::Flag::migrating)) {
            allowed = false;
          }
          _metadata.unlock();
        }
      } else {
        allowed = false;
      }
      _state.unlock();
    }
  }

  return allowed;
}

bool Cache::freeMemory() {
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
    uint64_t reclaimed = freeMemoryFrom(randomHash);

    if (reclaimed > 0) {
      failures = 0;
      underLimit = reclaimMemory(reclaimed);
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

bool Cache::migrate(std::shared_ptr<Table> newTable) {
  _state.lock();
  if (!isOperational()) {
    _state.unlock();
    return false;
  }
  startOperation();
  newTable->setTypeSpecifics(_bucketClearer, _slotsPerBucket);
  newTable->enable();
  _table->setAuxiliary(newTable);
  TRI_ASSERT(!_state.isSet(State::Flag::migrating));
  _state.toggleFlag(State::Flag::migrating);
  _state.unlock();

  // do the actual migration
  for (uint32_t i = 0; i < _table->size(); i++) {
    migrateBucket(_table->primaryBucket(i), _table->auxiliaryBuckets(i),
                  newTable);
  }

  // swap tables
  _state.lock();
  std::shared_ptr<Table> oldTable = _table;
  _table = newTable;
  _state.unlock();

  // clear out old table and release it
  std::shared_ptr<Table> confirm =
      oldTable->setAuxiliary(std::shared_ptr<Table>(nullptr));
  TRI_ASSERT(confirm.get() == newTable.get());
  oldTable->clear();
  _manager->reclaimTable(oldTable);

  // unmarking migrating flags
  _state.lock();
  _state.toggleFlag(State::Flag::migrating);
  _state.unlock();
  _metadata.lock();
  _metadata.changeTable(_table->memoryUsage());
  _metadata.toggleFlag(State::Flag::migrating);
  _metadata.unlock();

  endOperation();
  return true;
}
