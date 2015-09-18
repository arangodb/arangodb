////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to establish connections
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_HTTP_LISTEN_TASK_H
#define ARANGODB_HTTP_SERVER_HTTP_LISTEN_TASK_H 1

#include "Scheduler/ListenTask.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              class HttpListenTask
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class HttpServer;
    class Endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief task used to establish connections
////////////////////////////////////////////////////////////////////////////////

    class HttpListenTask : public ListenTask {
      HttpListenTask (HttpListenTask const&) = delete;
      HttpListenTask& operator= (HttpListenTask const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief listen to given port
////////////////////////////////////////////////////////////////////////////////

        HttpListenTask (HttpServer* server, Endpoint* endpoint);

// -----------------------------------------------------------------------------
// --SECTION--                                                ListenTask methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleConnected (TRI_socket_t s, ConnectionInfo const& info);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying general server
////////////////////////////////////////////////////////////////////////////////

        HttpServer* _server;
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
