////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBRestCollectionHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RocksDBRestCollectionHandler::RocksDBRestCollectionHandler(
    ArangodServer& server, GeneralRequest* request, GeneralResponse* response)
    : RestCollectionHandler(server, request, response) {}

futures::Future<Result> RocksDBRestCollectionHandler::handleExtraCommandPut(
    std::shared_ptr<LogicalCollection> coll, std::string const& suffix,
    velocypack::Builder& builder) {
  if (suffix == "recalculateCount") {
    if (!ExecContext::current().canUseCollection(coll->name(),
                                                 auth::Level::RW)) {
      co_return Result(TRI_ERROR_FORBIDDEN);
    }

    auto physical = toRocksDBCollection(coll->getPhysical());

    Result res;
    uint64_t count = 0;
    try {
      count = co_await physical->recalculateCounts();
    } catch (basics::Exception const& e) {
      res.reset(e.code(), e.message());
    }
    if (res.ok()) {
      VPackObjectBuilder guard(&builder);
      builder.add("result", VPackValue(true));
      builder.add("count", VPackValue(count));
    }
    co_return res;
  }

  co_return TRI_ERROR_NOT_IMPLEMENTED;
}
