////////////////////////////////////////////////////////////////////////////////
/// @brief front-end configuration request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminFeConfigurationHandler.h"

#include <Basics/FileUtils.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// private variables
// -----------------------------------------------------------------------------

namespace {
  string transientResult;
}

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // Handler methods
    // -----------------------------------------------------------------------------

    HttpHandler::status_e RestAdminFeConfigurationHandler::execute () {
      switch (request->requestType()) {
        case HttpRequest::HTTP_REQUEST_GET:
          return executeRead();

        case HttpRequest::HTTP_REQUEST_POST:
          return executeWrite();

        default:
          generateError(HttpResponse::METHOD_NOT_ALLOWED, "expecting GET or POST");
          return HANDLER_DONE;
      }

      return HANDLER_DONE;
    }

    // -----------------------------------------------------------------------------
    // Handler methods
    // -----------------------------------------------------------------------------

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
          LOGGER_INFO << "cannot read configuration '" << _filename << "'";
        }
      }

      if (result.empty()) {
        result = "{}";
      }

      response = new HttpResponse(HttpResponse::OK);
      response->setContentType("application/json");
      response->body().appendText(result);

      return HANDLER_DONE;
    }



    HttpHandler::status_e RestAdminFeConfigurationHandler::executeWrite () {
      if (_filename.empty()) {
        transientResult = request->body().c_str();
      }
      else {
        try {
          FileUtils::spit(_filename, request->body());
        }
        catch (...) {
          generateError(HttpResponse::SERVER_ERROR, "cannot write configuration");
          return HANDLER_DONE;
        }
      }

      response = new HttpResponse(HttpResponse::OK);
      return HANDLER_DONE;
    }
  }
}
