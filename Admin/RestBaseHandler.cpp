////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for error handling and json in-/output
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBaseHandler.h"

#include <boost/scoped_ptr.hpp>

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "ResultGenerator/OutputGenerator.h"
#include "Variant/VariantArray.h"
#include "Variant/VariantBoolean.h"
#include "Variant/VariantInt32.h"
#include "Variant/VariantObject.h"
#include "Variant/VariantString.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an output format
////////////////////////////////////////////////////////////////////////////////

string SelectResultGenerator (HttpRequest* request) {
  string format = request->header("accept");

  if (format.empty()) {
    return "application/json";
  }

  return format;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestBaseHandler::RestBaseHandler (HttpRequest* request)
  : HttpHandler(request) {
  bool found;

  if (request->requestType() == HttpRequest::HTTP_REQUEST_GET) {
    string const& method = StringUtils::tolower(request->value("__METHOD__", found));

    if (found) {
      if (method == "put") {
        LOGGER_TRACE << "forcing method 'put'";
        request->setRequestType(HttpRequest::HTTP_REQUEST_PUT);
      }
      else if (method == "post") {
        LOGGER_TRACE << "forcing method 'post'";
        request->setRequestType(HttpRequest::HTTP_REQUEST_POST);
      }
      else if (method == "delete") {
        LOGGER_TRACE << "forcing method 'delete'";
        request->setRequestType(HttpRequest::HTTP_REQUEST_DELETE);
      }
    }

    string const& body = request->value("__BODY__", found);

    if (found) {
      LOGGER_TRACE << "forcing body";
      request->setBody(body);
    }
  }

  string const& format = request->value("__OUTPUT__", found);

  if (found) {
    LOGGER_TRACE << "forcing output format '" << format << "'";
    request->setHeader("accept", format);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               HttpHandler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::handleError (TriagensError const& error) {
  if (response != 0) {
    delete response;
  }

  generateError(HttpResponse::SERVER_ERROR, 
                TRI_ERROR_INTERNAL,
                DIAGNOSTIC_INFORMATION(error));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a result
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateResult (VariantObject* result) {
  response = new HttpResponse(HttpResponse::OK);

  string contentType;
  bool ok = OutputGenerator::output(SelectResultGenerator(request), response->body(), result, contentType);

  if (ok) {
    response->setContentType(contentType);
  }
  else {
    delete response;
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }

  delete result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestBaseHandler::generateError (HttpResponse::HttpResponseCode code, int errorCode, string const& message) {
  response = new HttpResponse(code);

  VariantArray* result = new VariantArray();
  result->add("error", new VariantBoolean(true));
  result->add("code", new VariantInt32((int32_t) code));
  result->add("errorNum", new VariantInt32((int32_t) errorCode));
  result->add("errorMessage", new VariantString(message));

  string contentType;
  bool ok = OutputGenerator::output(SelectResultGenerator(request), response->body(), result, contentType);

  if (ok) {
    response->setContentType(contentType);
  }
  else {
    response->body().appendText("{ \"error\" : true, \"errorMessage\" : \"" );
    response->body().appendText(StringUtils::escapeUnicode(message));
    response->body().appendText("\", \"code\" : ");
    response->body().appendInteger(code);
    response->body().appendText("\", \"errorNum\" : ");
    response->body().appendInteger(errorCode);
    response->body().appendText("}");

    response->setContentType("application/json");
  }

  delete result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a request body in json given a description
////////////////////////////////////////////////////////////////////////////////

bool RestBaseHandler::parseBody (InputParser::ObjectDescription& desc) {
  boost::scoped_ptr<VariantArray> json(InputParser::jsonArray(request));
  bool ok = desc.parse(json.get());

  if (! ok) {
    generateError(HttpResponse::BAD, 
                  TRI_REST_ERROR_CORRUPTED_JSON,
                  desc.lastError());
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
