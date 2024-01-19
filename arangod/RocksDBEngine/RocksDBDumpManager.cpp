////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBDumpManager.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DumpLimitsFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBDumpContext.h"
#include "VocBase/ticks.h"

#include <absl/strings/str_cat.h>

#include <chrono>
#include <thread>

using namespace arangodb;

DECLARE_GAUGE(arangodb_dump_ongoing, std::uint64_t,
              "Number of dumps currently ongoing");
DECLARE_GAUGE(arangodb_dump_memory_usage, std::uint64_t,
              "Memory usage of currently ongoing dumps");
DECLARE_COUNTER(
    arangodb_dump_threads_blocked_total,
    "Number of times a dump thread was blocked because of memory restrictions");

RocksDBDumpManager::RocksDBDumpManager(RocksDBEngine& engine,
                                       metrics::MetricsFeature& metricsFeature,
                                       DumpLimits const& limits)
    : _engine(engine),
      _limits(limits),
      _dumpsOngoing(metricsFeature.add(arangodb_dump_ongoing{})),
      _dumpsMemoryUsage(metricsFeature.add(arangodb_dump_memory_usage{})),
      _dumpsThreadsBlocked(
          metricsFeature.add(arangodb_dump_threads_blocked_total{})) {}

RocksDBDumpManager::~RocksDBDumpManager() {
  garbageCollect(true);
  TRI_ASSERT(_dumpsMemoryUsage.load() == 0);
}

std::shared_ptr<RocksDBDumpContext> RocksDBDumpManager::createContext(
    RocksDBDumpContextOptions opts, std::string const& user,
    std::string const& database, bool useVPack) {
  TRI_ASSERT(ServerState::instance()->isSingleServer() ||
             ServerState::instance()->isDBServer());

  opts.docsPerBatch =
      std::clamp(opts.docsPerBatch, _limits.docsPerBatchLowerBound,
                 _limits.docsPerBatchUpperBound);
  opts.batchSize = std::clamp(opts.batchSize, _limits.batchSizeLowerBound,
                              _limits.batchSizeUpperBound);
  opts.parallelism = std::clamp(opts.parallelism, _limits.parallelismLowerBound,
                                _limits.parallelismUpperBound);

  // If the local RocksDB database still uses little endian key encoding,
  // then the whole new dump method does not work, since ranges in _revs
  // do not correspond to ranges in RocksDB keys in the documents column
  // family. Therefore, we block the creation of a dump context right away.
  if (rocksutils::rocksDBEndianness == RocksDBEndianness::Little) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_OLD_ROCKSDB_FORMAT);
  }

  // generating the dump context can throw exceptions. if it does, then
  // no harm is done, and no resources will be leaked.
  auto context = std::make_shared<RocksDBDumpContext>(
      _engine, *this, _engine.server().getFeature<DatabaseFeature>(),
      generateId(), std::move(opts), user, database, useVPack);

  std::lock_guard mutexLocker{_lock};

  if (_engine.server().isStopping()) {
    // do not accept any further contexts when we are already shutting down
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  bool inserted = _contexts.try_emplace(context->id(), context).second;
  if (!inserted) {
    // cannot insert into map. should never happen
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to insert dump context");
  }

  TRI_ASSERT(_contexts.at(context->id()) != nullptr);

  _dumpsOngoing.fetch_add(1);

  return context;
}

std::shared_ptr<RocksDBDumpContext> RocksDBDumpManager::find(
    std::string const& id, std::string const& database,
    std::string const& user) {
  std::lock_guard mutexLocker{_lock};

  // this will throw in case the context cannot be found or belongs to a
  // different user
  auto it = lookupContext(id, database, user);
  TRI_ASSERT(it != _contexts.end());

  return (*it).second;
}

void RocksDBDumpManager::remove(std::string const& id,
                                std::string const& database,
                                std::string const& user) {
  std::shared_ptr<RocksDBDumpContext> victim;
  {
    std::lock_guard mutexLocker{_lock};

    // this will throw in case the context cannot be found or belongs to a
    // different user
    auto it = lookupContext(id, database, user);
    TRI_ASSERT(it != _contexts.end());
    victim = (*it).second;

    TRI_ASSERT(victim != nullptr);
    // give the victim a hind to stop all its threads.
    victim->stop();

    // if we remove the context from the map, then the context will be
    // destroyed if it is not in use by any other thread. if it is in
    // use by another thread, the thread will have a shared_ptr of the
    // context, and the context will be destroyed once the shared_ptr
    // goes out of scope in the other thread
    _contexts.erase(it);
    _dumpsOngoing.fetch_sub(1);
  }

  // when we go out of scope here, we can destroy the victim
  // without holding the mutex
}

