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

#include "Cache/Manager.h"
#include "Basics/Common.h"
#include "Basics/SharedPRNG.h"
#include "Basics/asio-helper.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/PlainCache.h"
#include "Cache/Table.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionalCache.h"
#include "Logger/Logger.h"

#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <set>
#include <stack>
#include <utility>

using namespace arangodb::cache;

const uint64_t Manager::minSize = 1024 * 1024;
const uint64_t Manager::minCacheAllocation =
    Cache::minSize + Table::allocationSize(Table::minLogSize) +
    std::max(PlainCache::allocationSize(true),
             TransactionalCache::allocationSize(true)) +
    Manager::cacheRecordOverhead;
const std::chrono::milliseconds Manager::rebalancingGracePeriod(10);

bool Manager::cmp_weak_ptr::operator()(
    std::weak_ptr<Cache> const& left, std::weak_ptr<Cache> const& right) const {
  return !left.owner_before(right) && !right.owner_before(left);
}

size_t Manager::hash_weak_ptr::operator()(
    const std::weak_ptr<Cache>& wp) const {
  auto sp = wp.lock();
  return std::hash<decltype(sp)>()(sp);
}

Manager::Manager(boost::asio::io_service* ioService, uint64_t globalLimit,
                 bool enableWindowedStats)
    : _lock(),
      _shutdown(false),
      _shuttingDown(false),
      _resizing(false),
      _rebalancing(false),
      _accessStats((globalLimit >= (1024 * 1024 * 1024))
                       ? ((1024 * 1024) / sizeof(std::weak_ptr<Cache>))
                       : (globalLimit / (1024 * sizeof(std::weak_ptr<Cache>)))),
      _enableWindowedStats(enableWindowedStats),
      _findStats(nullptr),
      _findHits(),
      _findMisses(),
      _caches(),
      _globalSoftLimit(globalLimit),
      _globalHardLimit(globalLimit),
      _globalHighwaterMark(
          static_cast<uint64_t>(Manager::highwaterMultiplier *
                                static_cast<double>(_globalSoftLimit))),
      _fixedAllocation(sizeof(Manager) + Manager::tableListsOverhead +
                       _accessStats.memoryUsage()),
      _spareTableAllocation(0),
      _globalAllocation(_fixedAllocation),
      _transactions(),
      _ioService(ioService),
      _resizeAttempt(0),
      _outstandingTasks(0),
      _rebalancingTasks(0),
      _resizingTasks(0),
      _rebalanceCompleted(std::chrono::steady_clock::now() -
                          Manager::rebalancingGracePeriod) {
  TRI_ASSERT(_globalAllocation < _globalSoftLimit);
  TRI_ASSERT(_globalAllocation < _globalHardLimit);
  if (enableWindowedStats) {
    try {
      _findStats.reset(new Manager::FindStatBuffer(16384));
      _fixedAllocation += _findStats->memoryUsage();
      _globalAllocation = _fixedAllocation;
    } catch (std::bad_alloc) {
      _findStats.reset(nullptr);
      _enableWindowedStats = false;
    }
  }
}

Manager::~Manager() { shutdown(); }

std::shared_ptr<Cache> Manager::createCache(CacheType type,
                                            bool enableWindowedStats,
                                            uint64_t maxSize) {
  std::shared_ptr<Cache> result(nullptr);
  _lock.writeLock();
  bool allowed = isOperational();
  Metadata metadata;
  std::shared_ptr<Table> table(nullptr);

  if (allowed) {
    uint64_t fixedSize = 0;
    switch (type) {
      case CacheType::Plain:
        fixedSize = PlainCache::allocationSize(enableWindowedStats);
        break;
      case CacheType::Transactional:
        fixedSize = TransactionalCache::allocationSize(enableWindowedStats);
        break;
      default:
        break;
    }
    std::tie(allowed, metadata, table) = registerCache(fixedSize, maxSize);
  }

  if (allowed) {
    switch (type) {
      case CacheType::Plain:
        result = PlainCache::create(this, metadata, table, enableWindowedStats);
        break;
      case CacheType::Transactional:
        result = TransactionalCache::create(this, metadata, table,
                                            enableWindowedStats);
        break;
      default:
        break;
    }
  }

  if (result.get() != nullptr) {
    _caches.emplace(result);
  }
  _lock.writeUnlock();

  return result;
}

