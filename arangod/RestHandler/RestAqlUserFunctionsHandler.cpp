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

  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    bool parsingSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(parsingSuccess);

    if (!parsingSuccess) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    VPackSlice body = parsedBody.get()->slice();

    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    bool replacedExisting = false;
    auto res = registerUserFunction(_vocbase, body, replacedExisting);
    if (res.ok()) {
      VPackBuffer<uint8_t> resultBuffer;
      VPackBuilder result(resultBuffer);

      auto response = _response.get();
      resetResponse(rest::ResponseCode::OK);

      response->setContentType(rest::ContentType::JSON);
      result.openObject();
      result.add("replacedExisting",
                 VPackValue(replacedExisting));
      result.add("error", VPackValue(false));
      result.add("code",
                 VPackValue(static_cast<int>(_response->responseCode())));
      result.close();
      generateResult(rest::ResponseCode::OK, std::move(resultBuffer));

    }
    else {
      generateError(res);
    }
    return RestStatus::DONE;
  }
  else if (type == rest::RequestType::DELETE_REQ) {
    std::vector<std::string> const& suffixes = _request->decodedSuffixes();
    if ((suffixes.size() != 1) || suffixes[1].empty() ) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "superfluous suffix, expecting _api/aqlfunction/<functionname or prefix>");
      return RestStatus::DONE;
    }

    bool deleteGroup = extractBooleanParameter(StaticStrings::deleteGroup, false);

    if (deleteGroup) {
      int deletedCount = 0;
      Result res = unregisterUserFunctionsGroup(_vocbase, suffixes[0], deletedCount);

      if (res.ok()) {
        VPackBuffer<uint8_t> resultBuffer;
        VPackBuilder result(resultBuffer);

        auto response = _response.get();
        resetResponse(rest::ResponseCode::OK);

        response->setContentType(rest::ContentType::JSON);
        result.add("deletedCount", VPackValue(static_cast<int>(deletedCount)));
      }
      else {
        generateError(res);

      }
      
    }
    else {
     auto res = unregisterUserFunction(_vocbase,suffixes[0]);
     
     if (res.ok()) {
       resetResponse(rest::ResponseCode::OK);
     }
     else {
       generateError(res);
       return RestStatus::DONE;
     }
    }
  }
  else if (type == rest::RequestType::GET) {
    std::string functionNamespace;
    std::vector<std::string> const& suffixes = _request->decodedSuffixes();
    if ((suffixes.size() != 1) || suffixes[1].empty() ) {
      extractStringParameter(StaticStrings::functionNamespace, functionNamespace);
      if (functionNamespace.empty()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                      "superfluous suffix, expecting _api/aqlfunction/[<functionname or prefix>|?" +
                      StaticStrings::functionNamespace + "=<functionname or prefix>]");
        return RestStatus::DONE;
      }
    }
    else {
      functionNamespace = suffixes[0];
    }

    std::shared_ptr<VPackBuilder> arrayOfFunctions;

    arrayOfFunctions.reset(new VPackBuilder());

    auto res = toArrayUserFunctions(_vocbase, functionNamespace, *arrayOfFunctions.get());


    VPackBuffer<uint8_t> resultBuffer;
    VPackBuilder result(resultBuffer);
    {
      VPackObjectBuilder guard(&result);
      resetResponse(rest::ResponseCode::OK);

      auto response = _response.get();

      response->setContentType(rest::ContentType::JSON);
      result.openObject();
      result.add(VPackValue("documents"));

      result.addExternal(arrayOfFunctions.get()->slice().begin());
      result.add("error", VPackValue(false));
      result.add("code",
                 VPackValue(static_cast<int>(_response->responseCode())));
      result.close();
    }

    generateResult(rest::ResponseCode::OK, std::move(resultBuffer));

    return RestStatus::DONE;
    
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
