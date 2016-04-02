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

#include "EndpointFeature.h"

#include "Dispatcher/DispatcherFeature.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpsServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

EndpointFeature::EndpointFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Endpoint"),
      _reuseAddress(true),
      _backlogSize(64),
      _keepAliveTimeout(300.0) {
  setOptional(true);
  requiresElevatedPrivileges(true);
  startsAfter("Ssl");
  startsAfter("Server");

  // if our default value is too high, we'll use half of the max value provided
  // by the system
  if (_backlogSize > SOMAXCONN) {
    _backlogSize = SOMAXCONN / 2;
  }
}

void EndpointFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(
      Section("server", "Server features", "server options", false, false));

  options->addOption("--server.endpoint",
                     "endpoint for client requests (e.g. "
                     "'http+tcp://127.0.0.1:8529', or "
                     "'vpp+ssl://192.168.1.1:8529')",
                     new VectorParameter<StringParameter>(&_endpoints));

  options->addSection(
      Section("tcp", "TCP features", "tcp options", false, false));

  options->addHiddenOption("--tcp.reuse-address", "try to reuse TCP port(s)",
                           new BooleanParameter(&_reuseAddress));

  options->addHiddenOption("--tcp.backlog-size", "listen backlog size",
                           new UInt64Parameter(&_backlogSize));

  options->addSection(
      Section("http", "HttpServer features", "http options", false, false));

  options->addOption("--http.keep-alive-timeout",
                     "keep-alive timeout in seconds",
                     new DoubleParameter(&_keepAliveTimeout));
}

void EndpointFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  if (_backlogSize > SOMAXCONN) {
    LOG(WARN) << "value for --tcp.backlog-size exceeds default system "
                 "header SOMAXCONN value "
              << SOMAXCONN << ". trying to use " << SOMAXCONN << " anyway";
  }
}

void EndpointFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

  buildEndpointLists();

  if (_endpointList.empty()) {
    LOG(FATAL) << "no endpoints have been specified, giving up, please use the "
                  "'--server.endpoint' option";
    FATAL_ERROR_EXIT();
  }
}

void EndpointFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  _endpointList.dump();
  buildServers();

  for (auto& server : _servers) {
    server->startListening();
  }
}

void EndpointFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

  for (auto& server : _servers) {
    server->stopListening();
  }

  for (auto& server : _servers) {
    server->stop();
  }

  for (auto& server : _servers) {
    delete server;
  }
}

void EndpointFeature::buildEndpointLists() {
  for (std::vector<std::string>::const_iterator i = _endpoints.begin();
       i != _endpoints.end(); ++i) {
    bool ok = _endpointList.add((*i), std::vector<std::string>(), _backlogSize,
                                _reuseAddress);

    if (!ok) {
      LOG(FATAL) << "invalid endpoint '" << (*i) << "'";
      FATAL_ERROR_EXIT();
    }
  }
}

void EndpointFeature::buildServers() {
  ServerFeature* server = dynamic_cast<ServerFeature*>(
      application_features::ApplicationServer::lookupFeature("Server"));

  TRI_ASSERT(server != nullptr);

  HttpHandlerFactory* handlerFactory = server->httpHandlerFactory();
  AsyncJobManager* jobManager = server->jobManager();

#warning TODO filter list
  HttpServer* httpServer;

  // unencrypted HTTP endpoints
  httpServer =
      new HttpServer(SchedulerFeature::SCHEDULER, DispatcherFeature::DISPATCHER,
                     handlerFactory, jobManager, _keepAliveTimeout);

  httpServer->setEndpointList(&_endpointList);
  _servers.push_back(httpServer);

  // ssl endpoints
  if (_endpointList.has(Endpoint::ENCRYPTION_SSL)) {
    SslFeature* ssl = dynamic_cast<SslFeature*>(
        application_features::ApplicationServer::lookupFeature("Ssl"));

    // check the ssl context
    if (ssl == nullptr || ssl->sslContext() == nullptr) {
      LOG(FATAL) << "no ssl context is known, cannot create https server, "
                    "please use the '--ssl.keyfile' option";
      FATAL_ERROR_EXIT();
    }

    SSL_CTX* sslContext = ssl->sslContext();

    // https
    httpServer = new HttpsServer(SchedulerFeature::SCHEDULER,
                                 DispatcherFeature::DISPATCHER, handlerFactory,
                                 jobManager, _keepAliveTimeout, sslContext);

    httpServer->setEndpointList(&_endpointList);
    _servers.push_back(httpServer);
  }
}
