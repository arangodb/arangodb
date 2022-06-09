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

#include "IterateDocuments.h"

#include <fstream>
#include <memory>
#include <stdexcept>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "RestServer/arangod.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Collections.h"

#include "Execution.h"
#include "Server.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>

namespace arangodb::sepp::workloads {

auto IterateDocuments::stoppingCriterion() const noexcept
    -> StoppingCriterion::type {
  return _options.stop;
}

auto IterateDocuments::createThreads(Execution& exec, Server& server)
    -> WorkerThreadList {
  ThreadOptions defaultThread;
  defaultThread.stop = _options.stop;

  if (_options.defaultThreadOptions) {
    defaultThread.fillBlockCache =
        _options.defaultThreadOptions->fillBlockCache;
    defaultThread.collection = _options.defaultThreadOptions->collection;
  }

  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(std::make_unique<Thread>(defaultThread, exec, server));
  }
  return result;
}

IterateDocuments::Thread::Thread(ThreadOptions options, Execution& exec,
                                 Server& server)
    : ExecutionThread(exec, server), _options(options) {}

IterateDocuments::Thread::~Thread() = default;

void IterateDocuments::Thread::run() {
  // TODO - make more configurable

  auto collection = _server.vocbase()->lookupCollection(_options.collection);
  if (!collection) {
    throw std::runtime_error("Could not find collection " +
                             _options.collection);
  }

  RocksDBCollection* rcoll =
      static_cast<RocksDBCollection*>(collection->getPhysical());
  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  rocksdb::Slice upper(bounds.end());

  auto& engine =
      _server.vocbase()->server().getFeature<arangodb::RocksDBEngine>();
  rocksdb::DB* rootDB = engine.db()->GetRootDB();

  rocksdb::ReadOptions ro(/*cksum*/ false, /*cache*/ _options.fillBlockCache);
  ro.snapshot = rootDB->GetSnapshot();
  auto guard = scopeGuard([&]() noexcept {
    if (ro.snapshot) {
      rootDB->ReleaseSnapshot(ro.snapshot);
    }
  });
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;

  rocksdb::ColumnFamilyHandle* docCF =
      arangodb::RocksDBColumnFamilyManager::get(
          arangodb::RocksDBColumnFamilyManager::Family::Documents);
  std::unique_ptr<rocksdb::Iterator> it(rootDB->NewIterator(ro, docCF));

  OperationOptions options;
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    ++_operations;
    if (_operations % 512 == 0 && shouldStop()) {
      break;
    }
  }
}

auto IterateDocuments::Thread::shouldStop() const noexcept -> bool {
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