void Manager::destroyCache(std::shared_ptr<Cache> cache) {
  Cache::destroy(cache);
}

void Manager::beginShutdown() {
  _lock.writeLock();
  if (!_shutdown) {
    _shuttingDown = true;
  }
  _lock.writeUnlock();
}

void Manager::shutdown() {
  _lock.writeLock();
  if (!_shutdown) {
    if (!_shuttingDown) {
      _shuttingDown = true;
    }
    while (!_caches.empty()) {
      std::shared_ptr<Cache> cache = *_caches.begin();
      _lock.writeUnlock();
      cache->shutdown();
      _lock.writeLock();
    }
    freeUnusedTables();
    _shutdown = true;
  }
  _lock.writeUnlock();
}

// change global cache limit
bool Manager::resize(uint64_t newGlobalLimit) {
  _lock.writeLock();
  if ((newGlobalLimit < Manager::minSize) ||
      (static_cast<uint64_t>(0.5 * (1.0 - Manager::highwaterMultiplier) *
                             static_cast<double>(newGlobalLimit)) <
       _fixedAllocation) ||
      (static_cast<uint64_t>(Manager::highwaterMultiplier *
                             static_cast<double>(newGlobalLimit)) <
       (_caches.size() * Manager::minCacheAllocation))) {
    _lock.writeUnlock();
    return false;
  }
  
  bool success = true;
  if (!isOperational() || globalProcessRunning()) {
    // shut(ting) down or still have another global process running already
    success = false;
  } else {
    bool done = adjustGlobalLimitsIfAllowed(newGlobalLimit);
    if (!done) {
      // otherwise we need to actually resize
      _resizing = true;
      _globalSoftLimit = newGlobalLimit;
      _globalHighwaterMark = static_cast<uint64_t>(
          Manager::highwaterMultiplier * static_cast<double>(_globalSoftLimit));
      freeUnusedTables();
      done = adjustGlobalLimitsIfAllowed(newGlobalLimit);
      if (!done) {
        rebalance(true);
        shrinkOvergrownCaches(TaskEnvironment::resizing);
      }
    }
  }

  _lock.writeUnlock();
  return success;
}

uint64_t Manager::globalLimit() {
  _lock.readLock();
  uint64_t limit = _resizing ? _globalSoftLimit : _globalHardLimit;
  _lock.readUnlock();

  return limit;
}

uint64_t Manager::globalAllocation() {
  _lock.readLock();
  uint64_t allocation = _globalAllocation;
  _lock.readUnlock();

  return allocation;
}

std::pair<double, double> Manager::globalHitRates() {
  double lifetimeRate = std::nan("");
  double windowedRate = std::nan("");

  uint64_t currentHits = _findHits.value();
  uint64_t currentMisses = _findMisses.value();
  if (currentHits + currentMisses > 0) {
    lifetimeRate = 100.0 * (static_cast<double>(currentHits) /
                            static_cast<double>(currentHits + currentMisses));
  }

  if (_enableWindowedStats && _findStats.get() != nullptr) {
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
        windowedRate =
            100.0 * (static_cast<double>(currentHits) /
                     static_cast<double>(currentHits + currentMisses));
      }
    }
  }

  return std::make_pair(lifetimeRate, windowedRate);
}

Transaction* Manager::beginTransaction(bool readOnly) {
  return _transactions.begin(readOnly);
}

void Manager::endTransaction(Transaction* tx) { _transactions.end(tx); }

std::tuple<bool, Metadata, std::shared_ptr<Table>> Manager::registerCache(
    uint64_t fixedSize, uint64_t maxSize) {
  TRI_ASSERT(_lock.isWriteLocked());
  Metadata metadata;
  std::shared_ptr<Table> table(nullptr);
  bool ok = true;

  if ((_globalHighwaterMark / (_caches.size() + 1)) <
      Manager::minCacheAllocation) {
    ok = false;
  }

  if (ok) {
    table = leaseTable(Table::minLogSize);
    ok = (table.get() != nullptr);
  }

  if (ok) {
    metadata =
        Metadata(Cache::minSize, fixedSize, table->memoryUsage(), maxSize);
    ok = increaseAllowed(metadata.allocatedSize - table->memoryUsage(), true);
    if (ok) {
      _globalAllocation += (metadata.allocatedSize - table->memoryUsage());
    }
  }

  if (!ok && (table.get() != nullptr)) {
    reclaimTable(table, true);
    table.reset();
  }

  return std::make_tuple(ok, metadata, table);
}

