////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestTtlHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "RestServer/TtlFeature.h"
#include "VocBase/Methods/Ttl.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestTtlHandler::RestTtlHandler(application_features::ApplicationServer& server,
                               GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestTtlHandler::execute() {
  if (!_vocbase.isSystem()) {
    // ttl operations only allowed in _system database
    generateError(Result(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE));
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() == 1) {
    if (suffixes[0] == "properties") {
      return handleProperties();
    } else if (suffixes[0] == "statistics") {
      return handleStatistics();
    }
  }
   
  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  return RestStatus::DONE;
}

RestStatus RestTtlHandler::handleProperties() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    VPackBuilder builder;
    Result result =
        methods::Ttl::getProperties(_vocbase.server().getFeature<TtlFeature>(), builder);

    if (result.fail()) {
      generateError(result);
    } else {
      generateOk(rest::ResponseCode::OK, builder.slice());
    }

    return RestStatus::DONE;
  } else if (type == rest::RequestType::PUT) {
    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);

    if (!parseSuccess) {
      // error message generated in parseVPackBody
      return RestStatus::DONE;
    }
    
    VPackBuilder builder;
    Result result =
        methods::Ttl::setProperties(_vocbase.server().getFeature<TtlFeature>(), body, builder);

    if (result.fail()) {
      generateError(result);
    } else {
      generateOk(rest::ResponseCode::OK, builder.slice());
    }

    return RestStatus::DONE;
  }
 
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

RestStatus RestTtlHandler::handleStatistics() {
  // extract the request type - only GET is allowed here
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    VPackBuilder builder;
    Result result =
        methods::Ttl::getStatistics(_vocbase.server().getFeature<TtlFeature>(), builder);

    if (result.fail()) {
      generateError(result);
    } else {
      generateOk(rest::ResponseCode::OK, builder.slice());
    }

    return RestStatus::DONE;
  }
 
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}
