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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "RestPregel3Handler.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "GeneralServer/RestHandler.h"
#include "Logger/LogMacros.h"
#include "Rest/CommonDefines.h"

#include "Pregel3/Methods.h"

using namespace arangodb;
using namespace arangodb::pregel3;

RestPregel3Handler::RestPregel3Handler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _pregel3Feature(server.getFeature<Pregel3Feature>()) {}

RestPregel3Handler::~RestPregel3Handler() = default;

auto RestPregel3Handler::execute() -> RestStatus {
  // depends on ServerState::RoleEnum
  auto methods = Pregel3Methods::createInstance(_vocbase);
  return executeByMethod(*methods);
}

auto RestPregel3Handler::executeByMethod(Pregel3Methods const& methods)
    -> RestStatus {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

auto RestPregel3Handler::handlePostRequest(
    pregel3::Pregel3Methods const& methods) -> RestStatus {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  // todo check that suffixes = ["query"]

  auto generateErrorWrongInput = [&](std::string const& info = "") -> void {
    generateError(rest::ResponseCode::BAD, ErrorCode(TRI_ERROR_BAD_PARAMETER),
                  Utils::wrongRequestBody + std::string(" ") + info);
  };

  // get the body of the request
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !body.isObject()) {
    generateErrorWrongInput("Malformed JSON document.");
    return RestStatus::DONE;
  }

  // parse the body of the request
  // read queryId if it is given
  std::string queryId;
  if (body.hasKey(Utils::queryId)) {
    auto queryIdSlice = body.get(Utils::queryId);
    if (!queryIdSlice.isString()) {
      generateErrorWrongInput("The value of " + std::string(Utils::queryId) +
                              " is not of type String.");
      return RestStatus::DONE;
    }
    if (methods.getPregel3Feature()->hasQueryId(queryId)) {
      generateErrorWrongInput(" Query id " + queryId +
                              " exists already. Please, choose another one.");
    }
  } else {
    // otherwise generate a new queryId
    queryId = methods.getPregel3Feature()->generateQueryId();
  }

  // read the graph specification
  if (!body.hasKey(Utils::graphSpec)) {
    generateErrorWrongInput("The graph is not specified.");
    return RestStatus::DONE;
  }
  auto graphSpec =
      GraphSpecification::fromVelocyPack(body.get(Utils::graphSpec));

  _pregel3Feature.createQuery(queryId, graphSpec);

  // send the answer
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add(Utils::queryId, VPackValue(queryId));
  }
  generateOk(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}

auto RestPregel3Handler::handleGetRequest(
    const pregel3::Pregel3Methods& methods) -> RestStatus {
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);

    builder.add(VPackValue("version"));
    builder.add(VPackValue("v3"));
  }
  generateOk(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}
