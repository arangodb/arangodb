////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBIndexCacheRefillFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillThread.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;

RocksDBIndexCacheRefillFeature::RocksDBIndexCacheRefillFeature(Server& server)
    : ArangodFeature{server, *this},
      _maxCapacity(128 * 1024),
      _maxConcurrentIndexFillTasks(2),
      _autoRefill(false),
      _fillOnStartup(false),
      _currentlyRunningIndexFillTasks(0) {
  setOptional(true);
  // we want to be late in the startup sequence
  startsAfter<BootstrapFeature>();
  startsAfter<DatabaseFeature>();
  startsAfter<RocksDBEngine>();
}

RocksDBIndexCacheRefillFeature::~RocksDBIndexCacheRefillFeature() {
  stopThread();
}

void RocksDBIndexCacheRefillFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption(
          "--rocksdb.auto-fill-index-caches-on-startup",
          "Automatically fill in-memory index cache entries on server startup.",
          new options::BooleanParameter(&_fillOnStartup),
          arangodb::options::makeFlags(options::Flags::DefaultNoComponents,
                                       options::Flags::OnDBServer,
                                       options::Flags::OnSingle))
      .setIntroducedIn(30906);

  options
      ->addOption("--rocksdb.auto-refill-index-caches",
                  "Automatically (re-)fill in-memory index cache entries upon "
                  "insert/update/replace.",
                  new options::BooleanParameter(&_autoRefill),
                  arangodb::options::makeFlags(
                      options::Flags::DefaultNoComponents,
                      options::Flags::OnDBServer, options::Flags::OnSingle))
      .setIntroducedIn(30906);

  options
      ->addOption(
          "--rocksdb.auto-refill-index-cache-queue-capacity",
          "Maximum capacity for automatic in-memory index cache refill queue.",
          new options::SizeTParameter(&_maxCapacity),
          options::makeFlags(options::Flags::DefaultNoComponents,
                             options::Flags::OnDBServer,
                             options::Flags::OnSingle))
      .setIntroducedIn(30906);

  options
      ->addOption("--rocksdb.max-concurrent-index-fill-tasks",
                  "Maximum number of concurrent index fill tasks at startup.",
                  new options::SizeTParameter(&_maxConcurrentIndexFillTasks),
                  options::makeFlags(options::Flags::DefaultNoComponents,
                                     options::Flags::OnDBServer,
                                     options::Flags::OnSingle))
      .setIntroducedIn(30906);
}

void RocksDBIndexCacheRefillFeature::beginShutdown() {
  if (_refillThread != nullptr) {
    _refillThread->shutdown();
  }
}

void RocksDBIndexCacheRefillFeature::start() {
  if (ServerState::instance()->isCoordinator()) {
    // we don't have in-memory caches for indexes on the coordinator
    return;
  }

  _refillThread =
      std::make_unique<RocksDBIndexCacheRefillThread>(server(), _maxCapacity);

  if (!_refillThread->start()) {
    LOG_TOPIC("836a6", FATAL, Logger::ENGINES)
        << "could not start rocksdb index cache refill thread";
    FATAL_ERROR_EXIT();
  }

  if (_fillOnStartup) {
    buildIndexRefillTasks();
    queueIndexRefillTasks();
  }
}

void RocksDBIndexCacheRefillFeature::stop() { stopThread(); }

bool RocksDBIndexCacheRefillFeature::autoRefill() const noexcept {
  return _autoRefill;
}

size_t RocksDBIndexCacheRefillFeature::maxCapacity() const noexcept {
  return _maxCapacity;
}

bool RocksDBIndexCacheRefillFeature::fillOnStartup() const noexcept {
  return _fillOnStartup;
}

void RocksDBIndexCacheRefillFeature::trackRefill(
    std::shared_ptr<LogicalCollection> const& collection, IndexId iid,
    std::vector<std::string> keys) {
  if (_refillThread != nullptr) {
    _refillThread->trackRefill(collection, iid, std::move(keys));
  }
}

void RocksDBIndexCacheRefillFeature::stopThread() {
  if (_refillThread == nullptr) {
    return;
  }

  _refillThread->beginShutdown();

  // wait until background thread stops
  while (_refillThread->isRunning()) {
    std::this_thread::yield();
  }
  _refillThread.reset();
}

void RocksDBIndexCacheRefillFeature::buildIndexRefillTasks() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto& df = server().getFeature<DatabaseFeature>();
  // get names of all databases
  for (auto const& database : methods::Databases::list(server(), "")) {
    try {
      DatabaseGuard guard(df, database);

      methods::Collections::enumerate(
          &guard.database(),
          [&](std::shared_ptr<LogicalCollection> const& collection) {
            auto indexes = collection->getIndexes();
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

void RocksDBIndexCacheRefillFeature::queueIndexRefillTasks() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  std::unique_lock lock(_indexFillTasksMutex);
  while (!_indexFillTasks.empty() &&
         _currentlyRunningIndexFillTasks < _maxConcurrentIndexFillTasks) {
    // intentional copy
    auto task = _indexFillTasks.back();
    _indexFillTasks.pop_back();

    ++_currentlyRunningIndexFillTasks;

    lock.unlock();

    SchedulerFeature::SCHEDULER->queue(
        RequestLane::INTERNAL_LOW, [this, task = std::move(task)]() {
          if (!server().isStopping()) {
            try {
              warmupIndex(task.database, task.collection, task.iid);
            } catch (...) {
              // warmup is best effort, so we do not care much if it fails and
              // why
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
            queueIndexRefillTasks();
          }
        });

    // lock mutex again for next round
    lock.lock();
  }
}

void RocksDBIndexCacheRefillFeature::warmupIndex(std::string const& database,
                                                 std::string const& collection,
                                                 IndexId iid) {
  auto& df = server().getFeature<DatabaseFeature>();

  DatabaseGuard guard(df, database);

  auto c =
      guard.database().useCollection(collection, /*checkPermissions*/ false);
  if (c == nullptr) {
    return;
  }

  auto releaser = scopeGuard(
      [&]() noexcept { guard.database().releaseCollection(c.get()); });

  auto indexes = c->getIndexes();
  for (auto const& index : indexes) {
    if (index->id() == iid) {
      LOG_TOPIC("7dc37", INFO, Logger::ENGINES)
          << "warming up index '" << iid.id() << "' in " << database << "/"
          << collection;

      // found the correct index
      TRI_ASSERT(index->canWarmup());
      Result res = index->scheduleWarmup();
      // warmup is best effort, so we do not care much if it fails
      if (res.fail()) {
        LOG_TOPIC("69f0e", WARN, Logger::ENGINES)
            << "warming up index '" << iid.id() << "' in " << database << "/"
            << collection << " failed: " << res.errorMessage();
      }
      break;
    }
  }
}
