////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for handlers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandler.h"

#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpsServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "GeneralServer/GeneralServerJob.h"

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler::HttpHandler (HttpRequest* request)
  : _request(request),
    _response(0),
    _server(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler::~HttpHandler () {
  if (_request != 0) {
    delete _request;
  }

  if (_response != 0) {
    delete _response;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response
////////////////////////////////////////////////////////////////////////////////

HttpResponse* HttpHandler::getResponse () const {
  return _response;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job* HttpHandler::createJob (AsyncJobServer* server) {
  HttpServer* httpServer = dynamic_cast<HttpServer*>(server);

  // TODO: ugly temporary hack, must be fixed
  if (httpServer != 0) {
    return new GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>(httpServer, this);
  }

  HttpsServer* httpsServer = dynamic_cast<HttpsServer*>(server);
  if (httpsServer != 0) {
    return new GeneralServerJob<HttpsServer, HttpHandlerFactory::GeneralHandler>(httpsServer, this);
  }
  // end of hack

  LOGGER_WARNING("cannot convert AsyncJobServer into a HttpServer");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure the handler has only one response, otherwise we'd have a leak
////////////////////////////////////////////////////////////////////////////////
        
void HttpHandler::removePreviousResponse () {
  if (_response != 0) {
    delete _response;
    _response = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new HTTP response
////////////////////////////////////////////////////////////////////////////////

HttpResponse* HttpHandler::createResponse (HttpResponse::HttpResponseCode code) {
  // avoid having multiple responses. this would be a memleak
  removePreviousResponse();

  // otherwise, we return a "standard" (standalone) Http response
  return new HttpResponse(code);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
