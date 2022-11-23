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
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"

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
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

RestStatus RestDocumentStateHandler::handleGetRequest(
    replication2::DocumentStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 3) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expect GET /_api/document-state/<state-id>/snapshot/<action>");
    return RestStatus::DONE;
  }

  std::optional<replication2::LogId> logId =
      replication2::LogId::fromString(suffixes[0]);
  if (!logId.has_value()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        fmt::format(
            "invalid state id {} during GET /_api/document-state/<state-id>",
            suffixes[0]));
    return RestStatus::DONE;
  }

  auto const& verb = suffixes[1];
  if (verb == "snapshot") {
    return processSnapshotRequest(methods, logId.value(),
                                  parseGetSnapshotParams());
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources: 'snapshot'");
  }
  return RestStatus::DONE;
}

auto RestDocumentStateHandler::parseGetSnapshotParams()
    -> ResultT<replication2::replicated_state::document::SnapshotParams> {
  using namespace replication2::replicated_state;

  auto const& suffixes = _request->suffixes();
  if (suffixes[2] == "status") {
    if (suffixes.size() < 4) {
      return document::SnapshotParams{document::SnapshotParams::Status{}};
    }
    auto id = document::SnapshotId::fromString(suffixes[3]);
    if (!id.has_value()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          fmt::format("Invalid snapshot id: {}", suffixes[3]));
    }
    return document::SnapshotParams{document::SnapshotParams::Status{*id}};
  }
  return ResultT<document::SnapshotParams>::error(
      TRI_ERROR_HTTP_BAD_PARAMETER,
      "expect GET one of the following actions: status");
}

RestStatus RestDocumentStateHandler::handlePostRequest(
    replication2::DocumentStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 3) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expect POST /_api/document-state/<state-id>/snapshot/<action>");
    return RestStatus::DONE;
  }

  std::optional<replication2::LogId> logId =
      replication2::LogId::fromString(suffixes[0]);
  if (!logId.has_value()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        fmt::format(
            "invalid state id {} during POST /_api/document-state/<state-id>",
            suffixes[0]));
    return RestStatus::DONE;
  }

  auto const& verb = suffixes[1];
  if (verb == "snapshot") {
    return processSnapshotRequest(methods, logId.value(),
                                  parsePostSnapshotParams());
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources: 'snapshot'");
  }
  return RestStatus::DONE;
}

auto RestDocumentStateHandler::parsePostSnapshotParams()
    -> ResultT<replication2::replicated_state::document::SnapshotParams> {
  using namespace replication2::replicated_state;

  auto const& suffixes = _request->suffixes();
  if (suffixes[2] == "start") {
    if (suffixes.size() != 3) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("expect POST "
                      "/_api/document-state/<state-id>/snapshot/"
                      "start?waitForIndex=<index>"));
    }

    auto waitForIndexParam =
        _request->parsedValue<decltype(replication2::LogIndex::value)>(
            "waitForIndex");
    if (!waitForIndexParam.has_value()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "invalid waitForIndex parameter, expect POST "
          "/_api/document-state/<state-id>/snapshot/"
          "start?waitForIndex=<index>");
    }

    return document::SnapshotParams{document::SnapshotParams::Start{
        .waitForIndex = replication2::LogIndex{*waitForIndexParam}}};
  } else if (suffixes[2] == "next") {
    if (suffixes.size() != 4) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "expect POST "
          "/_api/document-state/<state-id>/snapshot/next/<snapshot-id>");
    }

    auto id = document::SnapshotId::fromString(suffixes[3]);
    if (!id.has_value()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Invalid snapshot id: {}", suffixes[3]));
    }

    return document::SnapshotParams{document::SnapshotParams::Next{*id}};
  }

  return ResultT<document::SnapshotParams>::error(
      TRI_ERROR_HTTP_BAD_PARAMETER,
      "expect POST one of the following actions: 'start', 'next'");
}

RestStatus RestDocumentStateHandler::handleDeleteRequest(
    replication2::DocumentStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 3) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expect DELETE /_api/document-state/<state-id>/snapshot/<action>");
    return RestStatus::DONE;
  }

  std::optional<replication2::LogId> logId =
      replication2::LogId::fromString(suffixes[0]);
  if (!logId.has_value()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        fmt::format(
            "invalid state id {} during DELETE /_api/document-state/<state-id>",
            suffixes[0]));
    return RestStatus::DONE;
  }

  auto const& verb = suffixes[1];
  if (verb == "snapshot") {
    return processSnapshotRequest(methods, logId.value(),
                                  parseDeleteSnapshotParams());
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources: 'snapshot'");
  }
  return RestStatus::DONE;
}

auto RestDocumentStateHandler::parseDeleteSnapshotParams()
    -> ResultT<replication2::replicated_state::document::SnapshotParams> {
  using namespace replication2::replicated_state;

  auto const& suffixes = _request->suffixes();
  if (suffixes[2] == "finish") {
    if (suffixes.size() != 4) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("expect DELETE "
                      "/_api/document-state/<state-id>/snapshot/"
                      "finish/<snapshot-id>"));
    }

    auto id = document::SnapshotId::fromString(suffixes[3]);
    if (!id.has_value()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Invalid snapshot id: {}", suffixes[3]));
    }

    return document::SnapshotParams{document::SnapshotParams::Finish{*id}};
  }

  return ResultT<document::SnapshotParams>::error(
      TRI_ERROR_HTTP_BAD_PARAMETER,
      "expect DELETE one of the following actions: 'finish'");
}

RestStatus RestDocumentStateHandler::processSnapshotRequest(
    replication2::DocumentStateMethods const& methods,
    replication2::LogId const& logId,
    ResultT<replication2::replicated_state::document::SnapshotParams>&&
        params) {
  if (params.fail()) {
    generateError(params.result());
    return RestStatus::DONE;
  }
  auto result = methods.processSnapshotRequest(logId, *params);
  if (result.fail()) {
    generateError(result.result());
  } else {
    generateOk(rest::ResponseCode::OK, result.get().slice());
  }
  return RestStatus::DONE;
}
}  // namespace arangodb
