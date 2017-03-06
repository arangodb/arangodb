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
#include "Cache/Manager.h"
#include "Cache/Metadata.h"
#include "Cache/State.h"

#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <list>
#include <thread>

using namespace arangodb::cache;

uint64_t Cache::_evictionStatsCapacity = 1024;
uint64_t Cache::_findStatsCapacity = 16384;

Cache::ConstructionGuard::ConstructionGuard() {}

Cache::Finding::Finding(CachedValue* v) : _value(v) {
  if (_value != nullptr) {
    _value->lease();
  }
}

Cache::Finding::Finding(Finding const& other) : _value(other._value) {
  if (_value != nullptr) {
    _value->lease();
  }
}

Cache::Finding::Finding(Finding&& other) : _value(other._value) {
  other._value = nullptr;
}

Cache::Finding& Cache::Finding::operator=(Finding const& other) {
  if (&other == this) {
    return *this;
  }

  if (_value != nullptr) {
    _value->release();
  }

  _value = other._value;
  if (_value != nullptr) {
    _value->lease();
  }

  return *this;
}

Cache::Finding& Cache::Finding::operator=(Finding&& other) {
  if (&other == this) {
    return *this;
  }

  if (_value != nullptr) {
    _value->release();
  }

  _value = other._value;
  other._value = nullptr;

  return *this;
}

Cache::Finding::~Finding() {
  if (_value != nullptr) {
    _value->release();
  }
}

void Cache::Finding::reset(CachedValue* v) {
  if (_value != nullptr) {
    _value->release();
  }

  _value = v;
  if (_value != nullptr) {
    _value->lease();
  }
}

bool Cache::Finding::found() const { return (_value != nullptr); }

CachedValue const* Cache::Finding::value() const { return _value; }

CachedValue* Cache::Finding::copy() const {
  return ((_value == nullptr) ? nullptr : _value->copy());
}

Cache::Cache(ConstructionGuard guard, Manager* manager,
             Manager::MetadataItr metadata, bool allowGrowth,
             bool enableWindowedStats)
    : _state(),
      _allowGrowth(allowGrowth),
      _evictionStats(_evictionStatsCapacity),
      _insertionCount(0),
      _enableWindowedStats(enableWindowedStats),
      _findStats(nullptr),
      _findHits(0),
      _findMisses(0),
      _manager(manager),
      _metadata(metadata),
      _openOperations(0),
      _migrateRequestTime(std::chrono::steady_clock::now()),
      _resizeRequestTime(std::chrono::steady_clock::now()),
      _lastResizeRequestStatus(true) {
  if (_enableWindowedStats) {
    try {
      _findStats.reset(new StatBuffer(_findStatsCapacity));
    } catch (std::bad_alloc) {
      _findStats.reset(nullptr);
      _enableWindowedStats = false;
    }
  }
}

uint64_t Cache::limit() {
  uint64_t limit = 0;
  _state.lock();
  if (isOperational()) {
    _metadata->lock();
    limit = _metadata->softLimit();
    _metadata->unlock();
  }
  _state.unlock();
  return limit;
}