void Manager::unregisterCache(std::shared_ptr<Cache> cache) {
  _lock.writeLock();
  Metadata* metadata = cache->metadata();
  metadata->readLock();
  _globalAllocation -= metadata->allocatedSize;
  metadata->readUnlock();
  _caches.erase(cache);
  _lock.writeUnlock();
}

std::pair<bool, Manager::time_point> Manager::requestGrow(
    std::shared_ptr<Cache> cache) {
  Manager::time_point nextRequest = futureTime(100);
  bool allowed = false;

  bool ok = _lock.writeLock(Manager::triesSlow);
  if (ok) {
    if (isOperational() && !globalProcessRunning()) {
      Metadata* metadata = cache->metadata();
      metadata->writeLock();

      allowed = !metadata->isResizing() &&
                !metadata->isMigrating();
      if (allowed) {
        if (metadata->allocatedSize >= metadata->deservedSize &&
            pastRebalancingGracePeriod()) {
          uint64_t increase =
              std::min(metadata->hardUsageLimit / 2,
                       metadata->maxSize - metadata->allocatedSize);
          if (increase > 0 && increaseAllowed(increase)) {
            uint64_t newLimit = metadata->allocatedSize + increase;
            metadata->adjustDeserved(newLimit);
          } else {
            allowed = false;
          }
        }

        if (allowed) {
          nextRequest = std::chrono::steady_clock::now();
          resizeCache(TaskEnvironment::none, cache,
                      metadata->newLimit());  // unlocks metadata
        }
      }

      if (!allowed) {
        metadata->writeUnlock();
      }
    }
    _lock.writeUnlock();
  }

  return std::make_pair(allowed, nextRequest);
}

std::pair<bool, Manager::time_point> Manager::requestMigrate(
    std::shared_ptr<Cache> cache, uint32_t requestedLogSize) {
  Manager::time_point nextRequest = futureTime(100);
  bool allowed = false;

  bool ok = _lock.writeLock(Manager::triesSlow);
  if (ok) {
    if (isOperational() && !globalProcessRunning()) {
      Metadata* metadata = cache->metadata();
      metadata->writeLock();

      allowed = !metadata->isMigrating();
      if (allowed) {
        if (metadata->tableSize < Table::allocationSize(requestedLogSize)) {
          uint64_t increase =
              Table::allocationSize(requestedLogSize) - metadata->tableSize;
          if ((metadata->allocatedSize + increase >= metadata->deservedSize) &&
              pastRebalancingGracePeriod()) {
            if (increaseAllowed(increase)) {
              uint64_t newLimit = metadata->allocatedSize + increase;
              uint64_t granted = metadata->adjustDeserved(newLimit);
              if (granted < newLimit) {
                allowed = false;
              }
            } else {
              allowed = false;
            }
          }
        }
      }

      if (allowed) {
        // first find out if cache is allowed to migrate
        allowed =
            metadata->migrationAllowed(Table::allocationSize(requestedLogSize));
      }
      if (allowed) {
        // now find out if we can lease the table
        std::shared_ptr<Table> table = leaseTable(requestedLogSize);
        allowed = (table.get() != nullptr);
        if (allowed) {
          nextRequest = std::chrono::steady_clock::now();
          migrateCache(TaskEnvironment::none, cache,
                       table);  // unlocks metadata
        }
      }

      if (!allowed) {
        metadata->writeUnlock();
      }
    }
    _lock.writeUnlock();
  }

  return std::make_pair(allowed, nextRequest);
}

void Manager::reportAccess(std::shared_ptr<Cache> cache) {
  if ((basics::SharedPRNG::rand() & static_cast<unsigned long>(7)) == 0) {
    _accessStats.insertRecord(cache);
  }
}

void Manager::reportHitStat(Stat stat) {
  switch (stat) {
    case Stat::findHit: {
      _findHits.add(1);
      if (_enableWindowedStats && _findStats.get() != nullptr) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findHit));
      }
      break;
    }
    case Stat::findMiss: {
      _findMisses.add(1);
      if (_enableWindowedStats && _findStats.get() != nullptr) {
        _findStats->insertRecord(static_cast<uint8_t>(Stat::findMiss));
      }
      break;
    }
    default: { break; }
  }
}

bool Manager::isOperational() const {
  TRI_ASSERT(_lock.isLocked());
  return (!_shutdown && !_shuttingDown);
}

