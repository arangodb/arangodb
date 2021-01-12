////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Transaction/StandaloneContext.h"
#include "V8/v8-vpack.h"
#include "VocBase/Methods/Tasks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

RestControlPregelHandler::RestControlPregelHandler(application_features::ApplicationServer& server,
                                                   GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestControlPregelHandler::execute() {
  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::POST: {
      startExecution();
      break;
    }
    case rest::RequestType::GET: {
      getExecutionStatus();
      break;
    }
    case rest::RequestType::DELETE_REQ: {
      cancelExecution();
      break;
    }
    default: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
  }
  return RestStatus::DONE;
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestControlPregelHandler::forwardingTarget() {
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

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  if (sourceServer == ServerState::instance()->getShortId()) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  return {std::make_pair(ci.getCoordinatorByShortID(sourceServer), false)};
}

void RestControlPregelHandler::startExecution() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  // algorithm
  std::string algorithm =
      VelocyPackHelper::getStringValue(body, "algorithm", StaticStrings::Empty);
  if ("" == algorithm) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "invalid algorithm");
    return;
  }

  // extract the parameters
  auto parameters = body.get("params");
  if (!parameters.isObject()) {
    parameters = VPackSlice::emptyObjectSlice();
  }

  // extract the collections
  std::vector<std::string> vertexCollections;
  std::vector<std::string> edgeCollections;
  std::unordered_map<std::string, std::vector<std::string>> edgeCollectionRestrictions;
  auto vc = body.get("vertexCollections");
  auto ec = body.get("edgeCollections");
  if (vc.isArray() && ec.isArray()) {
    for (auto v : VPackArrayIterator(vc)) {
      vertexCollections.push_back(v.copyString());
    }
    for (auto e : VPackArrayIterator(ec)) {
      edgeCollections.push_back(e.copyString());
    }
  } else {
    auto gs = VelocyPackHelper::getStringValue(body, "graphName", "");
    if ("" == gs) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "expecting graphName as string");
      return;
    }

    graph::GraphManager gmngr{_vocbase};
    auto graphRes = gmngr.lookupGraphByName(gs);
    if (graphRes.fail()) {
      generateError(std::move(graphRes).result());
      return;
    }
    std::unique_ptr<graph::Graph> graph = std::move(graphRes.get());

    auto const& gv = graph->vertexCollections();
    for (auto const& v : gv) {
      vertexCollections.push_back(v);
    }

    auto const& ge = graph->edgeCollections();
    for (auto const& e : ge) {
      edgeCollections.push_back(e);
    }

    auto const& ed = graph->edgeDefinitions();
    for (auto const& e : ed) {
      auto const& from = e.second.getFrom();
      // intentionally create map entry
      for (auto const& f : from) {
        auto& restrictions = edgeCollectionRestrictions[f];
        restrictions.push_back(e.second.getName());
      }
    }
  }

  auto res = pregel::PregelFeature::startExecution(_vocbase, algorithm, vertexCollections,
                                                   edgeCollections, edgeCollectionRestrictions, parameters);
  if (res.first.fail()) {
    generateError(res.first);
    return;
  }

  VPackBuilder builder;
  builder.add(VPackValue(std::to_string(res.second)));
  generateResult(rest::ResponseCode::OK, builder.slice());
}

void RestControlPregelHandler::getExecutionStatus() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 1 || suffixes[0].empty()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
        "superfluous parameter, expecting /_api/control_pregel[/<id>]");
    return;
  }

  uint64_t executionNumber = arangodb::basics::StringUtils::uint64(suffixes[0]);
  std::shared_ptr<pregel::PregelFeature> pf = pregel::PregelFeature::instance();
  if (!pf) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "pregel feature not available");
    return;
  }
  auto c = pf->conductor(executionNumber);

  if (nullptr == c) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "Execution number is invalid");
    return;
  }

  VPackBuilder builder = c->toVelocyPack();
  generateResult(rest::ResponseCode::OK, builder.slice());
}

void RestControlPregelHandler::cancelExecution() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if ((suffixes.size() != 1) || suffixes[0].empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "bad parameter, expecting /_api/control_pregel/<id>");
    return;
  }

  std::shared_ptr<pregel::PregelFeature> pf = pregel::PregelFeature::instance();
  if (nullptr == pf) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "pregel feature not available");
    return;
  }

  uint64_t executionNumber = arangodb::basics::StringUtils::uint64(suffixes[0]);
  auto c = pf->conductor(executionNumber);

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
