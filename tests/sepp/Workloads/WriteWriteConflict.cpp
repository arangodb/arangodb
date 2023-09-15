
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

#include "WriteWriteConflict.h"

#include <fstream>
#include <memory>
#include <stdexcept>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Basics/voc-errors.h"
#include "Indexes/Index.h"
#include "RestServer/arangod.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"

#include "Execution.h"
#include "Server.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>

namespace arangodb::sepp::workloads {

auto WriteWriteConflict::stoppingCriterion() const noexcept
    -> StoppingCriterion::type {
  return _options.stop;
}

auto WriteWriteConflict::createThreads(Execution& exec, Server& server)
    -> WorkerThreadList {
  ThreadOptions defaultThread;
  defaultThread.stop = _options.stop;

  if (_options.defaultThreadOptions) {
    static_cast<WriteWriteConflict::Options::Thread&>(defaultThread) =
        _options.defaultThreadOptions.value();
  }

  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(
        std::make_unique<Thread>(defaultThread, i, exec, server));
  }
  return result;
}

WriteWriteConflict::Thread::Thread(ThreadOptions options, std::uint32_t id,
                                   Execution& exec, Server& server)
    : ExecutionThread(id, exec, server), _options(options) {}

WriteWriteConflict::Thread::~Thread() = default;

void WriteWriteConflict::Thread::run() {
  velocypack::Builder builder;
  builder.openObject();
  // we want all threads to keep hammering on the same document!
  builder.add("_key", VPackValue("blubb"));
  builder.add("foo", VPackValue(42));
  builder.close();

  OperationOptions opts{};
  if (_options.operation == OperationType::insert) {
    opts.overwriteMode = OperationOptions::OverwriteMode::Update;
  }

  transaction::Options trxOpts;
  trxOpts.delaySnapshot = _options.delaySnapshot;

  for (;;) {
    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(*_server.vocbase()),
        _options.collection, AccessMode::Type::WRITE, trxOpts);
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

    {
      auto res = trx.begin();
      if (!res.ok()) {
        throw std::runtime_error("Failed to begin trx: " +
                                 std::string(res.errorMessage()));
      }
    }

    {
      auto opRes = [&]() {
        switch (_options.operation) {
          case OperationType::insert:
            return trx.insert(_options.collection, builder.slice(), opts);
          case OperationType::update:
            return trx.update(_options.collection, builder.slice(), opts);
          case OperationType::replace:
            return trx.replace(_options.collection, builder.slice(), opts);
          default:
            TRI_ASSERT(false);
            throw std::runtime_error("Invalid operation type");
        }
      }();
      if (!opRes.ok()) {
        if (opRes.result.errorNumber() == TRI_ERROR_ARANGO_CONFLICT) {
          ++_conflicts;
        } else {
          throw std::runtime_error("Failed to insert document in trx: " +
                                   std::string(opRes.errorMessage()));
        }
      }
    }

    {
      auto res = trx.commit();
      if (!res.ok()) {
        throw std::runtime_error("Failed to commit trx: " +
                                 std::string(res.errorMessage()));
      }
    }

    ++_operations;

    if (_operations % 512 == 0 && shouldStop()) {
      break;
    }
  }
  std::cout << ("Conflicts: " + std::to_string(_conflicts) + "\n");
}

auto WriteWriteConflict::Thread::shouldStop() const noexcept -> bool {
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
