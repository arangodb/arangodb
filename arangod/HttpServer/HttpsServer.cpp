////////////////////////////////////////////////////////////////////////////////
/// @brief https server
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpsServer.h"

#include "HttpServer/HttpsCommTask.h"

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 class HttpsServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http server
////////////////////////////////////////////////////////////////////////////////

HttpsServer::HttpsServer (Scheduler* scheduler,
                          Dispatcher* dispatcher,
                          HttpHandlerFactory* handlerFactory,
                          AsyncJobManager* jobManager,
                          double keepAliveTimeout,
                          SSL_CTX* ctx)
  : HttpServer(scheduler, dispatcher, handlerFactory, jobManager, keepAliveTimeout),
    _ctx(ctx),
    _verificationMode(SSL_VERIFY_NONE),
    _verificationCallback(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

HttpsServer::~HttpsServer () {
  // don't free context here but in dtor of ApplicationEndpointServer
  // SSL_CTX_free(ctx);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification mode
////////////////////////////////////////////////////////////////////////////////

void HttpsServer::setVerificationMode (int mode) {
  _verificationMode = mode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification callback
////////////////////////////////////////////////////////////////////////////////

void HttpsServer::setVerificationCallback (int (*func)(int, X509_STORE_CTX *)) {
  _verificationCallback = func;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                HttpServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpCommTask* HttpsServer::createCommTask (TRI_socket_t s, const ConnectionInfo& info) {
  return new HttpsCommTask(
    this, s, info, _keepAliveTimeout, _ctx, _verificationMode, _verificationCallback);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
