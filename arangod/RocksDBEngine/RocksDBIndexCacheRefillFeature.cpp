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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBIndexCacheRefillFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/application-exit.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/IRegistry.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillThread.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;

DECLARE_COUNTER(rocksdb_cache_full_index_refills_total,
                "Total number of completed full index cache refills");

RocksDBIndexCacheRefillFeature::RocksDBIndexCacheRefillFeature(
    application_features::ApplicationServer& server,
    DatabaseFeature& databaseFeature, ClusterFeature* clusterFeature,
    metrics::IRegistry& metricsRegistry)
    : RocksDBIndexCacheRefillFeature(server, databaseFeature, clusterFeature,
                                     metricsRegistry,
                                     RocksDBIndexCacheRefillFeatureOptions{}) {}

RocksDBIndexCacheRefillFeature::RocksDBIndexCacheRefillFeature(
    application_features::ApplicationServer& server,
    DatabaseFeature& databaseFeature, ClusterFeature* clusterFeature,
    metrics::IRegistry& metricsRegistry,
    RocksDBIndexCacheRefillFeatureOptions options)
    : application_features::ApplicationFeature{server, *this},
      _databaseFeature(databaseFeature),
      _clusterFeature(clusterFeature),
      _metricsRegistry(metricsRegistry),
      _options(std::move(options)),
      _totalFullIndexRefills(addTotalFullIndexRefills(metricsRegistry)),
      _currentlyRunningIndexFillTasks(0) {
  setOptional(true);
  // we want to be late in the startup sequence
  startsAfter<BootstrapFeature>();
  startsAfter<DatabaseFeature>();
  startsAfter<RocksDBEngine>();

  // default value must be at least 1, as the minimum allowed value is also 1.
  TRI_ASSERT(_options.maxConcurrentIndexFillTasks >= 1);
}

RocksDBIndexCacheRefillFeature::~RocksDBIndexCacheRefillFeature() {
  stopThread();
}

void RocksDBIndexCacheRefillFeature::beginShutdown() {
  {
    std::unique_lock lock(_indexFillTasksMutex);
    _indexFillTasks.clear();
  }
  if (_refillThread != nullptr) {
    _refillThread->beginShutdown();
  }
}

void RocksDBIndexCacheRefillFeature::start() {
  if (ServerState::instance()->isCoordinator()) {
    // we don't have in-memory caches for indexes on the coordinator
    return;
  }

  _refillThread = std::make_unique<RocksDBIndexCacheRefillThread>(
      _databaseFeature, _metricsRegistry, _options.maxCapacity);

  if (!_refillThread->start()) {
    LOG_TOPIC("836a6", FATAL, Logger::ENGINES)
        << "could not start rocksdb index cache refill thread";
    FATAL_ERROR_EXIT();
  }

  if (_options.fillOnStartup) {
    buildStartupIndexRefillTasks();
    scheduleIndexRefillTasks();
  }
}

void RocksDBIndexCacheRefillFeature::stop() { stopThread(); }

bool RocksDBIndexCacheRefillFeature::autoRefill() const noexcept {
  return _options.autoRefill;
}

bool RocksDBIndexCacheRefillFeature::autoRefillOnFollowers() const noexcept {
  return _options.autoRefillOnFollowers;
}

size_t RocksDBIndexCacheRefillFeature::maxCapacity() const noexcept {
  return _options.maxCapacity;
}

bool RocksDBIndexCacheRefillFeature::fillOnStartup() const noexcept {
  return _options.fillOnStartup;
}

void RocksDBIndexCacheRefillFeature::trackRefill(
    std::shared_ptr<LogicalCollection> const& collection, IndexId iid,
    std::vector<std::string> keys) {
  if (_refillThread != nullptr) {
    _refillThread->trackRefill(collection, iid, std::move(keys));
  }
}

void RocksDBIndexCacheRefillFeature::scheduleFullIndexRefill(
    std::string const& database, std::string const& collection, IndexId iid) {
  {
    // create new refill task
    std::unique_lock lock(_indexFillTasksMutex);
    _indexFillTasks.emplace_back(IndexFillTask{database, collection, iid});
  }

  // schedule them
  scheduleIndexRefillTasks();
}

// wait until the background thread has applied all operations
void RocksDBIndexCacheRefillFeature::waitForCatchup() {
  if (_refillThread != nullptr) {
    _refillThread->waitForCatchup();
  }
}

