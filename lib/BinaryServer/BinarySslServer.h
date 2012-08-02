////////////////////////////////////////////////////////////////////////////////
/// @brief binary ssl server
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

#ifndef TRIAGENS_HTTP_SERVER_BINARY_SSL_SERVER_H
#define TRIAGENS_HTTP_SERVER_BINARY_SSL_SERVER_H 1

#include "GeneralServer/GeneralSslServer.h"
#include "HttpServer/GeneralBinaryServer.h"

#include <openssl/ssl.h>

#include "Basics/ssl-helper.h"
#include "Logger/Logger.h"

#include "HttpServer/BinaryCommTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/SslAsyncCommTask.h"
#include "Scheduler/Scheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                             class BinarySslServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief http server
////////////////////////////////////////////////////////////////////////////////

    class BinarySslServer : public GeneralSslServer<BinarySslServer, HttpHandlerFactory, BinaryCommTask<BinarySslServer> >,
                            public GeneralBinaryServer<BinarySslServer, HttpHandlerFactory, BinaryCommTask<BinarySslServer> > {

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http server
////////////////////////////////////////////////////////////////////////////////

        BinarySslServer (Scheduler* scheduler,
                         Dispatcher* dispatcher,
                         HttpHandlerFactory* handlerFactory,
                         SSL_CTX* ctx) 
        : GeneralServer<BinarySslServer, HttpHandlerFactory, BinaryCommTask<BinarySslServer> >(scheduler),
          GeneralServerDispatcher<BinarySslServer, HttpHandlerFactory, BinaryCommTask<BinarySslServer> >(scheduler, dispatcher),
          GeneralSslServer<BinarySslServer, HttpHandlerFactory, BinaryCommTask<BinarySslServer> >(scheduler, dispatcher, handlerFactory, ctx), 
          GeneralBinaryServer<BinarySslServer, HttpHandlerFactory, BinaryCommTask<BinarySslServer> >(scheduler, dispatcher, handlerFactory) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~BinarySslServer () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////
        
        void handleConnected (socket_t socket, ConnectionInfo& info) {
          GeneralSslServer::handleConnected(socket, info);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