bool Manager::globalProcessRunning() const {
  TRI_ASSERT(_lock.isLocked());
  return (_rebalancing || _resizing);
}

boost::asio::io_service* Manager::ioService() { return _ioService; }

void Manager::prepareTask(Manager::TaskEnvironment environment) {
  _outstandingTasks++;
  switch (environment) {
    case TaskEnvironment::rebalancing: {
      _rebalancingTasks++;
      break;
    }
    case TaskEnvironment::resizing: {
      _resizingTasks++;
      break;
    }
    case TaskEnvironment::none:
    default: { break; }
  }
}

void Manager::unprepareTask(Manager::TaskEnvironment environment) {
  switch (environment) {
    case TaskEnvironment::rebalancing: {
      if ((--_rebalancingTasks) == 0) {
        _lock.writeLock();
        _rebalancing = false;
        _rebalanceCompleted = std::chrono::steady_clock::now();
        _lock.writeUnlock();
      };
      break;
    }
    case TaskEnvironment::resizing: {
      if ((--_resizingTasks) == 0) {
        _lock.writeLock();
        _resizing = false;
        _lock.writeUnlock();
      };
      break;
    }
    case TaskEnvironment::none:
    default: { break; }
  }

  _outstandingTasks--;
}

bool Manager::rebalance(bool onlyCalculate) {
  if (!onlyCalculate) {
    _lock.writeLock();
    if (_caches.size() == 0
        || !isOperational()
        || globalProcessRunning()) {
      _lock.writeUnlock();
      return false;
    }

    // start rebalancing
    _rebalancing = true;
  }

  // adjust deservedSize for each cache
  std::shared_ptr<PriorityList> cacheList = priorityList();
  std::shared_ptr<Cache> cache;
  double weight;
  for (auto pair : (*cacheList)) {
    std::tie(cache, weight) = pair;
    uint64_t newDeserved = static_cast<uint64_t>(
        std::ceil(weight * static_cast<double>(_globalHighwaterMark)));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if (newDeserved < Manager::minCacheAllocation) {
      LOG_TOPIC(FATAL, Logger::FIXME)
          << "Deserved limit of " << newDeserved << " from weight " << weight
          << " and highwater " << _globalHighwaterMark
          << ". Should be at least " << Manager::minCacheAllocation;
      TRI_ASSERT(newDeserved >= Manager::minCacheAllocation);
    }
#endif
    Metadata* metadata = cache->metadata();
    metadata->writeLock();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    uint64_t fixed = metadata->fixedSize + metadata->tableSize + Manager::cacheRecordOverhead;
    if (newDeserved < fixed) {
      LOG_TOPIC(ERR, Logger::FIXME) << "Setting deserved cache size " << newDeserved << " below usage: " << fixed
      << " ; Using weight  " << weight;
    }
#endif
    metadata->adjustDeserved(newDeserved);
    metadata->writeUnlock();
  }

  if (!onlyCalculate) {
    if (_globalAllocation >= _globalHighwaterMark * 0.7) {
      shrinkOvergrownCaches(TaskEnvironment::rebalancing);
    }

    if (_rebalancingTasks.load() == 0) {
      _rebalanceCompleted = std::chrono::steady_clock::now();
      _rebalancing = false;
    }

    _lock.writeUnlock();
  }

  return true;
}

void Manager::shrinkOvergrownCaches(Manager::TaskEnvironment environment) {
  TRI_ASSERT(_lock.isWriteLocked());
  for (std::shared_ptr<Cache> cache : _caches) {
    // skip this cache if it is already resizing or shutdown!
    if (!cache->canResize()) {
      continue;
    }

    Metadata* metadata = cache->metadata();
    metadata->writeLock();

    if (metadata->allocatedSize > metadata->deservedSize) {
      resizeCache(environment, cache, metadata->newLimit()); // unlocks metadata
    } else {
      metadata->writeUnlock();
    }
  }
}

void Manager::freeUnusedTables() {
  TRI_ASSERT(_lock.isWriteLocked());
  for (size_t i = 0; i < 32; i++) {
    while (!_tables[i].empty()) {
      auto table = _tables[i].top();
      _globalAllocation -= table->memoryUsage();
      _tables[i].pop();
    }
  }
}

