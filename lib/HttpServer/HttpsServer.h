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

#ifndef ARANGODB_HTTP_SERVER_HTTPS_SERVER_H
#define ARANGODB_HTTP_SERVER_HTTPS_SERVER_H 1

#include "GeneralServer/GeneralSslServer.h"

#include "Basics/Common.h"

#include <openssl/ssl.h>

#include "Basics/ssl-helper.h"
#include "Basics/logging.h"

#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/GeneralHttpServer.h"
#include "HttpServer/HttpCommTask.h"
#include "HttpServer/HttpHandler.h"
#include "Scheduler/Scheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 class HttpsServer
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief http server
////////////////////////////////////////////////////////////////////////////////

    class HttpsServer : public GeneralSslServer< HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer> >,
                        public GeneralHttpServer< HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer> > {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http server
////////////////////////////////////////////////////////////////////////////////

        HttpsServer (Scheduler* scheduler,
                     Dispatcher* dispatcher,
                     AsyncJobManager* jobManager,
                     double keepAliveTimeout,
                     HttpHandlerFactory* handlerFactory,
                     SSL_CTX* ctx)
        : GeneralServer<HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer> >(scheduler, keepAliveTimeout),
          GeneralSslServer<HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer> >(scheduler, dispatcher, keepAliveTimeout, handlerFactory, ctx),
          GeneralHttpServer<HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer> >(scheduler, dispatcher, jobManager, keepAliveTimeout, handlerFactory) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~HttpsServer () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the protocol
////////////////////////////////////////////////////////////////////////////////

        static const char* protocol () {
          return "https";
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleConnected (TRI_socket_t socket, ConnectionInfo& info) {
          GeneralSslServer< HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer> >::handleConnected(socket, info);
        }

        virtual Endpoint::EncryptionType getEncryption() const {
          return  GeneralSslServer < HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer>>::getEncryption();
        }

        virtual void shutdownHandlers() {
          GeneralHttpServer < HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer>>::shutdownHandlers();
        }

        bool handleRequest(HttpCommTask<HttpsServer> * task, HttpHandlerFactory::GeneralHandler* handler) {
          return  GeneralHttpServer < HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer>>::handleRequest(task, handler);
        }

        virtual void shutdownHandlerByTask(Task* task) {
          GeneralHttpServer < HttpsServer, HttpHandlerFactory, HttpCommTask<HttpsServer>>::shutdownHandlerByTask(task);
        }

    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
