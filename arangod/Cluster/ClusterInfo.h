////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterInfo.h
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
// --SECTION--                                       some types for ClusterInfo
// -----------------------------------------------------------------------------

    typedef string ServerID;              // ID of a server
    typedef string DatabaseID;            // ID/name of a database
    typedef string CollectionID;          // ID of a collection
    typedef string ShardID;               // ID of a shard

// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectionInfo
// -----------------------------------------------------------------------------

    struct CollectionInfo {
        enum Status {
          LOADED = 1,
          UNLOADED = 2
        };
        enum Type {
          NORMAL = 1,
          EDGES = 2
        };
        DatabaseID database;
        string id;
        string name;
        Status status;
        Type type;
        vector<string> shards;

        CollectionInfo () : id(""), name(""), status(LOADED), type(NORMAL) {
        }
        ~CollectionInfo () {
        }
    };

    
// -----------------------------------------------------------------------------
// --SECTION--                                                class ClusterInfo
// -----------------------------------------------------------------------------

    class ClusterInfo {

// -----------------------------------------------------------------------------
// --SECTION--                                     constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises library
///
/// We are a singleton class, therefore nobody is allowed to create
/// new instances or copy them, except we ourselves.
////////////////////////////////////////////////////////////////////////////////
      
        ClusterInfo ();
        ClusterInfo (ClusterInfo const&);    // not implemented
        void operator= (ClusterInfo const&);  // not implemented

        static ClusterInfo* _theinstance;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down library
////////////////////////////////////////////////////////////////////////////////

        ~ClusterInfo ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the unique instance
////////////////////////////////////////////////////////////////////////////////

        static ClusterInfo* instance ();

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
/// @brief find the endpoint of a server from its ID.
///
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

        string getServerEndpoint(ServerID const& serverID);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
///
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadServerInformation ();

////////////////////////////////////////////////////////////////////////////////
/// @brief find the server who is responsible for a shard
///
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

        ServerID getResponsibleServer(ShardID const& shardID);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about shards from the agency
///
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadShardInformation ();

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

        bool doesDatabaseExist (DatabaseID const& databaseID);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about databases from the agency
///
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadDatabaseInformation ();

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
///
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty 0 pointer is returned. The caller
/// must not delete the pointer.
////////////////////////////////////////////////////////////////////////////////

        CollectionInfo const* getCollectionInfo(
                                   DatabaseID const& databaseID,
                                   CollectionID const& collectionID);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about collections from the agency
///
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadCollectionsInformation ();

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
        map<CollectionID,CollectionInfo*> collections;
                                               // from Current/Collections/

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


