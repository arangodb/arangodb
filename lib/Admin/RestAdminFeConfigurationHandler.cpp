////////////////////////////////////////////////////////////////////////////////
/// @brief front-end configuration request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminFeConfigurationHandler.h"

#include "Basics/FileUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Logger/Logger.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief if no filename is known, save configuration in memory
////////////////////////////////////////////////////////////////////////////////

static string transientResult;

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

RestAdminFeConfigurationHandler::RestAdminFeConfigurationHandler (HttpRequest* request, char const* filename)
  : RestAdminBaseHandler(request),
    _filename(filename) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestAdminFeConfigurationHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestAdminFeConfigurationHandler::execute () {
  switch (_request->requestType()) {
    case HttpRequest::HTTP_REQUEST_GET:
      return executeRead();

    case HttpRequest::HTTP_REQUEST_POST:
      return executeWrite();

    default:
      generateError(HttpResponse::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                    "expecting GET or POST");
      return HANDLER_DONE;
  }

  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a configuration
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestAdminFeConfigurationHandler::executeRead () {
  string result;

  if (_filename.empty()) {
    result = transientResult;
  }
  else {
    try {
      result = FileUtils::slurp(_filename);
    }
    catch (...) {
      LOGGER_INFO("cannot read configuration '" << _filename << "'");
    }
  }

  if (result.empty()) {
    result = "{}";
  }

  _response = createResponse(HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText(result);

  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a configuration
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestAdminFeConfigurationHandler::executeWrite () {
  if (_filename.empty()) {
    transientResult = _request->body();
  }
  else {
    try {
      FileUtils::spit(_filename, _request->body());
    }
    catch (...) {
      generateError(HttpResponse::SERVER_ERROR,
                    TRI_ERROR_SYS_ERROR,
                    "cannot write configuration");
      return HANDLER_DONE;
    }
  }

  _response = createResponse(HttpResponse::OK);
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
