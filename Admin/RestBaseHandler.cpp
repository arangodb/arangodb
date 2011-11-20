////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for error handling and json in-/output
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
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBaseHandler.h"

#include <boost/scoped_ptr.hpp>

#include <Basics/Logger.h>
#include <Basics/VariantArray.h>
#include <Basics/VariantObject.h>
#include <Basics/StringUtils.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>
#include <Rest/OutputGenerator.h>

using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace admin {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    RestBaseHandler::RestBaseHandler (HttpRequest* request)
      : HttpHandler(request) {

      if (request->requestType() == HttpRequest::HTTP_REQUEST_GET) {
        bool found;
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
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void RestBaseHandler::handleError (TriagensError const& error) {
      if (response != 0) {
        delete response;
      }

      generateError(HttpResponse::SERVER_ERROR, DIAGNOSTIC_INFORMATION(error));
    }



    void RestBaseHandler::generateResult (VariantObject* result) {
      response = new HttpResponse(HttpResponse::OK);

      string contentType;
      bool ok = OutputGenerator::output("json", response->body(), result, contentType);

      if (ok) {
        response->setContentType(contentType);
      }
      else {
        delete response;
        generateError(HttpResponse::SERVER_ERROR, "cannot generate output");
      }

      delete result;
    }



    void RestBaseHandler::generateError (HttpResponse::HttpResponseCode code, string const& message) {
      response = new HttpResponse(code);

      response->body().appendText("{ \"error\" : true, \"message\" : \"" );
      response->body().appendText(StringUtils::escapeUnicode(message));
      response->body().appendText("\", \"code\" : ");
      response->body().appendInteger(code);
      response->body().appendText("}");

      response->setContentType("application/json");
    }



    bool RestBaseHandler::parseBody (InputParser::ObjectDescription& desc) {
      boost::scoped_ptr<VariantArray> json(InputParser::jsonArray(request));
      bool ok = desc.parse(json.get());

      if (! ok) {
        generateError(HttpResponse::BAD, desc.lastError());
      }

      return ok;
    }
  }
}
