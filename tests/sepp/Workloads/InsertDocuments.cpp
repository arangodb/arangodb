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

#include <memory>

#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Collections.h"

#include "Server.h"

namespace arangodb::sepp::workloads {

auto InsertDocuments::createThreads(Execution const& exec, Server& server)
    -> WorkerThreadList {
  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(std::make_unique<Thread>(
        _options.defaultThreadOptions.value(), exec, server));
  }
  return result;
}

InsertDocuments::Thread::Thread(ThreadOptions options, Execution const& exec,
                                Server& server)
    : ExecutionThread(exec, server), _options(options) {}

InsertDocuments::Thread::~Thread() = default;

void InsertDocuments::Thread::run() {
  // TODO - make more configurable
  for (unsigned i = 0; i < 10; ++i) {
    auto trx = std::make_unique<SingleCollectionTransaction>(
        transaction::StandaloneContext::Create(*_server.vocbase()),
        _options.collection, AccessMode::Type::WRITE);

    trx->begin();
    trx->insert("testcol", _options.object, {});
    trx->commit();
  }
  _operations += 10;
}

}  // namespace arangodb::sepp::workloads