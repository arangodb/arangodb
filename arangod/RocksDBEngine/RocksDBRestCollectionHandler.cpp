////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBRestCollectionHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RocksDBRestCollectionHandler::RocksDBRestCollectionHandler(application_features::ApplicationServer& server,
                                                           GeneralRequest* request,
                                                           GeneralResponse* response)
    : RestCollectionHandler(server, request, response) {}

Result RocksDBRestCollectionHandler::handleExtraCommandPut(std::shared_ptr<LogicalCollection> coll,
                                                           std::string const& suffix,
                                                           velocypack::Builder& builder) {
  if (suffix == "recalculateCount") {
    if (!ExecContext::current().canUseCollection(coll->name(), auth::Level::RW)) {
      return Result(TRI_ERROR_FORBIDDEN);
    }

    auto physical = toRocksDBCollection(coll->getPhysical());

    Result res;
    uint64_t count = 0;
    try {
      count = physical->recalculateCounts();
    } catch (basics::Exception const& e) {
      res.reset(e.code(), e.message());
    }
    if (res.ok()) {
      VPackObjectBuilder guard(&builder);
      builder.add("result", VPackValue(true));
      builder.add("count", VPackValue(count));
    }
    return res;
  }

  return TRI_ERROR_NOT_IMPLEMENTED;
}
