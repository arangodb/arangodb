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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include "RestAqlUserFunctionsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "VocBase/Methods/AqlUserFunctions.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAqlUserFunctionsHandler::RestAqlUserFunctionsHandler(application_features::ApplicationServer& server,
                                                         GeneralRequest* request,
                                                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestAqlUserFunctionsHandler::execute() {
  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    // JSF_post_api_aqlfunction.md
    // POST /_api/aqlfunction
    bool parsingSuccess = false;
    VPackSlice body = this->parseVPackBody(parsingSuccess);
    if (!parsingSuccess) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    // call internal function that does the work
    bool replacedExisting = false;
    auto res = registerUserFunction(_vocbase, body, replacedExisting);

    if (res.ok()) {
      auto code = replacedExisting ? rest::ResponseCode::OK : rest::ResponseCode::CREATED;
      VPackBuilder tmp;
      tmp.add(VPackValue(VPackValueType::Object));
      tmp.add(StaticStrings::Error, VPackValue(false));
      tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
      tmp.add("isNewlyCreated", VPackValue(!replacedExisting));
      tmp.close();

      generateResult(code, tmp.slice());
    } else {
      generateError(res);
    }

    return RestStatus::DONE;
  } else if (type == rest::RequestType::DELETE_REQ) {
    // JSF_delete_api_aqlfunction.md
    // DELETE /_api/aqlfunction/{name}
    std::vector<std::string> const& suffixes = _request->decodedSuffixes();
    if ((suffixes.size() != 1) || suffixes[0].empty()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "superfluous suffix, expecting "
                    "_api/aqlfunction/<functionname or prefix>");
      return RestStatus::DONE;
    }

    Result res;
    int deletedCount = 0;
    bool deleteGroup = _request->parsedValue(StaticStrings::Group, false);

    if (deleteGroup) {
      res = unregisterUserFunctionsGroup(_vocbase, suffixes[0], deletedCount);
    } else {  // delete single
      res = unregisterUserFunction(_vocbase, suffixes[0]);
      ++deletedCount;
    }

    if (res.ok()) {
      VPackBuilder result;
      result.openObject();
      result.add("deletedCount", VPackValue(static_cast<int>(deletedCount)));
      result.close();
      generateOk(rest::ResponseCode::OK, result);
    } else {
      generateError(res);
    }

    return RestStatus::DONE;
    // DELETE
  } else if (type == rest::RequestType::GET) {
    // JSF_get_api_aqlfunction.md
    // GET /_api/aqlfunction - figure out parameters - function namespace
    std::string functionNamespace;
    std::vector<std::string> const& suffixes = _request->decodedSuffixes();

    if (!suffixes.empty()) {
      if ((suffixes.size() != 1) || suffixes[0].empty()) {
        extractStringParameter(StaticStrings::Prefix, functionNamespace);
        if (functionNamespace.empty()) {
          generateError(
              rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
              "superfluous suffix, expecting _api/aqlfunction/[<functionname "
              "or prefix>|?" +
                  StaticStrings::Prefix + "=<functionname or prefix>]");
          return RestStatus::DONE;
        }
      } else {
        functionNamespace = suffixes[0];
      }
    } else {
      extractStringParameter(StaticStrings::Prefix, functionNamespace);
      if (functionNamespace.empty()) {
        // compatibility mode
        extractStringParameter(StaticStrings::Namespace, functionNamespace);
      }
    }

    // internal get
    VPackBuilder arrayOfFunctions;
    auto res = toArrayUserFunctions(_vocbase, functionNamespace, arrayOfFunctions);

    // error handling
    if (res.ok()) {
      generateOk(rest::ResponseCode::OK, arrayOfFunctions.slice());
    } else {
      generateError(res);
    }
    return RestStatus::DONE;
  }  // GET

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}
