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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "RestHandler/RestDocumentStateHandler.h"

#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentStateMethods.h"

namespace arangodb {

RestDocumentStateHandler::RestDocumentStateHandler(ArangodServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestDocumentStateHandler::execute() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto methods = replication2::DocumentStateMethods::createInstance(_vocbase);
  return executeByMethod(*methods);
}

RestStatus RestDocumentStateHandler::executeByMethod(
    replication2::DocumentStateMethods const& methods) {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

RestStatus RestDocumentStateHandler::handleGetRequest(
    replication2::DocumentStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/document-state/<state-id>");
    return RestStatus::DONE;
  }

  replication2::LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/document-state/<state-id>/[verb]");
    return RestStatus::DONE;
  }
  auto const& verb = suffixes[1];
  if (verb == "snapshot") {
    return handleGetSnapshot(methods, logId);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources: 'snapshot'");
  }
  return RestStatus::DONE;
}

RestStatus RestDocumentStateHandler::handleGetSnapshot(
    replication2::DocumentStateMethods const& methods,
    replication2::LogId logId) {
  auto waitForIndexParam =
      _request->parsedValue<decltype(replication2::LogIndex::value)>(
          "waitForIndex");
  if (!waitForIndexParam.has_value()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid waitForIndex parameter");
    return RestStatus::DONE;
  }

  auto waitForIndex = replication2::LogIndex{waitForIndexParam.value()};
  return waitForFuture(methods.getSnapshot(logId, waitForIndex)
                           .thenValue([this](auto&& waitForResult) {
                             if (waitForResult.fail()) {
                               generateError(waitForResult.result());
                             } else {
                               generateOk(rest::ResponseCode::OK,
                                          waitForResult.get().slice());
                             }
                           }));
}
}  // namespace arangodb
