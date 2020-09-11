////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Cluster/ClusterMethods.h"
#include "RocksDBRestCompactHandler.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "Utils/ExecContext.h"

#include <rocksdb/db.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::rest;

RocksDBRestCompactHandler::RocksDBRestCompactHandler(GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RocksDBRestCompactHandler::execute() {
  ExecContext const* current = ExecContext::CURRENT;
  if (ExecContext::isAuthEnabled() && current != nullptr && !current->isSuperuser()) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN, "compaction is only allowed for superusers");
    return RestStatus::DONE;
  }
  
  if (_request->requestType() != rest::RequestType::PUT) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  bool changeLevel = _request->parsedValue("changeLevel", false);
  bool compactBottomMostLevel = _request->parsedValue("compactBottomMostLevel", false);
  
  rocksdb::DB* db = arangodb::rocksutils::globalRocksDB()->GetRootDB();
  Result res = rocksutils::compactAll(db, changeLevel, compactBottomMostLevel);
  if (res.fail()) {
    generateError(
        GeneralResponse::responseCode(res.errorNumber()), res.errorNumber(), std::string("database compaction failied: ") + res.errorMessage());
  } else {
    generateOk(rest::ResponseCode::OK, arangodb::velocypack::Slice::emptyObjectSlice());
  }
  return RestStatus::DONE;
}
