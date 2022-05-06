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

#include "InsertDocuments.h"

#include <fstream>
#include <memory>

#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Collections.h"

#include "Server.h"
#include "velocypack/Builder.h"

namespace arangodb::sepp::workloads {

auto InsertDocuments::stoppingCriterion() const noexcept
    -> StoppingCriterion::type {
  return _options.stop;
}

auto InsertDocuments::createThreads(Execution& exec, Server& server)
    -> WorkerThreadList {
  ThreadOptions defaultThread;
  defaultThread.stop = _options.stop;

  if (_options.defaultThreadOptions) {
    defaultThread.collection = _options.defaultThreadOptions->collection;
    if (std::holds_alternative<std::string>(
            _options.defaultThreadOptions->object)) {
      auto objectFile =
          std::get<std::string>(_options.defaultThreadOptions->object);
      try {
        std::ifstream t(objectFile);
        std::stringstream buffer;
        buffer << t.rdbuf();

        defaultThread.object =
            arangodb::velocypack::Parser::fromJson(buffer.str());
      } catch (std::exception const& e) {
        throw std::runtime_error("Failed to parse object file '" + objectFile +
                                 "' - " + e.what());
      }
    } else {
      defaultThread.object = std::make_shared<velocypack::Builder>();
      defaultThread.object->add(
          std::get<velocypack::Slice>(_options.defaultThreadOptions->object));
    }
  }

  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(std::make_unique<Thread>(defaultThread, exec, server));
  }
  return result;
}

InsertDocuments::Thread::Thread(ThreadOptions options, Execution& exec,
                                Server& server)
    : ExecutionThread(exec, server), _options(options) {}

InsertDocuments::Thread::~Thread() = default;

void InsertDocuments::Thread::run() {
  // TODO - make more configurable
  const unsigned numOps = 10;
  for (unsigned i = 0; i < numOps; ++i) {
    auto trx = std::make_unique<SingleCollectionTransaction>(
        transaction::StandaloneContext::Create(*_server.vocbase()),
        _options.collection, AccessMode::Type::WRITE);

    trx->begin();
    trx->insert("testcol", _options.object->slice(), {});
    trx->commit();
  }
  _operations += numOps;
}

auto InsertDocuments::Thread::shouldStop() const noexcept -> bool {
  using StopAfterOps = StoppingCriterion::NumberOfOperations;
  if (std::holds_alternative<StopAfterOps>(_options.stop)) {
    return _operations >= std::get<StopAfterOps>(_options.stop).count;
  }
  return false;
}

}  // namespace arangodb::sepp::workloads