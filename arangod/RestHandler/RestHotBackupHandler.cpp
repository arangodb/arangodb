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
///
/// @author Matthew Von-Maszewski
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestHotBackupHandler.h"

#include <velocypack/velocypack-aliases.h>

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "StorageEngine/HotBackup.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHotBackupHandler::RestHotBackupHandler(GeneralRequest* request, GeneralResponse* response)
  : RestBaseHandler(request, response) {

}

RestStatus RestHotBackupHandler::execute() {

  auto result = verifyPermitted();
  if (!result.ok()) {
    generateError(
      rest::ResponseCode::FORBIDDEN, result.errorNumber(), result.errorMessage());
    return RestStatus::DONE;
  }

  RequestType const type = _request->requestType();
  std::vector<std::string> const& suffixes = _request->suffixes();
  VPackSlice payload;
  result = parseHotBackupParams(type, suffixes, payload);
  if (!result.ok()) {
    if (result.errorNumber() == TRI_ERROR_HTTP_METHOD_NOT_ALLOWED) {
      generateError(
        rest::ResponseCode::METHOD_NOT_ALLOWED, result.errorNumber(),
        result.errorMessage());
    } else {
      generateError(
        rest::ResponseCode::BAD, result.errorNumber(),
        result.errorMessage());
    }
    return RestStatus::DONE;
  }

  HotBackup hotbackup;
  VPackBuilder report;
  result = hotbackup.execute(suffixes.front(), payload, report);
  if (!result.ok()) {
    rest::ResponseCode code;
    auto resultNum = result.errorNumber();
    if (resultNum == TRI_ERROR_NOT_IMPLEMENTED) {
      code = rest::ResponseCode::NOT_IMPLEMENTED;
    } else if (resultNum == TRI_ERROR_LOCK_TIMEOUT) {
      code = rest::ResponseCode::REQUEST_TIMEOUT;
    } else if (
      resultNum == TRI_ERROR_HTTP_SERVER_ERROR ||
      resultNum == TRI_ERROR_HOT_BACKUP_INTERNAL) {
      code = rest::ResponseCode::SERVER_ERROR;
    } else if (resultNum == TRI_ERROR_HTTP_NOT_FOUND || resultNum == TRI_ERROR_FILE_NOT_FOUND) {
      code = rest::ResponseCode::NOT_FOUND;
    } else {
      code = rest::ResponseCode::BAD;
    }
    generateError(code, result.errorNumber(), result.errorMessage());
    return RestStatus::DONE;
  }

  // This is awkward, the idea is as follows: For upload and download
  // requests, we have three cases, in which the good response code needs
  // to be chosen carefully:
  //  (1) An upload or download operation was scheduled    ==> ACCEPTED 202
  //  (2) An operation was aborted                         ==> ACCEPTED 202
  //  (3) Progress about an upload or download was queries ==> OK 200
  // This is, because (1) and (2) will only complete later, but (3) is
  // finished when the result is returned. This explains the following
  // logic. Note that the payload will always be an object, but we check
  // for the sake of completeness.
  rest::ResponseCode goodCode = rest::ResponseCode::OK;
  if (suffixes.front() == "create") {
    goodCode = rest::ResponseCode::CREATED;
  } else if (suffixes.front() == "upload" || suffixes.front() == "download") {
    if (!payload.isObject() ||
        payload.hasKey("abort") ||
        !(payload.hasKey("uploadId") && payload.hasKey("downloadId"))) {
      goodCode = rest::ResponseCode::ACCEPTED;
    }
  }

  VPackBuilder display;
  {
    VPackObjectBuilder o(&display);
    display.add("error", VPackValue(false));
    display.add("code", VPackValue(static_cast<uint32_t>(goodCode)));
    display.add("result", report.slice());
  }
  generateResult(goodCode, display.slice());

  return RestStatus::DONE;

} // RestHotBackupHandler::execute


/// @brief check for administrator rights and rocksdb storage engine
///        generate the error response if issues found
arangodb::Result RestHotBackupHandler::verifyPermitted() {

  // do we have admin rights (if rights are active)
  if (nullptr != ExecContext::CURRENT && !ExecContext::CURRENT->isAdminUser()) {
    return arangodb::Result(
      TRI_ERROR_HTTP_FORBIDDEN, "you need admin rights for hotbackup operations");
  } // if

  return arangodb::Result();

} // RestHotBackupHandler::verifyPermitted


arangodb::Result RestHotBackupHandler::parseHotBackupParams(
  RequestType const type, std::vector<std::string> const & suffixes,
  VPackSlice& slice) {

  if (type != RequestType::POST) {
    return arangodb::Result(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                            "backup endpoint only handles POST requests");
  }

  if (suffixes.size() != 1) {
    return arangodb::Result(
      TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
      "backup API only takes a single additional suffix out of "
#ifdef USE_ENTERPRISE
      "[create, delete, list, upload, download]"
#else
      "[create, delete, list]"
#endif
      );
  }

  bool parseSuccess = false;
  slice = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    return arangodb::Result(
      TRI_ERROR_HTTP_CORRUPTED_JSON, "failed to parse backup body");
  }

  return arangodb::Result();


} // RestHotBackupHander::parseHotBackupParams
