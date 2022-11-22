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

#include "RocksDBIndexCacheRefiller.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

RocksDBIndexCacheRefiller::RocksDBIndexCacheRefiller(ArangodServer& server)
    : ServerThread<ArangodServer>(server, "RocksDBCacheRefiller"),
      _databaseFeature(server.getFeature<DatabaseFeature>()),
      _numQueued(0) {}

RocksDBIndexCacheRefiller::~RocksDBIndexCacheRefiller() { shutdown(); }

void RocksDBIndexCacheRefiller::beginShutdown() {
  Thread::beginShutdown();

  // wake up the thread that may be waiting in run()
  CONDITION_LOCKER(guard, _condition);
  guard.broadcast();
}

void RocksDBIndexCacheRefiller::trackIndexCacheRefill(
    std::shared_ptr<LogicalCollection> const& collection, IndexId iid,
    std::vector<std::string> keys) {
  TRI_ASSERT(!keys.empty());

  CONDITION_LOCKER(guard, _condition);

  // will be created if it doesn't exist yet
  auto& target = _operations[collection->vocbase().id()][collection->id()][iid];

  size_t minSize = target.size() + keys.size();
  size_t newCapacity = std::max(size_t(8), target.capacity());
  while (newCapacity < minSize) {
    newCapacity *= 2;
  }
  target.reserve(newCapacity);
  for (auto& key : keys) {
    target.emplace_back(std::move(key));
    ++_numQueued;
  }

  guard.signal();
}

void RocksDBIndexCacheRefiller::refill(TRI_vocbase_t& vocbase, DataSourceId cid,
                                       IndexValues const& data) {
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, std::to_string(cid.id()),
                                  AccessMode::Type::READ);
  Result res = trx.begin();

  if (!res.ok()) {
    return;
  }

  for (auto const& it : data) {
    auto idx = trx.documentCollection()->lookupIndex(it.first);
    if (idx == nullptr) {
      continue;
    }
    static_cast<RocksDBIndex*>(idx.get())->refillCache(trx, it.second);
  }
}

void RocksDBIndexCacheRefiller::refill(TRI_vocbase_t& vocbase,
                                       CollectionValues const& data) {
  for (auto const& it : data) {
    try {
      refill(vocbase, it.first, it.second);
    } catch (...) {
      // possible that some collections get deleted in the middle
    }
  }
}

void RocksDBIndexCacheRefiller::refill(DatabaseValues const& data) {
  for (auto const& it : data) {
    try {
      DatabaseGuard guard(_databaseFeature, it.first);
      refill(guard.database(), it.second);
    } catch (...) {
      // possible that some databases get deleted in the middle
    }
  }
}

void RocksDBIndexCacheRefiller::run() {
  while (!isStopping()) {
    try {
      DatabaseValues operations;
      size_t numQueued = 0;

      {
        CONDITION_LOCKER(guard, _condition);
        operations = std::move(_operations);
        numQueued = _numQueued;
        _numQueued = 0;
      }

      if (!operations.empty()) {
        refill(operations);

        LOG_TOPIC("9b2f5", TRACE, Logger::ENGINES)
            << "(re-)inserted " << numQueued << " entries into index caches";
      }

      CONDITION_LOCKER(guard, _condition);
      guard.wait();
    } catch (std::exception const& ex) {
      LOG_TOPIC("443da", ERR, Logger::ENGINES)
          << "caught exception in RocksDBIndexCacheRefiller: " << ex.what();
    } catch (...) {
      LOG_TOPIC("6627f", ERR, Logger::ENGINES)
          << "caught unknown exception in RocksDBIndexCacheRefiller";
    }
  }
}
