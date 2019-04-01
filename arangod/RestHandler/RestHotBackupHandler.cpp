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
  if (verifyPermitted()) {
    // extract the request specifics
    RequestType const type = _request->requestType();
    std::vector<std::string> const& suffixes = _request->suffixes();
    std::shared_ptr<RocksDBHotBackup> operation(parseHotBackupParams(type, suffixes));

    if (operation) {
      if (operation->valid()) {
        operation->execute();
      } // if

      // if !valid() then !success() already set
      if (operation->success()) {
        if (operation->result().isEmpty()) {
          generateOk(rest::ResponseCode::OK, velocypack::Slice::emptyObjectSlice());
        } else {
          generateOk(rest::ResponseCode::OK, operation->resultSlice());
        } // else
      } else {
        if (!operation->result().isEmpty()) {
          std::string msg = "Error, details: ";
          msg += operation->resultSlice().toJson();
          generateError(operation->restResponseCode(),
                        operation->restResponseError(), msg);
        } else if (operation->errorMessage().empty()) {
          generateError(operation->restResponseCode(),
                        operation->restResponseError());
        } else {
          generateError(operation->restResponseCode(),
                        operation->restResponseError(),
                        operation->errorMessage());
        } // else
      } // else
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    } // else
  } // if

  return RestStatus::DONE;

} // RestHotBackupHandler::execute


/// @brief check for administrator rights and rocksdb storage engine
///        generate the error response if issues found
bool RestHotBackupHandler::verifyPermitted() {
  bool retFlag{true};

  // first test:  do we have admin rights (if rights are active)
  if (nullptr != ExecContext::CURRENT && !ExecContext::CURRENT->isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "you need admin rights for hotbackup operations");
    retFlag = false;
  } // if

  // second test:  is rocksdb the storage engine
  if (retFlag) {
    // test for rocksdb engine
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine != nullptr);
    if (!EngineSelectorFeature::isRocksDB()) {
      generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                    "hotbackup current supports only the rocksdb storage engine");
      retFlag = false;
    } // if
  } // if

  return retFlag;

} // RestHotBackupHandler::verifyPermitted


std::shared_ptr<RocksDBHotBackup> RestHotBackupHandler::parseHotBackupParams(
  RequestType const type, std::vector<std::string> const & suffixes) {

  std::shared_ptr<RocksDBHotBackup> operation;
  bool parseSuccess;

  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (parseSuccess) {
    operation = RocksDBHotBackup::operationFactory(type, suffixes, body);
  } // if

  return operation;

} // RestHotBackupHander::parseHotBackupParams
