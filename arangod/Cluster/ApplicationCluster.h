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

namespace triagens {
  namespace arango {

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

        ApplicationCluster ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationCluster ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

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
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server's endpoint by scanning Target/MapIDToEnpdoint for 
/// our id
////////////////////////////////////////////////////////////////////////////////
  
         std::string getEndpointForId () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/Coordinators for our id
////////////////////////////////////////////////////////////////////////////////
  
         ServerState::RoleEnum checkCoordinatorsList () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server role by scanning Plan/DBServers for our id
////////////////////////////////////////////////////////////////////////////////

         ServerState::RoleEnum checkServersList () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

       private:

////////////////////////////////////////////////////////////////////////////////
/// @brief thread for heartbeat
////////////////////////////////////////////////////////////////////////////////

         HeartbeatThread* _heartbeat;

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat interval (in ms)
///
/// @CMDOPT{\--cluster.heartbeat-interval @CA{interval}}
///
/// Specifies the cluster heartbeat interval in milliseconds. Lower values 
/// mean more frequent heartbeats, but may result in higher overhead (more
/// network traffic and CPU usage).
///
/// The default value is `1000` ms.
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

    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
