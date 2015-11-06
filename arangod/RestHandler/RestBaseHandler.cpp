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
#include "Basics/StringBufferAdapter.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"


using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

typedef arangodb::velocypack::Dumper<StringBufferAdapter, false> StringBufferDumper;

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
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::handleError (Exception const& ex) {
  generateError(HttpResponse::responseCode(ex.code()),
                ex.code(),
                ex.what());
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
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult (VPackSlice& slice) {
  generateResult(HttpResponse::OK, slice);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result from VelocyPack
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult (HttpResponse::HttpResponseCode code,
                                      VPackSlice& slice) {
  _response = createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  StringBufferAdapter buffer(_response->body().stringBuffer());

  StringBufferDumper dumper(buffer);
  try {
    dumper.dump(slice);
  }
  catch (...) {
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief generates a cancel message
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateCanceled () {
  VPackBuilder builder;
  try {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("error", VPackValue(true));
    builder.add("code", VPackValue((int32_t) HttpResponse::REQUEST_TIMEOUT));
    builder.add("errorNum", VPackValue((int32_t) TRI_ERROR_REQUEST_CANCELED));
    builder.add("errorMessage", VPackValue("request canceled"));
    builder.close();

    VPackSlice slice(builder.start());
    generateResult(HttpResponse::REQUEST_TIMEOUT, slice);
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError (HttpResponse::HttpResponseCode code,
                                     int errorCode) {
  char const* message = TRI_errno_string(errorCode);

  if (message) {
    generateError(code, errorCode, std::string(message));
  }
  else {
    generateError(code, errorCode, std::string("unknown error"));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError (HttpResponse::HttpResponseCode code, int errorCode, std::string const& message) {

  _response = createResponse(code);
  _response->setContentType("application/json; charset=utf-8");

  VPackBuilder builder;
  try {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("error", VPackValue(true));
    if (message.empty()) {
      // prevent empty error messages
      builder.add("errorMessage", VPackValue(TRI_errno_string(errorCode)));
    }
    else {
      builder.add("errorMessage", VPackValue(StringUtils::escapeUnicode(message)));
    }
    builder.add("code", VPackValue(code));
    builder.add("errorNum", VPackValue(errorCode));
    builder.close();
    VPackSlice slice(builder.start());
    StringBufferAdapter buffer(_response->body().stringBuffer());
    StringBufferDumper dumper(buffer);
    dumper.dump(slice);
  }
  catch (...) {
    // Building the error response failed
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
