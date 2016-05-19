////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "HttpsServer.h"

#include "HttpServer/HttpsCommTask.h"

using namespace arangodb;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http server
////////////////////////////////////////////////////////////////////////////////

HttpsServer::HttpsServer(Scheduler* scheduler, Dispatcher* dispatcher,
                         HttpHandlerFactory* handlerFactory,
                         AsyncJobManager* jobManager, double keepAliveTimeout,
                         std::vector<std::string> const& accessControlAllowOrigins,
                         SSL_CTX* ctx)
    : HttpServer(scheduler, dispatcher, handlerFactory, jobManager,
                 keepAliveTimeout, accessControlAllowOrigins),
      _ctx(ctx),
      _verificationMode(SSL_VERIFY_NONE),
      _verificationCallback(0) {}

HttpsServer::~HttpsServer() {
  // don't free context here but in dtor of ApplicationEndpointServer
  // SSL_CTX_free(ctx);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification mode
////////////////////////////////////////////////////////////////////////////////

void HttpsServer::setVerificationMode(int mode) { _verificationMode = mode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification callback
////////////////////////////////////////////////////////////////////////////////

void HttpsServer::setVerificationCallback(int (*func)(int, X509_STORE_CTX*)) {
  _verificationCallback = func;
}

HttpCommTask* HttpsServer::createCommTask(TRI_socket_t s,
                                          ConnectionInfo&& info) {
  return new HttpsCommTask(this, s, std::move(info), _keepAliveTimeout, _ctx,
                           _verificationMode, _verificationCallback);
}
