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
#include "Pregel/Conductor/Messages.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/REST/RestOptions.h"
#include "Pregel/StatusWriter/CollectionStatusWriter.h"
#include "Pregel/StatusActor.h"
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

  // We only need to support forwarding in case we want to cancel a running
  // pregel job.
  if (type != rest::RequestType::DELETE_REQ) {
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

namespace {
auto extractPregelOptions(VPackSlice body) -> ResultT<pregel::PregelOptions> {
  // emulate 3.10 pregel API
  // algorithm
  std::string algorithm =
      VelocyPackHelper::getStringValue(body, "algorithm", StaticStrings::Empty);
  if ("" == algorithm) {
    return Result{TRI_ERROR_HTTP_NOT_FOUND, "invalid algorithm"};
  }
  // extract the parameters
  auto parameters = body.get("params");
  if (!parameters.isObject()) {
    parameters = VPackSlice::emptyObjectSlice();
  }
  VPackBuilder parameterBuilder;
  parameterBuilder.add(parameters);
  // extract the collections
  auto vc = body.get("vertexCollections");
  auto ec = body.get("edgeCollections");
  pregel::PregelOptions options;
  if (vc.isArray() && ec.isArray()) {
    std::vector<std::string> vertexCollections;
    for (auto v : VPackArrayIterator(vc)) {
      vertexCollections.push_back(v.copyString());
    }
    std::vector<std::string> edgeCollections;
    for (auto e : VPackArrayIterator(ec)) {
      edgeCollections.push_back(e.copyString());
    }
    return pregel::PregelOptions{
        .algorithm = algorithm,
        .userParameters = parameterBuilder,
        .graphSource = {{pregel::GraphCollectionNames{
                            .vertexCollections = vertexCollections,
                            .edgeCollections = edgeCollections}},
                        {}}};
  } else {
    auto gs = VelocyPackHelper::getStringValue(body, "graphName", "");
    if ("" == gs) {
      return Result{TRI_ERROR_BAD_PARAMETER, "expecting graphName as string"};
    }
    return pregel::PregelOptions{
        .algorithm = algorithm,
        .userParameters = parameterBuilder,
        .graphSource = {{pregel::GraphName{.graph = gs}}, {}}};
  }

  // this should be used for a more restrictive pregel API
  // that would make more sense
  // auto restOptions =
  // inspection::deserializeWithErrorT<pregel::RestOptions>(
  //     velocypack::SharedSlice(velocypack::SharedSlice{}, body));
  // if (!restOptions.ok()) {
  //   return Result{TRI_ERROR_HTTP_NOT_FOUND,
  //                 fmt::format("Given pregel options are invalid: {}",
  //                             restOptions.error().error())};
  // }
  // return std::move(restOptions).get().options();
}

}  // namespace

void RestControlPregelHandler::startExecution() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  auto options = extractPregelOptions(std::move(body));
  if (options.fail()) {
    if (options.errorNumber() == TRI_ERROR_HTTP_NOT_FOUND) {
      generateError(rest::ResponseCode::NOT_FOUND, options.errorNumber(),
                    options.errorMessage());
      return;
    }
    generateError(rest::ResponseCode::BAD, options.errorNumber(),
                  options.errorMessage());
    return;
  }
  auto res = _pregel.startExecution(_vocbase, options.get());
  if (res.fail()) {
    generateError(res.result());
    return;
  }

  VPackBuilder builder;
  builder.add(VPackValue(std::to_string(res.get().value)));
  generateResult(rest::ResponseCode::OK, builder.slice());
}