bool Manager::adjustGlobalLimitsIfAllowed(uint64_t newGlobalLimit) {
  TRI_ASSERT(_lock.isWriteLocked());
  if (newGlobalLimit < _globalAllocation) {
    return false;
  }

  _globalHighwaterMark = static_cast<uint64_t>(
      Manager::highwaterMultiplier * static_cast<double>(newGlobalLimit));
  _globalSoftLimit = newGlobalLimit;
  _globalHardLimit = newGlobalLimit;

  return true;
}

void Manager::resizeCache(Manager::TaskEnvironment environment,
                          std::shared_ptr<Cache> cache, uint64_t newLimit) {
  TRI_ASSERT(_lock.isWriteLocked());
  Metadata* metadata = cache->metadata();
  TRI_ASSERT(metadata->isWriteLocked());

  if (metadata->usage <= newLimit) {
    uint64_t oldLimit = metadata->hardUsageLimit;
    bool success = metadata->adjustLimits(newLimit, newLimit);
    TRI_ASSERT(success);
    metadata->writeUnlock();
    if (oldLimit > newLimit) {
      _globalAllocation -= (oldLimit - newLimit);
    } else {
      _globalAllocation += (newLimit - oldLimit);
    }
    return;
  }
  
  bool success = metadata->adjustLimits(newLimit, metadata->hardUsageLimit);
  TRI_ASSERT(success);
  TRI_ASSERT(!metadata->isResizing());
  metadata->toggleResizing();
  metadata->writeUnlock();

  auto task = std::make_shared<FreeMemoryTask>(environment, this, cache);
  bool dispatched = task->dispatch();
  if (!dispatched) {
    // TODO: decide what to do if we don't have an io_service
    metadata->writeLock();
    metadata->toggleResizing();
    metadata->writeUnlock();
  }
}

void Manager::migrateCache(Manager::TaskEnvironment environment,
                           std::shared_ptr<Cache> cache,
                           std::shared_ptr<Table> table) {
  TRI_ASSERT(_lock.isWriteLocked());
  Metadata* metadata = cache->metadata();
  TRI_ASSERT(metadata->isWriteLocked());

  TRI_ASSERT(!metadata->isMigrating());
  metadata->toggleMigrating();
  metadata->writeUnlock();

  auto task = std::make_shared<MigrateTask>(environment, this, cache, table);
  bool dispatched = task->dispatch();
  if (!dispatched) {
    // TODO: decide what to do if we don't have an io_service
    metadata->writeLock();
    reclaimTable(table, true);
    metadata->toggleMigrating();
    metadata->writeUnlock();
  }
}

std::shared_ptr<Table> Manager::leaseTable(uint32_t logSize) {
  TRI_ASSERT(_lock.isWriteLocked());

  std::shared_ptr<Table> table(nullptr);
  if (_tables[logSize].empty()) {
    if (increaseAllowed(Table::allocationSize(logSize), true)) {
      try {
        table = std::make_shared<Table>(logSize);
        _globalAllocation += table->memoryUsage();
      } catch (std::bad_alloc) {
        table.reset();
      }
    }
  } else {
    table = _tables[logSize].top();
    _spareTableAllocation -= table->memoryUsage();
    _tables[logSize].pop();
  }

  return table;
}

void Manager::reclaimTable(std::shared_ptr<Table> table, bool internal) {
  TRI_ASSERT(table.get() != nullptr);
  if (!internal) {
    _lock.writeLock();
  }

  uint32_t logSize = table->logSize();
  size_t maxTables = (logSize < 18) ? (1 << (18 - logSize)) : 1;
  if ((_tables[logSize].size() < maxTables) &&
      ((table->memoryUsage() + _spareTableAllocation) <
       ((_globalSoftLimit - _globalHighwaterMark) / 2))) {
    _tables[logSize].emplace(table);
    _spareTableAllocation += table->memoryUsage();
  } else {
    _globalAllocation -= table->memoryUsage();
    table.reset();
  }

  if (!internal) {
    _lock.writeUnlock();
  }
}

bool Manager::increaseAllowed(uint64_t increase, bool privileged) const {
  TRI_ASSERT(_lock.isLocked());
  if (privileged) {
    if (_resizing &&
        (_globalAllocation <= _globalSoftLimit)) {
      return (increase <= (_globalSoftLimit - _globalAllocation));
    }

    return (increase <= (_globalHardLimit - _globalAllocation));
  }

  return (increase <= (_globalHighwaterMark - _globalAllocation));
}