uint64_t Cache::usage() {
  uint64_t usage = 0;
  _state.lock();
  if (isOperational()) {
    _metadata->lock();
    usage = _metadata->usage();
    _metadata->unlock();
  }
  _state.unlock();
  return usage;
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

void Cache::disableGrowth() {
  _state.lock();
  _allowGrowth = false;
  _state.unlock();
}

void Cache::enableGrowth() {
  _state.lock();
  _allowGrowth = false;
  _state.unlock();
}

bool Cache::resize(uint64_t requestedLimit) {
  _state.lock();
  bool allowed = isOperational();
  bool resized = false;
  startOperation();
  _state.unlock();

  if (allowed) {
    // wait for previous resizes to finish
    while (true) {
      _metadata->lock();
      if (!_metadata->isSet(State::Flag::resizing)) {
        _metadata->unlock();
        break;
      }
      _metadata->unlock();
    }

    resized = requestResize(requestedLimit, false);
  }

  endOperation();
  return resized;
}

bool Cache::isResizing() {
  bool resizing = false;
  _state.lock();
  if (isOperational()) {
    _metadata->lock();
    resizing = _metadata->isSet(State::Flag::resizing);
    _metadata->unlock();
    _state.unlock();
  }

  return resizing;
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

bool Cache::isMigrating() const {
  TRI_ASSERT(_state.isLocked());
  return _state.isSet(State::Flag::migrating);
}

bool Cache::requestResize(uint64_t requestedLimit, bool internal) {
  bool resized = false;
  int64_t lockTries = internal ? 10LL : -1LL;
  bool ok = _state.lock(lockTries);
  if (ok) {
    if (!internal || (_allowGrowth && (std::chrono::steady_clock::now() >
                                       _resizeRequestTime))) {
      _metadata->lock();
      uint64_t newLimit =
          (requestedLimit > 0)
              ? requestedLimit
              : (_lastResizeRequestStatus
                     ? (_metadata->hardLimit() * 2)
                     : (static_cast<uint64_t>(
                           static_cast<double>(_metadata->hardLimit()) *
                           1.25)));
      _metadata->unlock();
      auto result = _manager->requestResize(_metadata, newLimit);
      _resizeRequestTime = result.second;
      resized = result.first;
    }
    _state.unlock();
  }
  return resized;
}

void Cache::requestMigrate(uint32_t requestedLogSize) {
  if ((++_insertionCount & 0xFFF) == 0) {
    auto stats = _evictionStats.getFrequencies();
    if (((stats->size() == 1) &&
         ((*stats)[0].first == static_cast<uint8_t>(Stat::insertEviction))) ||
        ((stats->size() == 2) &&
         (((*stats)[0].first == static_cast<uint8_t>(Stat::insertNoEviction)) ||
          ((*stats)[0].second * 16 > (*stats)[1].second)))) {
      bool ok = _state.lock(10LL);
      if (ok) {
        if (!isMigrating() &&
            (std::chrono::steady_clock::now() > _migrateRequestTime)) {
          _metadata->lock();
          uint32_t newLogSize = (requestedLogSize > 0)
                                    ? requestedLogSize
                                    : (_metadata->logSize() + 1);
          _metadata->unlock();
          auto result = _manager->requestMigrate(_metadata, newLogSize);
          _resizeRequestTime = result.second;
          if (result.first) {
            _evictionStats.clear();
          }
        }
        _state.unlock();
      }
    }
  }
}

void Cache::freeValue(CachedValue* value) {
  while (value->refCount.load() > 0) {
    std::this_thread::yield();
  }

  delete value;
}

bool Cache::reclaimMemory(uint64_t size) {
  _metadata->lock();
  _metadata->adjustUsageIfAllowed(-static_cast<int64_t>(size));
  bool underLimit = (_metadata->softLimit() >= _metadata->usage());
  _metadata->unlock();

  return underLimit;
}

uint32_t Cache::hashKey(void const* key, uint32_t keySize) const {
  return (std::max)(static_cast<uint32_t>(1),
                    fasthash32(key, keySize, 0xdeadbeefUL));
}

void Cache::recordStat(Cache::Stat stat) {
  switch (stat) {
    case Stat::insertEviction:
    case Stat::insertNoEviction: {
      _evictionStats.insertRecord(static_cast<uint8_t>(stat));
      break;
    }
    case Stat::findHit: {
      _findHits++;
      if (_enableWindowedStats && _findStats.get() != nullptr) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findHit));
      }
      _manager->recordHitStat(Manager::Stat::findHit);
      break;
    }
    case Stat::findMiss: {
      _findMisses++;
      if (_enableWindowedStats && _findStats.get() != nullptr) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findMiss));
      }
      _manager->recordHitStat(Manager::Stat::findMiss);
      break;
    }
    default: { break; }
  }
}

Manager::MetadataItr& Cache::metadata() { return _metadata; }

void Cache::beginShutdown() {
  _state.lock();
  if (!_state.isSet(State::Flag::shutdown) &&
      !_state.isSet(State::Flag::shuttingDown)) {
    _state.toggleFlag(State::Flag::shuttingDown);
  }
  _state.unlock();
}

void Cache::shutdown() {
  _state.lock();
  _metadata->lock();
  auto handle = _metadata->cache();  // hold onto self-reference to prevent
                                     // pre-mature shared_ptr destruction
  TRI_ASSERT(handle.get() == this);
  _metadata->unlock();
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
    clearTables();
    _manager->unregisterCache(_metadata);
  }
  _state.unlock();
}

bool Cache::canResize() {
  bool allowed = true;
  _state.lock();
  if (isOperational()) {
    _metadata->lock();
    if (_metadata->isSet(State::Flag::resizing)) {
      allowed = false;
    }
    _metadata->unlock();
  } else {
    allowed = false;
  }
  _state.unlock();

  return allowed;
}

bool Cache::canMigrate() {
  bool allowed = true;
  _state.lock();
  if (isOperational()) {
    _metadata->lock();
    if (_metadata->isSet(State::Flag::migrating)) {
      allowed = false;
    }
    _metadata->unlock();
  } else {
    allowed = false;
  }
  _state.unlock();

  return allowed;
}
