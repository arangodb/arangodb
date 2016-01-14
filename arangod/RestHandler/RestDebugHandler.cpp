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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "RestDebugHandler.h"

#include "Rest/AnyServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace arangodb;

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern AnyServer* ArangoInstance;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestDebugHandler::RestDebugHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request) {}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestDebugHandler::isDirect() const { return false; }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestDebugHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();
  size_t const len = _request->suffix().size();
  if (len != 2 || !(_request->suffix()[0] == "failat")) {
    generateNotImplemented("ILLEGAL /_admin/debug/failat");
    return status_t(HANDLER_DONE);
  }
  std::vector<std::string> const& suffix = _request->suffix();

  // execute one of the CRUD methods
  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE:
      TRI_RemoveFailurePointDebugging(suffix[1].c_str());
      break;
    case HttpRequest::HTTP_REQUEST_PUT:
      TRI_AddFailurePointDebugging(suffix[1].c_str());
      break;
    default:
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      return status_t(HANDLER_DONE);
  }
  try {
    VPackBuilder result;
    result.add(VPackValue(true));
    VPackSlice s = result.slice();
    generateResult(s);
  } catch (...) {
    // Ignore this error
  }
  return status_t(HANDLER_DONE);
}