void RocksDBIndexCacheRefillFeature::stopThread() { _refillThread.reset(); }

void RocksDBIndexCacheRefillFeature::buildStartupIndexRefillTasks() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  // get names of all databases
  for (auto const& database :
       methods::Databases::list(_databaseFeature, _clusterFeature, "")) {
    try {
      DatabaseGuard guard(_databaseFeature, database);

      methods::Collections::enumerate(
          &guard.database(),
          [&](std::shared_ptr<LogicalCollection> const& collection) {
            auto indexes = collection->getPhysical()->getReadyIndexes();
            for (auto const& index : indexes) {
              if (!index->canWarmup()) {
                // index not suitable for warmup
                continue;
              }

              std::unique_lock lock(_indexFillTasksMutex);
              TRI_ASSERT(_currentlyRunningIndexFillTasks == 0);
              _indexFillTasks.emplace_back(
                  IndexFillTask{database, collection->name(), index->id()});
            }
          });
    } catch (...) {
      // must ignore any errors here in case a database or collection
      // got deleted in the meantime
    }
  }
}

void RocksDBIndexCacheRefillFeature::scheduleIndexRefillTasks() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  std::unique_lock lock(_indexFillTasksMutex);
  // while we still have something to push out, do it.
  // note: we will only be scheduling at most _maxConcurrentIndexFillTask
  // index refills concurrently, in order to not overwhelm the instance.
  while (!_indexFillTasks.empty() && _currentlyRunningIndexFillTasks <
                                         _options.maxConcurrentIndexFillTasks) {
    if (server().isStopping()) {
      return;
    }
    auto task = std::move(_indexFillTasks.back());
    _indexFillTasks.pop_back();

    ++_currentlyRunningIndexFillTasks;

    lock.unlock();

    SchedulerFeature::SCHEDULER->queue(
        RequestLane::INTERNAL_LOW, [this, task = std::move(task)]() {
          if (!server().isStopping()) {
            Result res;
            try {
              res = warmupIndex(task.database, task.collection, task.iid);
            } catch (basics::Exception const& ex) {
              res = {ex.code(), ex.what()};
            } catch (std::exception const& ex) {
              // warmup is best effort, so we do not care much if it fails and
              // why
              res = {TRI_ERROR_INTERNAL, ex.what()};
            }
            if (res.fail()) {
              // check error. it is somewhat expected that a collection or
              // database is not found anymore in case someone has dropped it
              if (!res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) &&
                  !res.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
                // an unexpected error
                LOG_TOPIC("91c13", WARN, Logger::ENGINES)
                    << "unable to warmup index '" << task.iid.id() << "' in "
                    << task.database << "/" << task.collection << ": "
                    << res.errorMessage();
              }
            } else {
              ++_totalFullIndexRefills;
            }
          }

          bool hasMore;
          {
            std::unique_lock lock(_indexFillTasksMutex);

            TRI_ASSERT(_currentlyRunningIndexFillTasks > 0);
            --_currentlyRunningIndexFillTasks;

            hasMore = !_indexFillTasks.empty();
          }

          if (hasMore) {
            // queue next index refilling tasks
            scheduleIndexRefillTasks();
          }
        });

    // lock mutex again for next round
    lock.lock();
  }
}

Result RocksDBIndexCacheRefillFeature::warmupIndex(
    std::string const& database, std::string const& collection, IndexId iid) {
  DatabaseGuard guard(_databaseFeature, database);

  auto c =
      guard.database().useCollection(collection, /*checkPermissions*/ false);
  if (c == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  auto releaser = scopeGuard(
      [&]() noexcept { guard.database().releaseCollection(c.get()); });

  auto indexes = c->getPhysical()->getReadyIndexes();
  for (auto const& index : indexes) {
    if (index->id() == iid) {
      // found the correct index
      TRI_ASSERT(index->canWarmup());

      LOG_TOPIC("7dc37", DEBUG, Logger::ENGINES)
          << "warming up index '" << iid.id() << "' in " << database << "/"
          << collection;

      // warmup is best effort, so we do not care much if it fails
      return index->warmup();
    }
  }

  return {TRI_ERROR_ARANGO_INDEX_NOT_FOUND};
}

metrics::Counter& RocksDBIndexCacheRefillFeature::addTotalFullIndexRefills(
    metrics::IRegistry& metricsRegistry) {
  return metricsRegistry.add(rocksdb_cache_full_index_refills_total{});
}
