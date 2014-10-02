////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for handlers
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
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandler.h"

#include "Basics/logging.h"
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

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response
////////////////////////////////////////////////////////////////////////////////

HttpResponse* HttpHandler::getResponse () const {
  return _response;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the response
////////////////////////////////////////////////////////////////////////////////

HttpResponse* HttpHandler::stealResponse () {
  HttpResponse* tmp = _response;
  _response = 0;
  return tmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the request
////////////////////////////////////////////////////////////////////////////////

HttpRequest* HttpHandler::stealRequest () {
  HttpRequest* tmp = _request;
  _request = 0;
  return tmp;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job* HttpHandler::createJob (AsyncJobServer* server,
                             bool isDetached) {
  HttpServer* httpServer = dynamic_cast<HttpServer*>(server);

  // check if we are an HTTP server at all
  if (httpServer != 0) {
    return new GeneralServerJob<HttpServer, HttpHandlerFactory::GeneralHandler>(httpServer, this, isDetached);
  }

  // check if we are an HTTPs server at all
  HttpsServer* httpsServer = dynamic_cast<HttpsServer*>(server);

  if (httpsServer != 0) {
    return new GeneralServerJob<HttpsServer, HttpHandlerFactory::GeneralHandler>(httpsServer, this, isDetached);
  }

  LOG_WARNING("cannot convert AsyncJobServer into a HttpServer");
  return 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

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

  int32_t apiCompatibility;
  if (this->_request != nullptr) {
    apiCompatibility = this->_request->compatibility();
  }
  else {
    apiCompatibility = HttpRequest::MinCompatibility;
  }

  // otherwise, we return a "standard" (standalone) Http response
  return new HttpResponse(code, apiCompatibility);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
