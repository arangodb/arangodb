////////////////////////////////////////////////////////////////////////////////
/// @brief sharding configuration
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_APPLICATION_CLUSTER_H
#define ARANGODB_CLUSTER_APPLICATION_CLUSTER_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationFeature.h"
#include "Cluster/ServerState.h"

struct TRI_server_s;

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
                            triagens::arango::ApplicationV8*);

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

        void setupOptions (std::map<std::string, basics::ProgramOptionsDescription>&);

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
/// @brief heartbeat interval (in milliseconds)
////////////////////////////////////////////////////////////////////////////////

         uint64_t _heartbeatInterval;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of agency endpoints
/// @startDocuBlock clusterAgencyEndpoint
/// `--cluster.agency-endpoint endpoint`
///
/// An agency endpoint the server can connect to. The option can be specified
/// multiple times so the server can use a cluster of agency servers. Endpoints
/// have the following pattern:
///
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
/// ```
/// --cluster.agency-endpoint tcp://192.168.1.1:4001 --cluster.agency-endpoint tcp://192.168.1.2:4002
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

         std::vector<std::string> _agencyEndpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief global agency prefix
/// @startDocuBlock clusterAgencyPrefix
/// `--cluster.agency-prefix prefix`
///
/// The global key prefix used in all requests to the agency. The specified
/// prefix will become part of each agency key. Specifying the key prefix
/// allows managing multiple ArangoDB clusters with the same agency
/// server(s).
///
/// *prefix* must consist of the letters *a-z*, *A-Z* and the digits *0-9*
/// only. Specifying a prefix is mandatory.
///
/// @EXAMPLES
///
/// ```
/// --cluster.prefix mycluster
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

         std::string _agencyPrefix;

////////////////////////////////////////////////////////////////////////////////
/// @brief this server's id
/// @startDocuBlock clusterMyId
/// `--cluster.my-id id
///
/// The local server's id in the cluster. Specifying *id* is mandatory on
/// startup. Each server of the cluster must have a unique id.
///
/// Specifying the id is very important because the server id is used for
/// determining the server's role and tasks in the cluster.
///
/// *id* must be a string consisting of the letters *a-z*, *A-Z* or the
/// digits *0-9* only.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

         std::string _myId;

////////////////////////////////////////////////////////////////////////////////
/// @brief this server's address / endpoint
/// @startDocuBlock clusterMyAddress
/// `--cluster.my-address endpoint`
///
/// The server's endpoint for cluster-internal communication. If specified, it
/// must have the following pattern:
/// - tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
/// - tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
/// - ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
/// - ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
///
/// If no *endpoint* is specified, the server will look up its internal
/// endpoint address in the agency. If no endpoint can be found in the agency
/// for the server's id, ArangoDB will refuse to start.
///
/// @EXAMPLES
///
/// ```
/// --cluster.my-address tcp://192.168.1.1:8530
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

         std::string _myAddress;

////////////////////////////////////////////////////////////////////////////////
/// @brief username used for cluster-internal communication
/// @startDocuBlock clusterUsername
/// `--cluster.username username`
///
/// The username used for authorization of cluster-internal requests.
/// This username will be used to authenticate all requests and responses in
/// cluster-internal communication, i.e. requests exchanged between coordinators
/// and individual database servers.
///
/// This option is used for cluster-internal requests only. Regular requests to
/// coordinators are authenticated normally using the data in the *_users*
/// collection.
///
/// If coordinators and database servers are run with authentication turned off,
/// (e.g. by setting the *--server.disable-authentication* option to *true*),
/// the cluster-internal communication will also be unauthenticated.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

         std::string _username;

////////////////////////////////////////////////////////////////////////////////
/// @brief password used for cluster-internal communication
/// @startDocuBlock clusterPassword
/// `--cluster.password password`
///
/// The password used for authorization of cluster-internal requests.
/// This password will be used to authenticate all requests and responses in
/// cluster-internal communication, i.e. requests exchanged between coordinators
/// and individual database servers.
///
/// This option is used for cluster-internal requests only. Regular requests to
/// coordinators are authenticated normally using the data in the `_users`
/// collection.
///
/// If coordinators and database servers are run with authentication turned off,
/// (e.g. by setting the *--server.disable-authentication* option to *true*),
/// the cluster-internal communication will also be unauthenticated.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

         std::string _password;

////////////////////////////////////////////////////////////////////////////////
/// @brief data path for the cluster
///
/// @CMDOPT{\--cluster.data-path @CA{path}}
///
/// The default directory where the databases for the cluster processes are
/// stored.
////////////////////////////////////////////////////////////////////////////////

         std::string _dataPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief log path for the cluster
///
/// @CMDOPT{\--cluster.log-path @CA{path}}
///
/// The default directory where the log files for the cluster processes are
/// stored.
////////////////////////////////////////////////////////////////////////////////

         std::string _logPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief agent path for the cluster
///
/// @CMDOPT{\--cluster.agent-path @CA{path}}
///
/// The path to agent executable.
////////////////////////////////////////////////////////////////////////////////

         std::string _agentPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief arangod path for the cluster
///
/// @CMDOPT{\--cluster.arangod-path @CA{path}}
///
/// The path to arangod executable.
////////////////////////////////////////////////////////////////////////////////

         std::string _arangodPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief DBserver config for the cluster
///
/// @CMDOPT{\--cluster.dbserver-config @CA{path}}
///
/// The configuration file for the DBserver.
////////////////////////////////////////////////////////////////////////////////

         std::string _dbserverConfig;

////////////////////////////////////////////////////////////////////////////////
/// @brief coordinator config for the cluster
///
/// @CMDOPT{\--cluster.coordinator-config @CA{path}}
///
/// The configuration file for the coordinator.
////////////////////////////////////////////////////////////////////////////////

         std::string _coordinatorConfig;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable the dispatcher frontend
///
/// @CMDOPT{\--server.disable-dispatcher-interface @CA{flag}}
///
/// If @LIT{true} the server can be used as dispatcher for a cluster. If you
/// enable this option, you should secure access to the dispatcher with a
/// password.
///
/// The default is @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

        bool _disableDispatcherFrontend;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable the dispatcher kickstarter
///
/// @CMDOPT{\--server.disable-dispatcher-kickstarter @CA{flag}}
///
/// If @LIT{true} the server can be used as kickstarter to start processes for a
/// cluster. If you enable this option, you should secure access to the
/// dispatcher with a password.
///
/// The default is @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

        bool _disableDispatcherKickstarter;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the cluster feature is enabled
////////////////////////////////////////////////////////////////////////////////

         bool _enableCluster;

////////////////////////////////////////////////////////////////////////////////
/// @brief flag for turning off heartbeat (used for testing)
////////////////////////////////////////////////////////////////////////////////

         bool _disableHeartbeat;

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
