////////////////////////////////////////////////////////////////////////////////
/// @brief http server implementation
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
/// @author Achim Brandt
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef FYN_HTTPSERVER_HTTP_SERVER_IMPL_H
#define FYN_HTTPSERVER_HTTP_SERVER_IMPL_H 1

#include <Rest/HttpServer.h>

#include "HttpServer/HttpCommTask.h"
#include "HttpServer/ServiceUnavailableHandler.h"


#include "GeneralServer/GeneralServerDispatcher.h"
#define HTTP_SERVER_SUPER_CLASS GeneralServerDispatcher<HttpServerImpl, HttpHandlerFactory, HttpCommTask>


namespace triagens {
  namespace rest {
    class HttpHandlerFactory;
    class HttpListenTask;
    class HttpRequest;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief http server implementation
    ////////////////////////////////////////////////////////////////////////////////

    class HttpServerImpl : public HTTP_SERVER_SUPER_CLASS, virtual public HttpServer {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http server
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        HttpServerImpl (Scheduler* scheduler)
          : HTTP_SERVER_SUPER_CLASS(scheduler),
            _closeWithoutKeepAlive(false) {
        }


        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http server
        ////////////////////////////////////////////////////////////////////////////////

        HttpServerImpl (Scheduler* scheduler, Dispatcher* dispatcher)
          : HTTP_SERVER_SUPER_CLASS(scheduler, dispatcher),
            _closeWithoutKeepAlive(false) {
        }


      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void activateMaintenance () {
          GeneralServer<HttpServerImpl, HttpHandlerFactory, HttpCommTask>::activateMaintenance();
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void activateMaintenance (MaintenanceCallback* callback) {
          GeneralServer<HttpServerImpl, HttpHandlerFactory, HttpCommTask>::activateMaintenance();
          _handlerFactory->addMaintenanceCallback(callback);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void deactivateMaintenance () {
          GeneralServer<HttpServerImpl, HttpHandlerFactory, HttpCommTask>::deactivateMaintenance();
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        size_t numberActiveHandlers () {
          return _handlerFactory == 0 ? 0 : _handlerFactory->numberActiveHandlers();
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool getCloseWithoutKeepAlive () const {
          return _closeWithoutKeepAlive;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setCloseWithoutKeepAlive (bool value = true) {
          _closeWithoutKeepAlive = value;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setHandlerFactory (HttpHandlerFactory* factory) {
          GeneralServer<HttpServerImpl, HttpHandlerFactory, HttpCommTask>::setHandlerFactory(factory);
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief creates a maintenance request
        ////////////////////////////////////////////////////////////////////////////////

        HttpRequest* createMaintenanceRequest (char const* ptr, size_t length) const {
          return new HttpRequest(ptr, length);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief creates a maintenance handler
        ////////////////////////////////////////////////////////////////////////////////

        HttpHandler* createMaintenanceHandler () const {
          return new ServiceUnavailableHandler();
        }

      private:
        bool _closeWithoutKeepAlive;
    };
  }
}

#endif
