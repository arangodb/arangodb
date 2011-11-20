////////////////////////////////////////////////////////////////////////////////
/// @brief redirect handler
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
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RedirectHandler.h"

#include <fstream>

#include <Basics/FileUtils.h>
#include <Basics/Logger.h>
#include <Basics/StringBuffer.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructores
    // -----------------------------------------------------------------------------

    RedirectHandler::RedirectHandler (HttpRequest* request, string const& redirect)
      : HttpHandler(request),
        _redirect(redirect) {
    }

    // -----------------------------------------------------------------------------
    // Handler methods
    // -----------------------------------------------------------------------------

    HttpHandler::status_e RedirectHandler::execute () {
      response = new HttpResponse(HttpResponse::MOVED_PERMANENTLY);

      response->setHeader("location", _redirect);
      response->setContentType("text/html");

      response->body().appendText("<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\"");
      response->body().appendText(_redirect);
      response->body().appendText(">");
      response->body().appendText(_redirect);
      response->body().appendText("</a>.</p></body></html>");

      return HANDLER_DONE;
    }



    void RedirectHandler::handleError (TriagensError const&) {
      response = new HttpResponse(HttpResponse::SERVER_ERROR);
    }
  }
}
