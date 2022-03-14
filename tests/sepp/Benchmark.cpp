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

#include "Benchmark.h"

#include <chrono>
#include <memory>
#include <stdexcept>

#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Collections.h"

#include "Server.h"

namespace arangodb::sepp {

Benchmark::Benchmark() : _server(std::make_unique<Server>()) {}

Benchmark::~Benchmark() = default;

void Benchmark::run(char const* exectuable) {
  _server->start(exectuable);

  VPackBuilder optionsBuilder;
  optionsBuilder.openObject();
  optionsBuilder.close();

  std::shared_ptr<LogicalCollection> collection;
  auto res = methods::Collections::create(
      *_server->vocbase(),  // collection vocbase
      {},
      "testcol",               // collection name
      TRI_COL_TYPE_DOCUMENT,   // collection type
      optionsBuilder.slice(),  // collection properties
      false,                   // replication wait flag
      false,                   // replication factor flag
      false,                   // new Database?, here always false
      collection);
  if (!res.ok()) {
    throw std::runtime_error("Failed to create collection: " + std::string(res.errorMessage()));
  }

  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::seconds(5);
  std::size_t cnt = 0;
  bool stop = false;
  do {
    auto trx = std::make_unique<SingleCollectionTransaction>(
        transaction::StandaloneContext::Create(*_server->vocbase()), "testcol",
        AccessMode::Type::WRITE);

    VPackBuilder docBuilder;
    docBuilder.openObject();
    docBuilder.add("foo", VPackValue("bar"));
    docBuilder.close();

    trx->begin();
    trx->insert("testcol", docBuilder.slice(), {});
    trx->commit();

    if (++cnt % 128 == 0) {
      stop = std::chrono::steady_clock::now() > end;
    }
  } while (!stop);
  auto diff = std::chrono::steady_clock::now() - start;
  LOG_DEVEL << "performed " << cnt << " operations in " << diff.count() << "s";
}

}  // namespace arangodb::sepp