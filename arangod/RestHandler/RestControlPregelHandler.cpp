////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Cluster/ServerState.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Rest/HttpRequest.h"
#include "Transaction/StandaloneContext.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Tasks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

RestControlPregelHandler::RestControlPregelHandler(GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

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
uint32_t RestControlPregelHandler::forwardingTarget() {
  rest::RequestType const type = _request->requestType();
  if (type != rest::RequestType::POST && type != rest::RequestType::GET &&
      type != rest::RequestType::DELETE_REQ) {
    return 0;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 1) {
    return 0;
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  return (sourceServer == ServerState::instance()->getShortId()) ? 0 : sourceServer;
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
      VelocyPackHelper::getStringValue(body, "algorithm", "");
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
      generateError(graphRes.copy_result());
      return;
    }
    std::unique_ptr<graph::Graph> graph = std::move(graphRes.get());

    auto gv = graph->vertexCollections();
    for (auto& v : gv) {
      vertexCollections.push_back(v);
    }

    auto ge = graph->edgeCollections();
    for (auto& e : ge) {
      edgeCollections.push_back(e);
    }
  }

  auto res = pregel::PregelFeature::startExecution(_vocbase, algorithm, vertexCollections,
                                                   edgeCollections, parameters);
  if (res.first.fail()) {
    generateError(res.first);
    return;
  }

  VPackBuilder builder;
  builder.add(VPackValue(res.second));
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
  auto c = pregel::PregelFeature::instance()->conductor(executionNumber);

  if (nullptr == c) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_INTERNAL,
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

  auto pf = pregel::PregelFeature::instance();
  if (nullptr == pf) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "pregel feature not available");
    return;
  }

  uint64_t executionNumber = arangodb::basics::StringUtils::uint64(suffixes[0]);
  auto c = pf->conductor(executionNumber);

  if (nullptr == c) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_INTERNAL,
                  "Execution number is invalid");
    return;
  }

  c->cancel();
  pf->cleanupConductor(executionNumber);

  VPackBuilder builder;
  builder.add(VPackValue(""));
  generateResult(rest::ResponseCode::OK, builder.slice());
}

}  // namespace arangodb
