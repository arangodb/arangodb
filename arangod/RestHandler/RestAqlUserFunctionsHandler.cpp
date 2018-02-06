////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include "RestAqlUserFunctionsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Rest/HttpRequest.h"

#include "VocBase/Methods/AqlUserFunctions.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAqlUserFunctionsHandler::RestAqlUserFunctionsHandler(GeneralRequest* request, GeneralResponse* response)
  : RestVocbaseBaseHandler(request, response) {}

RestStatus RestAqlUserFunctionsHandler::execute() {
  bool parseSuccess = true;

  auto const type = _request->requestType();

  if (type == rest::RequestType::PUT) {
    bool parsingSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(parsingSuccess);

    if (!parsingSuccess) {
      return RestStatus::DONE;
    }

    VPackSlice body = parsedBody.get()->slice();

    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    std::string const& prefix = _request->requestPath();
 
    bool replacedExisting = false;
    registerUserFunction(_vocbase, body, replacedExisting);

    return RestStatus::DONE;

  }
  else if (type == rest::RequestType::DELETE_REQ) {
  }
  else if (type == rest::RequestType::GET) {
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}





/*
exports.unregister = unregisterFunction; => delete exactMatch=true (default)
  exports.unregisterGroup = unregisterFunctionsGroup; delete exactMatch=false 

exports.register = registerFunction; => put TODO: doppelt gemoppelt function name in body checken
exports.toArray = toArrayFunctions; => get
*/
