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
#include "Basics/Exceptions.h"
#include "Basics/NumberOfCores.h"
#include "Basics/ScopeGuard.h"
#include "Basics/application-exit.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"
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

namespace {
size_t defaultConcurrentIndexFillTasks() {
  size_t n = NumberOfCores::getValue();
  if (n >= 16) {
    return n >> 3U;
  }
  return 1;
}

}  // namespace

DECLARE_COUNTER(rocksdb_cache_full_index_refills_total,
                "Total number of completed full index cache refills");

RocksDBIndexCacheRefillFeature::RocksDBIndexCacheRefillFeature(Server& server)
    : ArangodFeature{server, *this},
      _databaseFeature(server.getFeature<DatabaseFeature>()),
      _maxCapacity(128 * 1024),
      _maxConcurrentIndexFillTasks(::defaultConcurrentIndexFillTasks()),
      _autoRefill(false),
      _fillOnStartup(false),
      _autoRefillOnFollowers(true),
      _totalFullIndexRefills(server.getFeature<metrics::MetricsFeature>().add(
          rocksdb_cache_full_index_refills_total{})),
      _currentlyRunningIndexFillTasks(0) {
  setOptional(true);
  // we want to be late in the startup sequence
  startsAfter<BootstrapFeature>();
  startsAfter<DatabaseFeature>();
  startsAfter<RocksDBEngine>();

  // default value must be at least 1, as the minimum allowed value is also 1.
  TRI_ASSERT(_maxConcurrentIndexFillTasks >= 1);
}

RocksDBIndexCacheRefillFeature::~RocksDBIndexCacheRefillFeature() {
  stopThread();
}

void RocksDBIndexCacheRefillFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--rocksdb.auto-fill-index-caches-on-startup",
                  "Whether to automatically fill the in-memory edge cache with "
                  "entries on server startup.",
                  new options::BooleanParameter(&_fillOnStartup),
                  arangodb::options::makeFlags(
                      options::Flags::DefaultNoComponents,
                      options::Flags::OnDBServer, options::Flags::OnSingle,
                      options::Flags::Uncommon, options::Flags::Experimental))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(Enabling this option may cause additional CPU and
I/O load. You can limit how many index filling operations can execute
concurrently with the `--rocksdb.max-concurrent-index-fill-tasks` startup
option.)");

  options
      ->addOption("--rocksdb.auto-refill-index-caches-on-modify",
                  "Whether to automatically (re-)fill the in-memory edge "
                  "cache with entries on insert/update/replace/remove "
                  "operations by default.",
                  new options::BooleanParameter(&_autoRefill),
                  arangodb::options::makeFlags(
                      options::Flags::DefaultNoComponents,
                      options::Flags::OnDBServer, options::Flags::OnSingle,
                      options::Flags::Uncommon, options::Flags::Experimental))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(When documents are added, modified, or removed,
these changes are tracked and a background thread tries to update the edge
cache accordingly if the feature is enabled, by adding new, updating existing,
or deleting and refilling cache entries.

You can enable the feature for individual `INSERT`, `UPDATE`, `REPLACE`,  and
`REMOVE` operations in AQL queries, for individual document API requests that
insert, update, replace, or remove single or multiple edge documents, as well
as enable it by default using this startup option.

The background refilling is done on a best-effort basis and not guaranteed to
succeed, for example, if there is no memory available for the cache subsystem,
or during cache grow/shrink operations. A background thread is used so that
foreground write operations are not slowed down by a lot. It may still cause
additional I/O activity to look up data from the storage engine to repopulate
the cache.)");

  options
      ->addOption(
          "--rocksdb.auto-refill-index-caches-queue-capacity",
          "How many changes can be queued at most for automatically refilling "
          "the edge cache.",
          new options::SizeTParameter(&_maxCapacity),
          options::makeFlags(options::Flags::DefaultNoComponents,
                             options::Flags::OnDBServer,
                             options::Flags::OnSingle, options::Flags::Uncommon,
                             options::Flags::Experimental))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(This option restricts how many cache entries
the background thread for (re-)filling the in-memory edge cache can queue at
most. This limits the memory usage for the case of the background thread being
slower than other operations that invalidate cache entries of edge indexes.)");

  options
      ->addOption("--rocksdb.max-concurrent-index-fill-tasks",
                  "The maximum number of index fill tasks that can run "
                  "concurrently on server startup.",
                  new options::SizeTParameter(&_maxConcurrentIndexFillTasks,
                                              /*minValue*/ 1),
                  options::makeFlags(
                      options::Flags::DefaultNoComponents,
                      options::Flags::OnDBServer, options::Flags::OnSingle,
                      options::Flags::Uncommon, options::Flags::Experimental))
      .setIntroducedIn(30906)
      .setIntroducedIn(31002)
      .setLongDescription(R"(The lower this number, the lower the impact of the
edge cache filling, but the longer it takes to complete.)");

  options
      ->addOption(
          "--rocksdb.auto-refill-index-caches-on-followers",
          "Whether or not to automatically (re-)fill the in-memory index "
          "caches on followers as well.",
          new options::BooleanParameter(&_autoRefillOnFollowers),
          arangodb::options::makeFlags(options::Flags::DefaultNoComponents,
                                       options::Flags::OnDBServer,
                                       options::Flags::OnSingle))
      .setIntroducedIn(31005)
      .setLongDescription(R"(Set this to `false` to only (re-)fill in-memory
index caches on leaders and save memory on followers. 
Note that the value of this option should be identical for all DBServers.)");
}

void RocksDBIndexCacheRefillFeature::beginShutdown() {
  if (_refillThread != nullptr) {
    _refillThread->beginShutdown();
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
    buildStartupIndexRefillTasks();
    scheduleIndexRefillTasks();
  }
}

void RocksDBIndexCacheRefillFeature::stop() { stopThread(); }

bool RocksDBIndexCacheRefillFeature::autoRefill() const noexcept {
  return _autoRefill;
}

bool RocksDBIndexCacheRefillFeature::autoRefillOnFollowers() const noexcept {
  return _autoRefillOnFollowers;
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
  for (auto const& database : methods::Databases::list(server(), "")) {
    try {
      DatabaseGuard guard(_databaseFeature, database);

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

void RocksDBIndexCacheRefillFeature::scheduleIndexRefillTasks() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  std::unique_lock lock(_indexFillTasksMutex);
  // while we still have something to push out, do it.
  // note: we will only be scheduling at most _maxConcurrentIndexFillTask
  // index refills concurrently, in order to not overwhelm the instance.
  while (!_indexFillTasks.empty() &&
         _currentlyRunningIndexFillTasks < _maxConcurrentIndexFillTasks) {
    // intentional copy
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
              LOG_TOPIC("91c13", WARN, Logger::ENGINES)
                  << "unable to warmup index '" << task.iid.id() << "' in "
                  << task.database << "/" << task.collection << ": "
                  << res.errorMessage();
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
  auto& df = server().getFeature<DatabaseFeature>();

  DatabaseGuard guard(df, database);

  auto c =
      guard.database().useCollection(collection, /*checkPermissions*/ false);
  if (c == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  auto releaser = scopeGuard(
      [&]() noexcept { guard.database().releaseCollection(c.get()); });

  auto indexes = c->getIndexes();
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
