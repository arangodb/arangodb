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
/// @author Aalekh Nigam
////////////////////////////////////////////////////////////////////////////////

#include "GeneralsServer.h"

#include "HttpServer/HttpsCommTask.h"

using namespace arangodb::rest;



////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general(http/velocy) server
////////////////////////////////////////////////////////////////////////////////

GeneralsServer::GeneralsServer(Scheduler* scheduler, Dispatcher* dispatcher,
                         GeneralHandlerFactory* handlerFactory,
                         AsyncJobManager* jobManager, double keepAliveTimeout,
                         SSL_CTX* ctx)
    : GeneralServer(scheduler, dispatcher, handlerFactory, jobManager,
                 keepAliveTimeout),
      _ctx(ctx),
      _verificationMode(SSL_VERIFY_NONE),
      _verificationCallback(0) {}


GeneralsServer::~GeneralsServer() {
  // don't free context here but in dtor of ApplicationEndpointServer
  // SSL_CTX_free(ctx);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification mode
////////////////////////////////////////////////////////////////////////////////

void GeneralsServer::setVerificationMode(int mode) { _verificationMode = mode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification callback
////////////////////////////////////////////////////////////////////////////////

void GeneralsServer::setVerificationCallback(int (*func)(int, X509_STORE_CTX*)) {
  _verificationCallback = func;
}


