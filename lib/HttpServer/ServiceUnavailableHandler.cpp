////////////////////////////////////////////////////////////////////////////////
/// @brief redirect handler
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
/// @author Dr. Frank Celler
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ServiceUnavailableHandler.h"

#include <fstream>

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
    // constructors and destructores
// -----------------------------------------------------------------------------

    ServiceUnavailableHandler::ServiceUnavailableHandler ()
      : HttpHandler(0) {
      _response = createResponse(HttpResponse::SERVICE_UNAVAILABLE);
    }

// -----------------------------------------------------------------------------
    // Handler methods
// -----------------------------------------------------------------------------

    HttpHandler::status_e ServiceUnavailableHandler::execute () {
      return HANDLER_DONE;
    }



    void ServiceUnavailableHandler::handleError (TriagensError const&) {
    }
  }
}
