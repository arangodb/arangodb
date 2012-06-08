////////////////////////////////////////////////////////////////////////////////
/// @brief application server http server implementation
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

#include "ApplicationHttpServerImpl.h"

#include <Basics/delete_object.h>
#include <Basics/ProgramOptions.h>
#include <Basics/ProgramOptionsDescription.h>

#include "Dispatcher/ApplicationServerDispatcher.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServerImpl.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationHttpServerImpl::ApplicationHttpServerImpl (ApplicationServer* applicationServer)
      : applicationServer(applicationServer),
        showPort(true),
        requireKeepAlive(false),
        httpServers(),
        httpPorts(),
        httpAddressPorts() {
    }



    ApplicationHttpServerImpl::~ApplicationHttpServerImpl () {
      for_each(httpServers.begin(), httpServers.end(), DeleteObject());
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ApplicationHttpServerImpl::setupOptions (map<string, ProgramOptionsDescription>& options) {
      if (showPort) {
        options[ApplicationServer::OPTIONS_SERVER]
          ("server.port", &httpPorts, "listen port or address:port")
        ;
      }

      options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
        ("server.require-keep-alive", "close connection, if keep-alive is missing")
      ;
    }



    bool ApplicationHttpServerImpl::parsePhase2 (ProgramOptions& options) {
      if (options.has("server.require-keep-alive")) {
        requireKeepAlive= true;
      }

      for (vector<string>::const_iterator i = httpPorts.begin();  i != httpPorts.end();  ++i) {
        addPort(*i);
      }

      return true;
    }



    AddressPort ApplicationHttpServerImpl::addPort (string const& name) {
      AddressPort ap;

      if (! ap.split(name)) {
        LOGGER_ERROR << "unknown server:port definition '" << name << "'";
      }
      else {
        httpAddressPorts.push_back(ap);
      }

      return ap;
    }



    HttpServer* ApplicationHttpServerImpl::buildServer (HttpHandlerFactory* httpHandlerFactory) {
      return buildHttpServer(0, httpHandlerFactory, httpAddressPorts);
    }



    HttpServer* ApplicationHttpServerImpl::buildServer (HttpHandlerFactory* httpHandlerFactory, vector<AddressPort> const& ports) {
      if (ports.empty()) {
        delete httpHandlerFactory;
        return 0;
      }
      else {
        return buildHttpServer(0, httpHandlerFactory, ports);
      }
    }



    HttpServer* ApplicationHttpServerImpl::buildServer (HttpServer* httpServer, HttpHandlerFactory* httpHandlerFactory, vector<AddressPort> const& ports) {
      if (ports.empty()) {
        delete httpHandlerFactory;
        return 0;
      }
      else {
        HttpServerImpl* impl = dynamic_cast<HttpServerImpl*>(httpServer);

        return buildHttpServer(impl, httpHandlerFactory, ports);
      }
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    HttpServerImpl* ApplicationHttpServerImpl::buildHttpServer (HttpServerImpl* httpServer,
                                                                HttpHandlerFactory* httpHandlerFactory,
                                                                vector<AddressPort> const& ports) {
      Scheduler* scheduler = applicationServer->scheduler();

      if (scheduler == 0) {
        LOGGER_FATAL << "no scheduler is known, cannot create http server";
        TRI_ShutdownLogging();
        exit(EXIT_FAILURE);
      }

      // create new server
      if (httpServer == 0) {
        Dispatcher* dispatcher = 0;
        ApplicationServerDispatcher* asd = dynamic_cast<ApplicationServerDispatcher*>(applicationServer);

        if (asd != 0) {
          dispatcher = asd->dispatcher();
        }

        httpServer = new HttpServerImpl(scheduler, dispatcher);
      }

      httpServer->setHandlerFactory(httpHandlerFactory);

      if (requireKeepAlive) {
        httpServer->setCloseWithoutKeepAlive(true);
      }

      // keep a list of active server
      httpServers.push_back(httpServer);

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

          result = httpServer->addPort(port, applicationServer->addressReuseAllowed());
        }
        else {
          LOGGER_TRACE << "trying to open address " << bindAddress
                       << " on port " << port
                       << " for http requests";

          result = httpServer->addPort(bindAddress, port, applicationServer->addressReuseAllowed());
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
  }
}
