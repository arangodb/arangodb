////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentStateMethods.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Transaction/Helpers.h"
#include "Utils/CollectionNameResolver.h"

#include <optional>
#include <velocypack/Dumper.h>

namespace arangodb {

namespace {
/*
 * CustomTypeHandler to parse the collection ID from snapshot batches.
 */
struct SnapshotTypeHandler final : public VPackCustomTypeHandler {
  explicit SnapshotTypeHandler(TRI_vocbase_t& vocbase)
      : resolver(CollectionNameResolver(vocbase)) {}

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) final {
    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) final {
    return transaction::helpers::extractIdString(&resolver, value, base);
  }

  CollectionNameResolver resolver;
};
}  // namespace

RestDocumentStateHandler::RestDocumentStateHandler(ArangodServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {
  _customTypeHandler = std::make_unique<SnapshotTypeHandler>(_vocbase);
  _options = VPackOptions::Defaults;
  _options.customTypeHandler = _customTypeHandler.get();
}

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
  if (suffixes.size() < 2) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expect GET /_api/document-state/<state-id>/[shards|snapshot]");
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
    if (suffixes.size() < 3) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          "expect GET /_api/document-state/<state-id>/snapshot/<action>");
      return RestStatus::DONE;
    }
    auto params = parseGetSnapshotParams();
    if (params.fail()) {
      generateError(rest::ResponseCode::BAD, params.result().errorNumber(),
                    params.result().errorMessage());
      return RestStatus::DONE;
    }
    return processSnapshotRequest(methods, logId.value(),
                                  parseGetSnapshotParams());
  } else if (verb == "shards") {
    auto shards = methods.getAssociatedShardList(logId.value());
    VPackBuilder builder;
    velocypack::serialize(builder, shards);
    generateOk(rest::ResponseCode::OK, builder.slice());
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
    if (id.fail()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          fmt::format("Invalid snapshot id: {}! Error: {}", suffixes[3],
                      id.result().errorMessage()));
    }
    return document::SnapshotParams{document::SnapshotParams::Status{id.get()}};
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
                      "start"));
    }

    bool parseSuccess{false};
    auto body = parseVPackBody(parseSuccess);
    if (!parseSuccess) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER, "could not parse body as VPack object");
    }

    return document::SnapshotParams{
        velocypack::deserialize<document::SnapshotParams::Start>(body)};
  } else if (suffixes[2] == "next") {
    if (suffixes.size() != 4) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "expect POST "
          "/_api/document-state/<state-id>/snapshot/next/<snapshot-id>");
    }

    auto id = document::SnapshotId::fromString(suffixes[3]);
    if (id.fail()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Invalid snapshot id: {}! Error: {}", suffixes[3],
                      id.result().errorMessage()));
    }

    return document::SnapshotParams{document::SnapshotParams::Next{id.get()}};
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
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  fmt::format("invalid state id {} during DELETE "
                              "/_api/document-state/<state-id>",
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
    if (id.fail()) {
      return ResultT<document::SnapshotParams>::error(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Invalid snapshot id: {}! Error: {}", suffixes[3],
                      id.result().errorMessage()));
    }

    return document::SnapshotParams{document::SnapshotParams::Finish{id.get()}};
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
    generateOk(rest::ResponseCode::OK, result.get().slice(), _options);
  }
  return RestStatus::DONE;
}
}  // namespace arangodb
