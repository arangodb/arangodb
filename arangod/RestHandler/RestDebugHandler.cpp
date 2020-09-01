////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "RestDebugHandler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDebugHandler::RestDebugHandler(application_features::ApplicationServer& server,
                                   GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestDebugHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  size_t const len = suffixes.size();

  if (len == 0 || len > 2 || suffixes[0] != "failat") {
    generateNotImplemented("ILLEGAL /_admin/debug/failat");
    return RestStatus::DONE;
  }

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::GET: {
      VPackBuilder result;
      result.add(VPackValue(TRI_CanUseFailurePointsDebugging()));
      generateResult(rest::ResponseCode::OK, result.slice());
      return RestStatus::DONE;
    }
    case rest::RequestType::DELETE_REQ:
      if (len == 1) {
        TRI_ClearFailurePointsDebugging();
      } else {
        TRI_RemoveFailurePointDebugging(suffixes[1].c_str());
      }
      break;
    case rest::RequestType::PUT:
      if (len == 2) {
        TRI_AddFailurePointDebugging(suffixes[1].c_str());
      } else {
        generateNotImplemented("ILLEGAL /_admin/debug/failat");
      }
      break;
    default:
      generateNotImplemented("ILLEGAL /_admin/debug/failat");
      return RestStatus::DONE;
  }
  try {
    VPackBuilder result;
    result.add(VPackValue(true));
    generateResult(rest::ResponseCode::OK, result.slice());
  } catch (...) {
    // Ignore this error
  }
  return RestStatus::DONE;
}
