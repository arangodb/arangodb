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
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHotBackupHandler::RestHotBackupHandler(GeneralRequest* request, GeneralResponse* response)
  : RestBaseHandler(request, response) {

}

RestStatus RestHotBackupHandler::execute() {
  if (nullptr == ExecContext::CURRENT || ExecContext::CURRENT->isAdminUser()) {

    // extract the request specifics
    RequestType const type = _request->requestType();
    std::vector<std::string> const& suffixes = _request->suffixes();
    std::shared_ptr<RocksDBHotBackup> operation(parseHotBackupParams(type, suffixes));

    /// TODO: add test for rocksdb engine

    auto reportError = [&]() {
      if (!operation->result().isEmpty()) {
        std::string msg = operation->resultSlice().toJson();
        generateError(operation->restResponseCode(),
                      operation->restResponseError(), msg);
      } else if (operation->errorMessage().empty()) {
        generateError(operation->restResponseCode(),
                      operation->restResponseError());
      } else {
        generateError(operation->restResponseCode(),
                      operation->restResponseError(),
                      operation->errorMessage());
      }
    };

    if (operation) {
      if (!operation->valid()) {
        reportError();
        return RestStatus::DONE;
      }
      operation->execute();


    if (ServerState::instance()->isCoordinator()) {
      
      // Coordinator job:
      // 1. Make mark in agency & Kill supervision in one transaction
      // 2. Try to get global lock
      // 3. Make backup
      Result result = hotBackupCoordinator(arangodb::CONSISTENT, 60.0);
      if (result.ok()) {
        generateResult(rest::ResponseCode::OK, VPackSlice());
      }
      
      
    } else {
      std::shared_ptr<RocksDBHotBackup> operation(parseHotBackupParams(type, suffixes));
      
      /// add test for rocksdb engine
      
      if (operation && operation->valid()) {
        operation->execute();
        
        if (operation->success()) {
          generateResult(rest::ResponseCode::OK, operation->resultSlice());
        } else {
          generateError(operation->restResponseCode(),
                        operation->restResponseError());
        } // else
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      } // else
      

      /// add test for rocksdb engine
    
      auto reportError = [&]() {
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
                           }
                         };

      if (operation) {
        if (!operation->valid()) {
          reportError();
          return RestStatus::DONE;
        }
        operation->execute();

        if (operation->success()) {
          generateOk(rest::ResponseCode::OK, operation->resultSlice());
        } else {
          reportError();
        }
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      } // else
    }
    
  } else {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "you need admin rights for hotbackup operations");
  } // else

  return RestStatus::DONE;
  
} // RestHotBackupHandler::execute


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
