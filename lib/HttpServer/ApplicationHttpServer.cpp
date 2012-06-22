////////////////////////////////////////////////////////////////////////////////
/// @brief application http server feature
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationHttpServer.h"

#include "Basics/delete_object.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"
#include "Logger/Logger.h"
#include "Scheduler/ApplicationScheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationHttpServer::ApplicationHttpServer (ApplicationScheduler* applicationScheduler,
                                              ApplicationDispatcher* applicationDispatcher)
  : ApplicationFeature("HttpServer"),
    _applicationScheduler(applicationScheduler),
    _applicationDispatcher(applicationDispatcher),
    _showPort(true),
    _requireKeepAlive(false),
    _httpServers(),
    _httpPorts(),
    _httpAddressPorts() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationHttpServer::~ApplicationHttpServer () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shows the port options
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpServer::showPortOptions (bool value) {
  _showPort = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a http address:port
////////////////////////////////////////////////////////////////////////////////

AddressPort ApplicationHttpServer::addPort (string const& name) {
  AddressPort ap;
  
  if (! ap.split(name)) {
    LOGGER_ERROR << "unknown server:port definition '" << name << "'";
  }
  else {
    _httpAddressPorts.push_back(ap);
  }
  
  return ap;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
////////////////////////////////////////////////////////////////////////////////

HttpServer* ApplicationHttpServer::buildServer (HttpHandlerFactory* httpHandlerFactory) {
  return buildHttpServer(0, httpHandlerFactory, _httpAddressPorts);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
////////////////////////////////////////////////////////////////////////////////

HttpServer* ApplicationHttpServer::buildServer (HttpHandlerFactory* httpHandlerFactory,
                                                vector<AddressPort> const& ports) {
  if (ports.empty()) {
    delete httpHandlerFactory;
    return 0;
  }
  else {
    return buildHttpServer(0, httpHandlerFactory, ports);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
////////////////////////////////////////////////////////////////////////////////

HttpServer* ApplicationHttpServer::buildServer (HttpServer* httpServer,
                                                HttpHandlerFactory* httpHandlerFactory,
                                                vector<AddressPort> const& ports) {
  if (ports.empty()) {
    delete httpHandlerFactory;
    return 0;
  }
  else {
    return buildHttpServer(httpServer, httpHandlerFactory, ports);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpServer::setupOptions (map<string, ProgramOptionsDescription>& options) {
  if (_showPort) {
    options[ApplicationServer::OPTIONS_SERVER]
      ("server.port", &_httpPorts, "listen port or address:port")
    ;
  }

  options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
    ("server.require-keep-alive", "close connection, if keep-alive is missing")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpServer::parsePhase2 (ProgramOptions& options) {
  if (options.has("server.require-keep-alive")) {
    _requireKeepAlive= true;
  }

  for (vector<string>::const_iterator i = _httpPorts.begin();  i != _httpPorts.end();  ++i) {
    addPort(*i);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpServer::shutdown () {
  for_each(_httpServers.begin(), _httpServers.end(), DeleteObject());
  _httpServers.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief build an http server
////////////////////////////////////////////////////////////////////////////////

HttpServer* ApplicationHttpServer::buildHttpServer (HttpServer* httpServer,
                                                    HttpHandlerFactory* httpHandlerFactory,
                                                    vector<AddressPort> const& ports) {
  Scheduler* scheduler = _applicationScheduler->scheduler();

  if (scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create http server";
    TRI_ShutdownLogging();
    exit(EXIT_FAILURE);
  }

  // create new server
  if (httpServer == 0) {
    Dispatcher* dispatcher = 0;

    if (_applicationDispatcher != 0) {
      dispatcher = _applicationDispatcher->dispatcher();
    }

    httpServer = new HttpServer(scheduler, dispatcher);
  }

  httpServer->setHandlerFactory(httpHandlerFactory);

  if (_requireKeepAlive) {
    httpServer->setCloseWithoutKeepAlive(true);
  }

  // keep a list of active server
  _httpServers.push_back(httpServer);

  // open http ports
  deque<AddressPort> addresses;
  addresses.insert(addresses.begin(), ports.begin(), ports.end());

  while (! addresses.empty()) {
    AddressPort ap = addresses[0];
    addresses.pop_front();

    string bindAddress = ap._address;
    int port = ap._port;

    bool result;

    if (bindAddress.empty()) {
      LOGGER_TRACE << "trying to open port " << port << " for http requests";

      result = httpServer->addPort(port, _applicationScheduler->addressReuseAllowed());
    }
    else {
      LOGGER_TRACE << "trying to open address " << bindAddress
                   << " on port " << port
                   << " for http requests";

      result = httpServer->addPort(bindAddress, port, _applicationScheduler->addressReuseAllowed());
    }

    if (result) {
      LOGGER_DEBUG << "opened port " << port << " for " << (bindAddress.empty() ? "any" : bindAddress);
    }
    else {
      LOGGER_TRACE << "failed to open port " << port << " for " << (bindAddress.empty() ? "any" : bindAddress);
      addresses.push_back(ap);

      if (scheduler->isShutdownInProgress()) {
        addresses.clear();
      }
      else {
        sleep(1);
      }
    }
  }

  return httpServer;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
