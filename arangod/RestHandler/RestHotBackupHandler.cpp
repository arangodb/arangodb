////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RestHotBackupHandler.h"

//#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

//#include "Basics/StringUtils.h"
//#include "Basics/conversions.h"
//#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
//#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHotBackupHandler::RestHotBackupHandler(GeneralRequest* request, GeneralResponse* response)
  : RestBaseHandler(request, response) {

}

RestStatus RestHotBackupHandler::execute() {
  // extract the request specifics
  RequestType const type = _request->requestType();
  std::vector<std::string> const& suffixes = _request->suffixes();
  std::shared_ptr<RocksDBHotBackup> operation(parseHotBackupParams(type, suffixes));

/// add test for rocksdb engine

  if (operation && operation->valid()) {
    operation->execute();

    if (!operation->success()) {
      generateError(operation->restResponseCode(),
                    operation->restResponseError());
    } //if
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }

  return RestStatus::DONE;

} // RestHotBackupHandler::execute


std::shared_ptr<RocksDBHotBackup> RestHotBackupHandler::parseHotBackupParams(
  RequestType const type, std::vector<std::string> const & suffixes) {

  std::shared_ptr<RocksDBHotBackup> operation;
  bool parseSuccess;

  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (parseSuccess) {
    operation=RocksDBHotBackup::operationFactory(type, suffixes, body);
  } // if

  return operation;

} // RestHotBackupHander::parseHotBackupParams
