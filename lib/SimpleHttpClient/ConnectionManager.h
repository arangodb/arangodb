////////////////////////////////////////////////////////////////////////////////
/// @brief manages open HTTP connections on the client side
///
/// @file ConnectionManager.h
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_CONNECTION_MANAGER_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_CONNECTION_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "SimpleHttpClient/GeneralClientConnection.h"

namespace triagens {
  namespace httpclient {

// -----------------------------------------------------------------------------
// --SECTION--                                                ConnectionManager
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief options for connections
////////////////////////////////////////////////////////////////////////////////

    struct ConnectionOptions {
      double _connectTimeout;
      double _requestTimeout;
      size_t _connectRetries;
      double _singleRequestTimeout;
      uint32_t _sslProtocol;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief the class to manage open client connections
////////////////////////////////////////////////////////////////////////////////

    class ConnectionManager {

// -----------------------------------------------------------------------------
// --SECTION--                                     constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises library
///
/// We are a singleton class, therefore nobody is allowed to create
/// new instances or copy them, except we ourselves.
////////////////////////////////////////////////////////////////////////////////

        ConnectionManager ( ) { }
        ConnectionManager (ConnectionManager const&);    // not implemented
        void operator= (ConnectionManager const&);       // not implemented

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down library
////////////////////////////////////////////////////////////////////////////////

      public:

        ~ConnectionManager ( );

// -----------------------------------------------------------------------------
// --SECTION--                                                public subclasses
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class to administrate one connection to a server
////////////////////////////////////////////////////////////////////////////////

        struct SingleServerConnection {
          GeneralClientConnection* connection;
          triagens::rest::Endpoint* endpoint;
          time_t lastUsed;
          std::string ep_spec;

          SingleServerConnection (GeneralClientConnection* c,
                                  triagens::rest::Endpoint* e,
                                  std::string& ep_spec)
            : connection(c), 
              endpoint(e), 
              lastUsed(0), 
              ep_spec(ep_spec) {
          }

          ~SingleServerConnection ();

        };

////////////////////////////////////////////////////////////////////////////////
/// @brief class to administrate all connections to a server
////////////////////////////////////////////////////////////////////////////////

        struct ServerConnections {
          std::vector<SingleServerConnection*> connections;
          std::list<SingleServerConnection*> unused;
          triagens::basics::ReadWriteLock lock;

          ServerConnections () {
          }

          ~ServerConnections ();   // closes all connections
        };

// -----------------------------------------------------------------------------
// --SECTION--                                                   public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the unique instance
////////////////////////////////////////////////////////////////////////////////

        static ConnectionManager* instance () {
          // This does not have to be thread-safe, because we guarantee that
          // this is called very early in the startup phase when there is still
          // a single thread.
          if (nullptr == _theinstance) {
            _theinstance = new ConnectionManager( );
            // this now happens exactly once
          }
          return _theinstance;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise function to call once when still single-threaded
////////////////////////////////////////////////////////////////////////////////

        static void initialise () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

        static void cleanup () {
          delete _theinstance;
          _theinstance = nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief open or get a previously cached connection to a server
////////////////////////////////////////////////////////////////////////////////

        SingleServerConnection* leaseConnection (std::string& endpoint);

////////////////////////////////////////////////////////////////////////////////
/// @brief return leased connection to a server
////////////////////////////////////////////////////////////////////////////////

        void returnConnection (SingleServerConnection* singleConnection);

////////////////////////////////////////////////////////////////////////////////
/// @brief report a leased connection as being broken
////////////////////////////////////////////////////////////////////////////////

        void brokenConnection(SingleServerConnection* singleConnection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all connections that have been unused for more than
/// limit seconds
////////////////////////////////////////////////////////////////////////////////

        void closeUnusedConnections (double limit);

// -----------------------------------------------------------------------------
// --SECTION--                                         private methods and data
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
////////////////////////////////////////////////////////////////////////////////

        static ConnectionManager* _theinstance;

////////////////////////////////////////////////////////////////////////////////
/// @brief global options for connections
////////////////////////////////////////////////////////////////////////////////

        static ConnectionOptions _globalConnectionOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief map to store all connections to all servers with corresponding lock
////////////////////////////////////////////////////////////////////////////////

        // We keep connections to servers open:
        std::map<std::string, ServerConnections*> allConnections;
        triagens::basics::ReadWriteLock allLock;

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
