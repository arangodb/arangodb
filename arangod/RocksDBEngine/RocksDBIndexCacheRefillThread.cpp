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

#include "RocksDBIndexCacheRefillThread.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include <chrono>

using namespace arangodb;

DECLARE_COUNTER(rocksdb_cache_auto_refill_loaded_total,
                "Total number of auto-refilled in-memory cache items");
DECLARE_COUNTER(rocksdb_cache_auto_refill_dropped_total,
                "Total number of dropped items for in-memory cache refilling");

RocksDBIndexCacheRefillThread::RocksDBIndexCacheRefillThread(
    ArangodServer& server, size_t maxCapacity)
    : ServerThread<ArangodServer>(server, "RocksDBCacheRefiller"),
      _databaseFeature(server.getFeature<DatabaseFeature>()),
      _maxCapacity(maxCapacity),
      _numQueued(0),
      _proceeding(0),
      _totalNumQueued(server.getFeature<metrics::MetricsFeature>().add(
          rocksdb_cache_auto_refill_loaded_total{})),
      _totalNumDropped(server.getFeature<metrics::MetricsFeature>().add(
          rocksdb_cache_auto_refill_dropped_total{})) {}

RocksDBIndexCacheRefillThread::~RocksDBIndexCacheRefillThread() { shutdown(); }

void RocksDBIndexCacheRefillThread::beginShutdown() {
  Thread::beginShutdown();

  std::lock_guard guard{_condition.mutex};
  // clear all remaining operations, so that we don't try applying them anymore.
  _operations.clear();
  // wake up the thread that may be waiting in run()
  _condition.cv.notify_all();
}

void RocksDBIndexCacheRefillThread::trackRefill(
    std::shared_ptr<LogicalCollection> const& collection, IndexId iid,
    std::vector<std::string> keys) {
  TRI_ASSERT(!keys.empty());
  size_t const n = keys.size();

  {
    std::lock_guard guard{_condition.mutex};

    if (_numQueued + n >= _maxCapacity) {
      // we have reached the maximum queueing capacity, so give up on whatever
      // keys we received just now.
      // increase metric and return
      _totalNumDropped += n;
      return;
    }

    // the map entries for the database/collection will be created if they don't
    // yet exist
    auto& entries = _operations[collection->vocbase().id()][collection->id()];

    auto it = entries.find(iid);
    if (it == entries.end()) {
      // no entry yet for this particular index id. now move all keys over at
      // once, which is most efficient. this should be the usual case, as we
      // are normally clearing all stored data after every round
      entries.emplace(iid, std::move(keys));
    } else {
      // entry for particular index id already existed
      auto& target = (*it).second;
      TRI_ASSERT(!target.empty());
      size_t minSize = target.size() + n;
      size_t newCapacity = std::max(size_t(8), target.capacity());
      while (newCapacity < minSize) {
        // grow capacity with factor of 2 growth strategy
        newCapacity *= 2;
      }
      target.reserve(newCapacity);

      // move keys over to us
      for (auto& key : keys) {
        target.emplace_back(std::move(key));
      }
    }
    _numQueued += n;
  }

  // wake up background thread
  _condition.cv.notify_one();
  // increase metric
  _totalNumQueued += n;
}

void RocksDBIndexCacheRefillThread::waitForCatchup() {
  // give up after 10 seconds max
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);

  std::unique_lock guard{_condition.mutex};

  while (_proceeding > 0 || _numQueued > 0) {
    guard.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (std::chrono::steady_clock::now() > end) {
      break;
    }

    guard.lock();
  }
}

void RocksDBIndexCacheRefillThread::refill(TRI_vocbase_t& vocbase,
                                           DataSourceId cid,
                                           IndexValues const& data) {
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, std::to_string(cid.id()),
                                  AccessMode::Type::READ);
  Result res = trx.begin();

  if (!res.ok()) {
    return;
  }

  // loop over all the indexes in the given collection
  for (auto const& it : data) {
    auto idx = trx.documentCollection()->lookupIndex(it.first);
    if (idx == nullptr) {
      // index doesn't exist anymore
      continue;
    }
    static_cast<RocksDBIndex*>(idx.get())->refillCache(trx, it.second);
  }
}

void RocksDBIndexCacheRefillThread::refill(TRI_vocbase_t& vocbase,
                                           CollectionValues const& data) {
  // loop over every collection in the given database
  for (auto const& it : data) {
    try {
      refill(vocbase, it.first, it.second);
    } catch (...) {
      // possible that some collections get deleted in the middle
    }
  }
}

void RocksDBIndexCacheRefillThread::refill(DatabaseValues const& data) {
  // loop over all databases that we have data for
  for (auto const& it : data) {
    try {
      DatabaseGuard guard(_databaseFeature, it.first);
      refill(guard.database(), it.second);
    } catch (...) {
      // possible that some databases get deleted in the middle
    }
  }
}

void RocksDBIndexCacheRefillThread::run() {
  while (!isStopping()) {
    try {
      DatabaseValues operations;
      size_t numQueued = 0;

      {
        std::lock_guard guard{_condition.mutex};

        operations = std::move(_operations);
        numQueued = _numQueued;
        _numQueued = 0;
        _proceeding = numQueued;
      }

      if (!operations.empty()) {
        LOG_TOPIC("1dd43", TRACE, Logger::ENGINES)
            << "(re-)inserting " << numQueued << " entries into index caches";

        // note: if refill somehow throws, it is not the end of the world.
        // we will then not have repopulated some cache entries, but it
        // should not matter too much, as repopulating the cache entries
        // is best effort only and does not affect correctness.
        refill(operations);

        LOG_TOPIC("9b2f5", TRACE, Logger::ENGINES)
            << "(re-)inserted " << numQueued << " entries into index caches";
      }

      std::unique_lock guard{_condition.mutex};
      _proceeding = 0;

      if (!isStopping() && _operations.empty()) {
        _condition.cv.wait_for(guard, std::chrono::seconds{10});
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("443da", ERR, Logger::ENGINES)
          << "caught exception in RocksDBIndexCacheRefillThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC("6627f", ERR, Logger::ENGINES)
          << "caught unknown exception in RocksDBIndexCacheRefillThread";
    }
  }
}
