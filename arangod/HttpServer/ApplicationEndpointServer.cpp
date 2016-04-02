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

#include "ApplicationEndpointServer.h"

#include "Basics/FileUtils.h"
#include "Basics/RandomGenerator.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/ssl-helper.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpsServer.h"
#include "Logger/Logger.h"
#include "Rest/Version.h"
#include "Scheduler/ApplicationScheduler.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

ApplicationEndpointServer::ApplicationEndpointServer(
    ApplicationServer* applicationServer,
    ApplicationScheduler* applicationScheduler,
    ApplicationDispatcher* applicationDispatcher, AsyncJobManager* jobManager,
    std::string const& authenticationRealm,
    HttpHandlerFactory::context_fptr setContext, void* contextData)
    : ApplicationFeature("EndpointServer"),
      _applicationServer(applicationServer),
      _applicationScheduler(applicationScheduler),
      _applicationDispatcher(applicationDispatcher),
      _jobManager(jobManager),
      _authenticationRealm(authenticationRealm),
      _setContext(setContext),
      _contextData(contextData),
      _handlerFactory(nullptr),
      _servers(),
      _basePath(),
      _endpointList(),
      _httpPort(),
      _endpoints(),
      _reuseAddress(true),
      _keepAliveTimeout(300.0),
      _defaultApiCompatibility(0),
      _allowMethodOverride(false),
      _backlogSize(64) {
}

ApplicationEndpointServer::~ApplicationEndpointServer() {
  // ..........................................................................
  // Where ever possible we should EXPLICITLY write down the type used in
  // a templated class/method etc. This makes it a lot easier to debug the
  // code. Granted however, that explicitly writing down the type for an
  // overloaded class operator is a little unwieldy.
  // ..........................................................................

  delete _handlerFactory;

}
