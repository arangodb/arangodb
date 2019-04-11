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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "RestHotBackupHandler.h"

#include <velocypack/velocypack-aliases.h>

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHotBackupHandler::RestHotBackupHandler(GeneralRequest* request, GeneralResponse* response)
  : RestBaseHandler(request, response) {

}

RestStatus RestHotBackupHandler::execute() {

  auto const result = verifyPermitted();
  if (!result.ok()) {
    return generateError(rest::ResponseCode::FORBIDDEN, result);
  }

  RequestType const type = _request->requestType();
  std::vector<std::string> const& suffixes = _request->suffixes();
  VPackSlice payload;
  result = parseHotBackupParams(type, suffixes, payload);
  if (!result.ok()) {
    return generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, result);
  }
  
  HotBackup hotbackup;
  VPackBuilder report;
  result = hotbackup.execute(suffixes.front(), payload, report);
  if (!result.ok()) {
    generateError(rest::ResponseCode::BAD, result);
  }

  return generateResult(rest::ResponseCode::OK, body.slice());

} // RestHotBackupHandler::execute


/// @brief check for administrator rights and rocksdb storage engine
///        generate the error response if issues found
arangodb::Result RestHotBackupHandler::verifyPermitted() {

  // do we have admin rights (if rights are active)
  if (nullptr != ExecContext::CURRENT && !ExecContext::CURRENT->isAdminUser()) {
    return arangodb::Result(
      rest::ResponseCode::FORBIDDEN, "you need admin rights for hotbackup operations");
  } // if

  return arangodb::Result();

} // RestHotBackupHandler::verifyPermitted


arangodb::Result RestHotBackupHandler::parseHotBackupParams(
  RequestType const type, std::vector<std::string> const & suffixes,
  VPackSlice slice) {

  if (type != Request::POST) {
    return arangodb::Result(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
      "backup endpoint only handles POST requests");
  }

  if (suffixes.size() != 1) {
    return arangodb::Result(
      TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
      "backup API only takes a single additional suffix out of "
      "[create, delet, list, upload, download]");
  
  slice = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    return arangodb::Result(
      TRI_ERROR_HTTP_CORRUPTED_JSON, "failed to parse backup body");
  }

  return arangodb::Result();
  
} // RestHotBackupHander::parseHotBackupParams
