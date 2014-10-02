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

#ifndef ARANGODB_GENERAL_SERVER_GENERAL_LISTEN_TASK_H
#define ARANGODB_GENERAL_SERVER_GENERAL_LISTEN_TASK_H 1

#include "Basics/Common.h"

#include "Basics/socket-utils.h"
#include "Scheduler/ListenTask.h"
#include "Rest/Endpoint.h"

// -----------------------------------------------------------------------------
// --SECTION--                                           class GeneralListenTask
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief task used to establish connections
////////////////////////////////////////////////////////////////////////////////

    template<typename S>
    class GeneralListenTask : public ListenTask {
      private:
        GeneralListenTask (GeneralListenTask const&);
        GeneralListenTask& operator= (GeneralListenTask const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief listen to given port
////////////////////////////////////////////////////////////////////////////////

        GeneralListenTask (S* server, Endpoint* endpoint)
          : Task("GeneralListenTask"),
            ListenTask(endpoint),
            server(server) {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                ListenTask methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleConnected (TRI_socket_t s, ConnectionInfo const& info) {
          ConnectionInfo newInfo = info;
          server->handleConnected(s, newInfo);
          return true;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying general server
////////////////////////////////////////////////////////////////////////////////

        S* server;
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
