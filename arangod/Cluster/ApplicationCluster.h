////////////////////////////////////////////////////////////////////////////////
/// @brief sharding configuration
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann 
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_CLUSTER_APPLICATION_CLUSTER_H
#define TRIAGENS_CLUSTER_APPLICATION_CLUSTER_H 1

#include "ApplicationServer/ApplicationFeature.h"
#include "Cluster/ServerState.h"

extern "C" {
  struct TRI_server_s;
}

namespace triagens {
  namespace rest {
    class ApplicationDispatcher;
  }

  namespace arango {
    class ApplicationV8;
    class HeartbeatThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief sharding feature configuration
////////////////////////////////////////////////////////////////////////////////

    class ApplicationCluster: public rest::ApplicationFeature { 

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------
      
      private:
        ApplicationCluster (ApplicationCluster const&);
        ApplicationCluster& operator= (ApplicationCluster const&);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationCluster (struct TRI_server_s*,
                            triagens::rest::ApplicationDispatcher*,
                            triagens::arango::ApplicationV8*,
                            char* executablePath);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationCluster ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief disable the heartbeat (used for testing)
////////////////////////////////////////////////////////////////////////////////

        void disableHeartbeat () {
          _disableHeartbeat = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the cluster is enabled
////////////////////////////////////////////////////////////////////////////////

        inline bool enabled () const {
          return _enableCluster;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setupOptions (map<string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool open ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool start ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void close ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void stop ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

       private:

////////////////////////////////////////////////////////////////////////////////
/// @brief server
////////////////////////////////////////////////////////////////////////////////

         struct TRI_server_s* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher
////////////////////////////////////////////////////////////////////////////////

         triagens::rest::ApplicationDispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief v8 dispatcher
////////////////////////////////////////////////////////////////////////////////

         triagens::arango::ApplicationV8* _applicationV8;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread for heartbeat
////////////////////////////////////////////////////////////////////////////////

         HeartbeatThread* _heartbeat;

////////////////////////////////////////////////////////////////////////////////
/// @brief flag for turning off heartbeat (used for testing)
////////////////////////////////////////////////////////////////////////////////

         bool _disableHeartbeat;

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat interval (in milliseconds)
////////////////////////////////////////////////////////////////////////////////

         uint64_t _heartbeatInterval;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of agency endpoints
///
/// @CMDOPT{\--cluster.agency-endpoint @CA{endpoint}}
///
/// An agency endpoint the server can connect to. The option can be specified
/// multiple times so the server can use a cluster of agency servers. Endpoints
/// have the following pattern:
/// - tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
/// - tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
/// - ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
/// - ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
///
/// At least one endpoint must be specified or ArangoDB will refuse to start.
/// It is recommended to specify at least two endpoints so ArangoDB has an
/// alternative endpoint if one of them becomes unavailable.
///
/// @EXAMPLES
///
/// @code
/// --cluster.agency-endpoint tcp://192.168.1.1:4001 --cluster.agency-endpoint tcp://192.168.1.2:4002
/// @endcode
////////////////////////////////////////////////////////////////////////////////
         
         std::vector<std::string> _agencyEndpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief global agency prefix
///
/// @CMDOPT{\--cluster.agency-prefix @CA{prefix}}
///
/// The global key prefix used in all requests to the agency. The specified
/// prefix will become part of each agency key. Specifying the key prefix 
/// allows managing multiple ArangoDB clusters with the same agency
/// server(s). 
///
/// @CA{prefix} must consist of the letters `a-z`, `A-Z` and the digits `0-9`
/// only. Specifying a prefix is mandatory.
///
/// @EXAMPLES
///
/// @code
/// --cluster.prefix mycluster
/// @endcode
////////////////////////////////////////////////////////////////////////////////
         
         std::string _agencyPrefix;

////////////////////////////////////////////////////////////////////////////////
/// @brief this server's id
///
/// @CMDOPT{\--cluster.my-id @CA{id}}
///
/// The local server's id in the cluster. Specifying @CA{id} is mandatory on
/// startup. Each server of the cluster must have a unique id.
///
/// Specifying the id is very important because the server id is used for
/// determining the server's role and tasks in the cluster.
/// 
/// @CA{id} must be a string consisting of the letters `a-z`, `A-Z` or the 
/// digits `0-9` only.
////////////////////////////////////////////////////////////////////////////////

         std::string _myId;

////////////////////////////////////////////////////////////////////////////////
/// @brief this server's address / endpoint
///
/// @CMDOPT{\--cluster.my-address @CA{endpoint}}
///
/// The server's endpoint for cluster-internal communication. If specified, it
/// must have the following pattern:
/// - tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
/// - tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
/// - ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
/// - ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
///
/// If no @CA{endpoint} is specified, the server will look up its internal 
/// endpoint address in the agency. If no endpoint can be found in the agency
/// for the server's id, ArangoDB will refuse to start.
///
/// @EXAMPLES
///
/// @code
/// --cluster.my-address tcp://192.168.1.1:8530
/// @endcode
////////////////////////////////////////////////////////////////////////////////

         std::string _myAddress;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the cluster feature is enabled
////////////////////////////////////////////////////////////////////////////////

         bool _enableCluster;

////////////////////////////////////////////////////////////////////////////////
/// @brief our own executable name
////////////////////////////////////////////////////////////////////////////////

          std::string _myExecutablePath;
    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
