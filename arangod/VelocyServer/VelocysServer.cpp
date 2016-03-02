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

#include "VelocysServer.h"

#include "VelocyServer/VelocysCommTask.h"

using namespace arangodb::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new velocy server
////////////////////////////////////////////////////////////////////////////////

VelocysServer::VelocysServer(Scheduler* scheduler, Dispatcher* dispatcher,
                         GeneralHandlerFactory* handlerFactory,
                         AsyncJobManager* jobManager, double keepAliveTimeout,
                         SSL_CTX* ctx)
    : GeneralsServer(scheduler, dispatcher, handlerFactory, jobManager,
                 keepAliveTimeout){}


VelocysServer::~VelocysServer() {
  // don't free context here but in dtor of ApplicationEndpointServer
  // SSL_CTX_free(ctx);
}

ArangoTask* VelocysServer::createCommTask(TRI_socket_t s,
                                          const ConnectionInfo& info) {
  return new VelocysCommTask(this, s, info, _keepAliveTimeout, _ctx,
                           _verificationMode, _verificationCallback);
}
