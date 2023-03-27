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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RestControlPregelHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Inspection/VPackWithErrorT.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/REST/RestOptions.h"
#include "Transaction/StandaloneContext.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/SharedSlice.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

RestControlPregelHandler::RestControlPregelHandler(ArangodServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _pregel(server.getFeature<pregel::PregelFeature>()) {}

RestStatus RestControlPregelHandler::execute() {
  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::POST: {
      startExecution();
      break;
    }
    case rest::RequestType::GET: {
      handleGetRequest();
      break;
    }
    case rest::RequestType::DELETE_REQ: {
      handleDeleteRequest();
      break;
    }
    default: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
  }
  return RestStatus::DONE;
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>>
RestControlPregelHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  rest::RequestType const type = _request->requestType();
  if (type != rest::RequestType::POST && type != rest::RequestType::GET &&
      type != rest::RequestType::DELETE_REQ) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 1) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  // Do NOT forward requests to any other arangod instance in case we're
  // requesting the history API. Any coordinator is able to handle this
  // request.
  if (suffixes.size() >= 1 && suffixes.at(0) == "history") {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  if (sourceServer == ServerState::instance()->getShortId()) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto coordinatorId = ci.getCoordinatorByShortID(sourceServer);

  if (coordinatorId.empty()) {
    return ResultT<std::pair<std::string, bool>>::error(
        TRI_ERROR_CURSOR_NOT_FOUND, "cannot find target server for pregel id");
  }

  return {std::make_pair(std::move(coordinatorId), false)};
}

void RestControlPregelHandler::startExecution() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  auto restOptions = inspection::deserializeWithErrorT<pregel::RestOptions>(
      velocypack::SharedSlice(velocypack::SharedSlice{}, body));
  if (!restOptions.ok()) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  restOptions.error().error());
  }
  auto options = std::move(restOptions).get().options();

  auto res = _pregel.startExecution(_vocbase, options);
  if (res.fail()) {
    generateError(res.result());
    return;
  }

  VPackBuilder builder;
  builder.add(VPackValue(std::to_string(res.get().value)));
  generateResult(rest::ResponseCode::OK, builder.slice());
}

void RestControlPregelHandler::handleGetRequest() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.empty()) {
    bool const allDatabases = _request->parsedValue("all", false);
    bool const fanout = ServerState::instance()->isCoordinator() &&
                        !_request->parsedValue("local", false);

    VPackBuilder builder;
    _pregel.toVelocyPack(_vocbase, builder, allDatabases, fanout);
    generateResult(rest::ResponseCode::OK, builder.slice());
    return;
  }

  if (suffixes.size() == 1 && suffixes.at(0) != "history") {
    if (suffixes[0].empty()) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
          "superfluous parameter, expecting /_api/control_pregel[/<id>]");
      return;
    }
    auto executionNumber = arangodb::pregel::ExecutionNumber{
        arangodb::basics::StringUtils::uint64(suffixes[0])};
    auto c = _pregel.conductor(executionNumber);

    if (nullptr == c) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                    "Execution number is invalid");
      return;
    }

    VPackBuilder builder;
    c->toVelocyPack(builder);
    generateResult(rest::ResponseCode::OK, builder.slice());
    return;
  } else if ((suffixes.size() >= 1 || suffixes.size() <= 2) &&
             suffixes.at(0) == "history") {
    if (suffixes.size() == 1) {
      return handlePregelHistoryResult(_pregel.handleHistoryRequest(
          _vocbase, _request->requestType(), std::nullopt));
    } else {
      auto executionNumber = arangodb::pregel::ExecutionNumber{
          arangodb::basics::StringUtils::uint64(suffixes.at(1))};
      return handlePregelHistoryResult(_pregel.handleHistoryRequest(
          _vocbase, _request->requestType(), executionNumber));
    }
  }

  generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                "expecting one of the resources /_api/control_pregel[/<id>] or "
                "/_api/control_pregel/history[/<id>]");
}

void RestControlPregelHandler::handlePregelHistoryResult(
    ResultT<OperationResult> result) {
  if (result.fail()) {
    // check outer ResultT result
    generateError(rest::ResponseCode::BAD, result.errorNumber(),
                  result.errorMessage());
    return;
  }
  if (result.get().fail()) {
    // check inner OperationResult
    std::string message = std::string{result.get().errorMessage()};
    if (result.get().errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      // For reasons, not all OperationResults deliver the expected message.
      // Therefore, we need set up the message properly and manually here.
      message = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND).errorMessage();
    }

    ResponseCode code =
        GeneralResponse::responseCode(result.get().errorNumber());
    generateError(code, result.get().errorNumber(), message);
    return;
  }

  if (result->hasSlice()) {
    if (result->slice().isNone()) {
      // Truncate does not deliver a proper slice in a Cluster.
      generateResult(rest::ResponseCode::OK, VPackSlice::trueSlice());
    } else {
      generateResult(rest::ResponseCode::OK, result.get().slice());
    }
  } else {
    // Should always have a Slice, doing this check to be sure.
    // (e.g. a truncate might not return a Slice in SingleServer)
    generateResult(rest::ResponseCode::OK, VPackSlice::trueSlice());
  }
}

void RestControlPregelHandler::handleDeleteRequest() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if ((suffixes.size() >= 1 || suffixes.size() <= 2) &&
      suffixes.at(0) == "history") {
    if (suffixes.size() == 1) {
      return handlePregelHistoryResult(_pregel.handleHistoryRequest(
          _vocbase, _request->requestType(), std::nullopt));
    } else {
      auto executionNumber = arangodb::pregel::ExecutionNumber{
          arangodb::basics::StringUtils::uint64(suffixes.at(1))};
      return handlePregelHistoryResult(_pregel.handleHistoryRequest(
          _vocbase, _request->requestType(), executionNumber));
    }
  }

  if ((suffixes.size() != 1) || suffixes[0].empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "bad parameter, expecting /_api/control_pregel/<id> or "
                  "/_api/control_pregel/history[/<id>]");
    return;
  }

  auto executionNumber = arangodb::pregel::ExecutionNumber{
      arangodb::basics::StringUtils::uint64(suffixes[0])};
  auto c = _pregel.conductor(executionNumber);

  if (nullptr == c) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "Execution number is invalid");
    return;
  }

  c->cancel();

  VPackBuilder builder;
  builder.add(VPackValue(""));
  generateResult(rest::ResponseCode::OK, builder.slice());
}

}  // namespace arangodb
