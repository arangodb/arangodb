////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterState.h
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_CLUSTER_STATE_H
#define TRIAGENS_CLUSTER_STATE_H 1

#include "Basics/Common.h"
#include "Cluster/AgencyComm.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace triagens {
  namespace arango {
    
// -----------------------------------------------------------------------------
// --SECTION--                                      some types for ClusterState
// -----------------------------------------------------------------------------

    typedef string ServerID;              // ID of a server
    typedef string CollectionID;          // ID of a collection
    typedef string ShardID;               // ID of a shard

// -----------------------------------------------------------------------------
// --SECTION--                                               class ClusterState
// -----------------------------------------------------------------------------

    class ClusterState {

// -----------------------------------------------------------------------------
// --SECTION--                                     constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises library
///
/// We are a singleton class, therefore nobody is allowed to create
/// new instances or copy them, except we ourselves.
////////////////////////////////////////////////////////////////////////////////
      
        ClusterState ();
        ClusterState (ClusterState const&);    // not implemented
        void operator= (ClusterState const&);  // not implemented

        static ClusterState* _theinstance;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down library
////////////////////////////////////////////////////////////////////////////////

        ~ClusterState ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the unique instance
////////////////////////////////////////////////////////////////////////////////

        static ClusterState* instance ();

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise function to call once when still single-threaded
////////////////////////////////////////////////////////////////////////////////
        
        static void initialise ();

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////
        
        static void cleanup () {
          delete _theinstance;
          _theinstance = 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
////////////////////////////////////////////////////////////////////////////////

        void loadServerInformation ();

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about shards from the agency
////////////////////////////////////////////////////////////////////////////////

        void loadShardInformation ();

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID
////////////////////////////////////////////////////////////////////////////////

        string getServerEndpoint(ServerID const& serverID);

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
////////////////////////////////////////////////////////////////////////////////

        string getCollectionInfo(CollectionID const& collectionID);

////////////////////////////////////////////////////////////////////////////////
/// @brief get all shards in a collection
////////////////////////////////////////////////////////////////////////////////

        void getShardsCollection(CollectionID const& collectionID,
                                 vector<ShardID> &shards);

////////////////////////////////////////////////////////////////////////////////
/// @brief find the server who is responsible for a shard
////////////////////////////////////////////////////////////////////////////////

        ServerID getResponsibleServer(ShardID const& shardID);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a number of cluster-wide unique IDs, returns the first
/// one and guarantees that <number> are reserved for the caller.
////////////////////////////////////////////////////////////////////////////////
        
        uint64_t fetchIDs (uint64_t number);

      private:

        AgencyComm _agency;

        // Cached data from the agency, we reload whenever necessary:
        map<ServerID,string> serverAddresses;  // from Current/ServersRegistered
        map<ShardID,ServerID> shards;          // from Current/ShardLocation

        triagens::basics::ReadWriteLock lock;

    };

  }  // end namespace arango
}  // end namespace triagens

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