void RocksDBDumpManager::dropDatabase(TRI_vocbase_t& vocbase) {
  std::lock_guard mutexLocker{_lock};

  std::erase_if(_contexts, [&](auto const& x) {
    return x.second->database() == vocbase.name();
  });

  _dumpsOngoing = _contexts.size();
}

void RocksDBDumpManager::garbageCollect(bool force) {
  std::lock_guard mutexLocker{_lock};

  if (force) {
    _contexts.clear();
  } else {
    auto const now = TRI_microtime();
    std::erase_if(_contexts,
                  [&](auto const& x) { return x.second->expires() < now; });
  }

  _dumpsOngoing = _contexts.size();
}

std::unique_ptr<RocksDBDumpContext::Batch> RocksDBDumpManager::requestBatch(
    RocksDBDumpContext& context, std::string const& collectionName,
    std::uint64_t& batchSize, bool useVPack,
    velocypack::Options const* vpackOptions) {
  auto waitTime = std::chrono::milliseconds(10);

  bool metricIncreased = false;
  auto reserve = [&]() -> bool {
    bool result = reserveCapacity(batchSize);
    // we have exceeded the memory limit for dumping.
    if (!result && !metricIncreased) {
      // count only once per batch
      _dumpsThreadsBlocked.count();
      metricIncreased = true;
      LOG_TOPIC("d8adc", INFO, Logger::DUMP)
          << "blocking dump operation because memory reserve capacity for dump "
             "is temporarily exceeded";
    }
    return result;
  };

  int counter = 0;
  while (!reserve()) {
    if (context.stopped()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_REQUEST_CANCELED,
          "dump context was explicitly canceled or timed out");
    }

    // we have exceeded the memory limit for dumping.
    // block this thread and wait until we have some memory capacity left.
    if (_engine.server().isStopping()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::this_thread::sleep_for(waitTime);
    if (waitTime <= std::chrono::milliseconds(50)) {
      waitTime *= 2;
    }

    if (++counter >= 50) {
      // we came along here 50 times without making progress.
      // probably the batch size is still too high.
      // in order to make _some_ progress, we reduce the batch size
      // and then try again in the next round.
      counter = 0;
      batchSize /= 2;
      if (batchSize < 16 * 1024) {
        // now it doesn't make any sense anymore
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_RESOURCE_LIMIT,
            "dump resource limit exceeded. requested batch size value is "
            "probably too high");
      }
    }
  }

  if (useVPack) {
    return std::make_unique<RocksDBDumpContext::BatchVPackArray>(
        *this, batchSize, collectionName);
  }
  return std::make_unique<RocksDBDumpContext::BatchJSONL>(
      *this, batchSize, collectionName, vpackOptions);
}

bool RocksDBDumpManager::reserveCapacity(std::uint64_t value) noexcept {
  auto expected = _dumpsMemoryUsage.load();
  auto desired = expected + value;
  while (desired < _limits.memoryUsage) {
    if (_dumpsMemoryUsage.compare_exchange_weak(expected, desired)) {
      return true;
    }
    desired = expected + value;
  }
  return false;
}

// returns false if we are beyond the configured limit for all dumps.
void RocksDBDumpManager::trackMemoryUsage(std::uint64_t size) noexcept {
  _dumpsMemoryUsage.fetch_add(size);
}

void RocksDBDumpManager::untrackMemoryUsage(std::uint64_t size) noexcept {
  TRI_ASSERT(_dumpsMemoryUsage.load() >= size);
  _dumpsMemoryUsage.fetch_sub(size);
}

std::string RocksDBDumpManager::generateId() {
  // rationale: we use a HLC value here, because it is guaranteed to
  // move forward, even across restarts. the last HLC value is persisted
  // on server shutdown, so we avoid handing out an HLC value, shutting
  // down the server, and handing out the same HLC value for a different
  // dump after the restart.
  return absl::StrCat("dump-", TRI_HybridLogicalClock());
}

RocksDBDumpManager::MapType::iterator RocksDBDumpManager::lookupContext(
    std::string const& id, std::string const& database,
    std::string const& user) {
  auto it = _contexts.find(id);
  if (it == _contexts.end()) {
    // "cursor not found" is not a great return code, but it is much more
    // specific than a generic error. we can also think of a dump context
    // as a collection of cursors for shard dumping.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CURSOR_NOT_FOUND,
                                   "requested dump context not found");
  }

  auto& context = (*it).second;
  TRI_ASSERT(context != nullptr);
  if (!context->canAccess(database, user)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "insufficient permissions");
  }
  return it;
}