std::shared_ptr<Manager::PriorityList> Manager::priorityList() {
  TRI_ASSERT(_lock.isWriteLocked());
  double minimumWeight = static_cast<double>(Manager::minCacheAllocation) /
  static_cast<double>(_globalHighwaterMark);
  while (static_cast<uint64_t>(std::ceil(minimumWeight * static_cast<double>(_globalHighwaterMark))) <
         Manager::minCacheAllocation) {
    minimumWeight *= 1.001;  // bump by 0.1% until we fix precision issues
  }

  double uniformMarginalWeight = 0.2 / static_cast<double>(_caches.size());
  LOG_TOPIC(ERR, Logger::FIXME) << "uniformMarginalWeight " << uniformMarginalWeight;
  double baseWeight = std::max(minimumWeight, uniformMarginalWeight);
  LOG_TOPIC(ERR, Logger::FIXME) << "baseWeight " << baseWeight;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (1.0 < (baseWeight * static_cast<double>(_caches.size()))) {
    LOG_TOPIC(FATAL, Logger::FIXME)
    << "weight: " << baseWeight << ", count: " << _caches.size();
    TRI_ASSERT(1.0 >= (baseWeight * static_cast<double>(_caches.size())));
  }
#endif
  double remainingWeight =
  1.0 - (baseWeight * static_cast<double>(_caches.size()));

  std::shared_ptr<PriorityList> list(new PriorityList());
  list->reserve(_caches.size());

  // catalog accessed caches and count total accesses
  // to get basis for comparison
  std::vector<std::pair<std::weak_ptr<Cache>, uint64_t>> stats = _accessStats.getFrequencies();
  std::set<std::shared_ptr<Cache>> accessed;
  uint64_t totalAccesses = 0;
  uint64_t globalUsage = 0;
  for (auto const& s : stats) {
    totalAccesses += s.second;
    if (auto cache = s.first.lock()) {
      accessed.emplace(cache);
    }
  }
  totalAccesses = std::max(static_cast<uint64_t>(1), totalAccesses);

  double allocFrac = 1.0 - std::max(1.0, static_cast<double>(_globalAllocation) / static_cast<double>(_globalHighwaterMark));
  LOG_TOPIC(ERR, Logger::FIXME) << "Allocated fraction " <<  allocFrac << "%";

  // calculate global data usage
  for (auto it = _caches.begin(); it != _caches.end(); it++) {
    globalUsage += (*it)->usage();
  }
  globalUsage = std::max(globalUsage, static_cast<uint64_t>(1)); // avoid /0

  // gather all unaccessed caches at beginning of list
  for (auto it = _caches.begin(); it != _caches.end(); it++) {
    auto found = accessed.find(*it);
    if (found == accessed.end()) {
      double weight = baseWeight + ((*it)->usage() / globalUsage) * allocFrac;
      //LOG_TOPIC(ERR, Logger::FIXME) << "Cache (" << ((size_t)it->get()) << ") weight: " << weight;
      list->emplace_back(*it, weight);
    }
  }

  double normalizer =
  remainingWeight / static_cast<double>(totalAccesses);

  // gather all accessed caches in order
  for (auto s : stats) {
    if (auto cache = s.first.lock()) {
      double accessWeight = static_cast<double>(s.second) * normalizer * (1.0 - allocFrac);
      double usageWeight = (cache->usage() / globalUsage) * allocFrac;

      TRI_ASSERT(accessWeight >= 0.0);
      TRI_ASSERT(usageWeight >= 0.0);
      LOG_TOPIC(ERR, Logger::FIXME) << "Cache (" << ((size_t)cache.get()) << ") accesses "<< s.second
      << " weight: " << (baseWeight + accessWeight + usageWeight);
      list->emplace_back(cache, (baseWeight + accessWeight + usageWeight));
    }
  }

  return list;
}

Manager::time_point Manager::futureTime(uint64_t millisecondsFromNow) {
  return (std::chrono::steady_clock::now() +
          std::chrono::milliseconds(millisecondsFromNow));
}

bool Manager::pastRebalancingGracePeriod() const {
  TRI_ASSERT(_lock.isLocked());
  bool ok = !_rebalancing;
  if (ok) {
    ok = (std::chrono::steady_clock::now() - _rebalanceCompleted) >=
         Manager::rebalancingGracePeriod;
  }

  return ok;
}
