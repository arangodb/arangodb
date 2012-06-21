////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for handlers
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandler.h"

#include <Logger/Logger.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructs and destructors
    // -----------------------------------------------------------------------------



    HttpHandler::HttpHandler (HttpRequest* request)
      : request(request), response(0), _task(0), _job(0) {
    }



    HttpHandler::~HttpHandler () {
      if (request != 0) {
        delete request;
      }

      if (response != 0) {
        delete response;
      }

      if (_task != 0) {
        _task->setHandler(0);
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    HttpResponse* HttpHandler::getResponse () {
      return response;
    }

    bool HttpHandler::handleAsync () {
      if (_job == 0) {
        LOGGER_WARNING << "no job is known";
      }
      else {
        HttpResponse* response = getResponse();

        try {
          if (response == 0) {
            basics::InternalError err("no response received from handler");

            handleError(err);
            response = getResponse();
          }

          if (response != 0) {
            _task->handleResponse(response);
          }
        }
        catch (...) {
          LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__;
        }
 
        // this might delete the handler (i.e. ourselves!)
        return _job->beginShutdown();
      }

      return true;
    }
  }
}
