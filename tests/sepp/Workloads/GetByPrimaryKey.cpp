////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "GetByPrimaryKey.h"

#include <fstream>
#include <memory>
#include <stdexcept>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Indexes/Index.h"
#include "RestServer/arangod.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "VocBase/Methods/Collections.h"

#include "Execution.h"
#include "Server.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>

namespace arangodb::sepp::workloads {

auto GetByPrimaryKey::stoppingCriterion() const noexcept
    -> StoppingCriterion::type {
  return _options.stop;
}

auto GetByPrimaryKey::createThreads(Execution& exec, Server& server)
    -> WorkerThreadList {
  ThreadOptions defaultThread;
  defaultThread.stop = _options.stop;

  if (_options.defaultThreadOptions) {
    defaultThread.keyPrefix = _options.defaultThreadOptions->keyPrefix;
    defaultThread.minNumericKeyValue =
        _options.defaultThreadOptions->minNumericKeyValue;
    defaultThread.maxNumericKeyValue =
        _options.defaultThreadOptions->maxNumericKeyValue;
    defaultThread.fillBlockCache =
        _options.defaultThreadOptions->fillBlockCache;
    defaultThread.fetchFullDocument =
        _options.defaultThreadOptions->fetchFullDocument;
    defaultThread.collection = _options.defaultThreadOptions->collection;
  }

  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(std::make_unique<Thread>(defaultThread, exec, server));
  }
  return result;
}

GetByPrimaryKey::Thread::Thread(ThreadOptions options, Execution& exec,
                                Server& server)
    : ExecutionThread(exec, server), _options(options) {}

GetByPrimaryKey::Thread::~Thread() = default;

void GetByPrimaryKey::Thread::run() {
  // TODO - make more configurable

  auto collection = _server.vocbase()->lookupCollection(_options.collection);
  if (!collection) {
    throw std::runtime_error("Could not find collection " +
                             _options.collection);
  }

  RocksDBPrimaryIndex* primaryIndex = nullptr;
  for (auto const& idx : collection->getIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
      primaryIndex = static_cast<RocksDBPrimaryIndex*>(idx.get());
      break;
    }
  }

  if (!primaryIndex) {
    throw std::runtime_error("Could not find primary index for collection " +
                             _options.collection);
  }

  RocksDBCollection* rcoll =
      static_cast<RocksDBCollection*>(collection->getPhysical());
  std::uint64_t objectId = rcoll->objectId();
  std::uint64_t indexId = primaryIndex->objectId();

  rocksdb::ColumnFamilyHandle* docCF =
      arangodb::RocksDBColumnFamilyManager::get(
          arangodb::RocksDBColumnFamilyManager::Family::Documents);
  rocksdb::ColumnFamilyHandle* indexCF =
      arangodb::RocksDBColumnFamilyManager::get(
          arangodb::RocksDBColumnFamilyManager::Family::PrimaryIndex);

  auto& engine =
      _server.vocbase()->server().getFeature<arangodb::RocksDBEngine>();
  rocksdb::DB* rootDB = engine.db()->GetRootDB();

  std::string temp;
  RocksDBKey keyBuilder;
  rocksdb::PinnableSlice val;

  rocksdb::Status s;
  rocksdb::ReadOptions ro(/*cksum*/ false, /*cache*/ _options.fillBlockCache);
  ro.snapshot = rootDB->GetSnapshot();
  auto guard = scopeGuard([&]() noexcept {
    if (ro.snapshot) {
      rootDB->ReleaseSnapshot(ro.snapshot);
    }
  });
  ro.prefix_same_as_start = true;

  for (std::uint64_t i = _options.minNumericKeyValue;
       i < _options.maxNumericKeyValue; ++i) {
    temp.clear();
    if (!_options.keyPrefix.empty()) {
      temp.append(_options.keyPrefix);
    }
    temp.append(std::to_string(i));

    keyBuilder.constructPrimaryIndexValue(indexId, std::string_view(temp));

    s = rootDB->Get(ro, indexCF, keyBuilder.string(), &val);
    if (!s.ok()) {
      throw std::runtime_error("Failed to fetch primary key value: " +
                               s.ToString());
    }

    if (_options.fetchFullDocument) {
      keyBuilder.constructDocument(objectId, RocksDBValue::documentId(val));

      s = rootDB->Get(ro, docCF, keyBuilder.string(), &val);

      if (!s.ok()) {
        throw std::runtime_error("Failed to fetch document value: " +
                                 s.ToString());
      }
    }
    ++_operations;

    if (_operations % 512 == 0 && shouldStop()) {
      break;
    }
  }
}

auto GetByPrimaryKey::Thread::shouldStop() const noexcept -> bool {
  if (execution().stopped()) {
    return true;
  }

  using StopAfterOps = StoppingCriterion::NumberOfOperations;
  if (std::holds_alternative<StopAfterOps>(_options.stop)) {
    return _operations >= std::get<StopAfterOps>(_options.stop).count;
  }
  return false;
}

}  // namespace arangodb::sepp::workloads