RestControlPregelHandler::RequestParse
RestControlPregelHandler::parseRequestSuffixes(
    std::vector<std::string> const& suffixes) {
  if (suffixes.empty() or
      (suffixes.at(0) == "history" and suffixes.size() == 1)) {
    pregel::statuswriter::CollectionStatusWriter cWriter{_vocbase};
    return All{};
  }

  // Here suffixes.size() > 0
  auto executionNumberString = [&]() -> std::optional<std::string> {
    if (suffixes.at(0) == "history" && suffixes.size() == 2) {
      return suffixes.at(1);
    }
    if (suffixes.size() == 1) {
      return suffixes.at(0);
    }
    return std::nullopt;
  }();

  if (executionNumberString.has_value()) {
    auto maybeNumber = basics::StringUtils::try_uint64(*executionNumberString);

    if (maybeNumber.ok()) {
      return pregel::ExecutionNumber{maybeNumber.get()};
    } else {
      return maybeNumber.result();
    }
  }
  return Result{TRI_ERROR_BAD_PARAMETER};
}

void RestControlPregelHandler::handlePregelHistoryResult(
    ResultT<OperationResult> result, bool onlyReturnFirstAqlResultEntry) {
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
      if (onlyReturnFirstAqlResultEntry) {
        TRI_ASSERT(result->slice().isArray());
        if (result.get().slice().at(0).isNull()) {
          // due to AQL returning "null" values in case a document does not
          // exist ....
          Result nf = Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
          ResponseCode code = GeneralResponse::responseCode(nf.errorNumber());
          generateError(code, nf.errorNumber(), nf.errorMessage());
        } else {
          generateResult(rest::ResponseCode::OK, result.get().slice().at(0));
        }
      } else {
        generateResult(rest::ResponseCode::OK, result.get().slice());
      }
    }
  } else {
    // Should always have a Slice, doing this check to be sure.
    // (e.g. a truncate might not return a Slice in SingleServer)
    generateResult(rest::ResponseCode::OK, VPackSlice::trueSlice());
  }
}

void RestControlPregelHandler::handleGetRequest() {
  if (_pregel.isStopping()) {
    return handlePregelHistoryResult({Result(TRI_ERROR_SHUTTING_DOWN)});
  }

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  auto parse = parseRequestSuffixes(suffixes);

  if (std::holds_alternative<All>(parse)) {
    pregel::statuswriter::CollectionStatusWriter cWriter{_vocbase};
    return handlePregelHistoryResult(cWriter.readAllNonExpiredResults());
  }
  if (std::holds_alternative<pregel::ExecutionNumber>(parse)) {
    pregel::statuswriter::CollectionStatusWriter cWriter{
        _vocbase, std::get<pregel::ExecutionNumber>(parse)};
    return handlePregelHistoryResult(cWriter.readResult(), true);
  }
  generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                "expecting one of the resources /_api/control_pregel[/<id>] or "
                "/_api/control_pregel/history[/<id>]");
}

void RestControlPregelHandler::handleDeleteRequest() {
  if (_pregel.isStopping()) {
    return handlePregelHistoryResult({Result(TRI_ERROR_SHUTTING_DOWN)});
  }
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  auto parse = parseRequestSuffixes(suffixes);

  if (suffixes.size() >= 1 and suffixes.at(0) == "history") {
    if (std::holds_alternative<All>(parse)) {
      pregel::statuswriter::CollectionStatusWriter cWriter{_vocbase};
      return handlePregelHistoryResult(cWriter.deleteAllResults());
    }

    if (std::holds_alternative<pregel::ExecutionNumber>(parse)) {
      pregel::statuswriter::CollectionStatusWriter cWriter{
          _vocbase, std::get<pregel::ExecutionNumber>(parse)};
      return handlePregelHistoryResult(cWriter.deleteResult());
    }
  }

  if (std::holds_alternative<pregel::ExecutionNumber>(parse)) {
    auto canceled = _pregel.cancel(std::get<pregel::ExecutionNumber>(parse));
    if (canceled.fail()) {
      generateError(rest::ResponseCode::NOT_FOUND, canceled.errorNumber(),
                    canceled.errorMessage());
      return;
    }
    VPackBuilder builder;
    builder.add(VPackValue(""));
    generateResult(rest::ResponseCode::OK, builder.slice());
  }

  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                "bad parameter, expecting /_api/control_pregel/<id> or "
                "/_api/control_pregel/history[/<id>]");
}

}  // namespace arangodb
