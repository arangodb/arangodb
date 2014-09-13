////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for error handling and json in-/output
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBaseHandler.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestBaseHandler::RestBaseHandler (HttpRequest* request)
  : HttpHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                               HttpHandler methods
// -----------------------------------------------------------------------------

void RestBaseHandler::handleError (TriagensError const& error) {
  generateError(HttpResponse::SERVER_ERROR,
                TRI_ERROR_INTERNAL,
                DIAGNOSTIC_INFORMATION(error));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from JSON
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult (TRI_json_t const* json) {
  generateResult(HttpResponse::OK, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from JSON
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult (HttpResponse::HttpResponseCode code,
                                      TRI_json_t const* json) {
  _response = createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  int res = TRI_StringifyJson(_response->body().stringBuffer(), json);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a cancel message
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateCanceled () {
  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  char* msg = TRI_DuplicateString("request canceled");

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json,
                       "error", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json,
                       "code", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (int32_t) HttpResponse::REQUEST_TIMEOUT));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json,
                       "errorNum", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (int32_t) TRI_ERROR_REQUEST_CANCELED));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json,
                       "errorMessage", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, msg));

  generateResult(HttpResponse::REQUEST_TIMEOUT, json);

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError (HttpResponse::HttpResponseCode code,
                                     int errorCode) {
  char const* message = TRI_errno_string(errorCode);

  if (message) {
    generateError(code, errorCode, string(message));
  }
  else {
    generateError(code, errorCode, string("unknown error"));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError (HttpResponse::HttpResponseCode code, int errorCode, string const& message) {
  _response = createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  _response->body().appendText("{\"error\":true,\"errorMessage\":\"");
  _response->body().appendText(StringUtils::escapeUnicode(message));
  _response->body().appendText("\",\"code\":");
  _response->body().appendInteger(code);
  _response->body().appendText(",\"errorNum\":");
  _response->body().appendInteger(errorCode);
  _response->body().appendText("}");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
