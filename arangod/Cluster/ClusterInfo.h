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
/// @author Jan Steemann
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_CLUSTER_CLUSTER_INFO_H
#define TRIAGENS_CLUSTER_CLUSTER_INFO_H 1

#include "Basics/Common.h"
#include "Cluster/AgencyComm.h"
#include "VocBase/collection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#ifdef __cplusplus
extern "C" {
  struct TRI_json_s;
  struct TRI_memory_zone_s;
#endif

namespace triagens {
  namespace arango {
    
// -----------------------------------------------------------------------------
// --SECTION--                                       some types for ClusterInfo
// -----------------------------------------------------------------------------

    typedef std::string ServerID;              // ID of a server
    typedef std::string DatabaseID;            // ID/name of a database
    typedef std::string CollectionID;          // ID of a collection
    typedef std::string ShardID;               // ID of a shard

// -----------------------------------------------------------------------------
// --SECTION--                                              class CollectionInfo
// -----------------------------------------------------------------------------

    class CollectionInfo {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        CollectionInfo ();
        
        CollectionInfo (std::string const&);
        
        ~CollectionInfo ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t cid () const {
          return _id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection name
////////////////////////////////////////////////////////////////////////////////

        std::string name () const {
          return _name;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection type
////////////////////////////////////////////////////////////////////////////////

        TRI_col_type_e type () const {
          return _type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e status () const {
          return _status;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status
////////////////////////////////////////////////////////////////////////////////

        std::string statusString () const {
          return TRI_GetStatusStringCollectionVocBase(_status);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief TODO: returns the indexes
////////////////////////////////////////////////////////////////////////////////

/*
        std::vector<std::string> indexes () const {
          return _indexes;
        }
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> shardKeys () const {
          return _shardKeys;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::string> shardIds () const {
          return _shardIds;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates a collection info object
////////////////////////////////////////////////////////////////////////////////

        void invalidate ();

////////////////////////////////////////////////////////////////////////////////
/// @brief populate object properties from the JSON given
////////////////////////////////////////////////////////////////////////////////

        bool createFromJson (struct TRI_json_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON string from the object
////////////////////////////////////////////////////////////////////////////////
        
        struct TRI_json_s* toJson (struct TRI_memory_zone_s*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
        
        TRI_voc_cid_t                      _id;
        std::string                        _name;
        TRI_col_type_e                     _type;
        TRI_vocbase_col_status_e           _status;
 
        // TODO: indexes
        std::vector<std::string>           _shardKeys;
        std::map<std::string, std::string> _shardIds;
    };

    
// -----------------------------------------------------------------------------
// --SECTION--                                                 class ClusterInfo
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

    class ClusterInfo {
      private:

        typedef std::map<CollectionID, CollectionInfo>     DatabaseCollections;
        typedef std::map<DatabaseID, DatabaseCollections>  AllCollections;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises library
/// We are a singleton class, therefore nobody is allowed to create
/// new instances or copy them, except we ourselves.
////////////////////////////////////////////////////////////////////////////////
      
        ClusterInfo ();
        ClusterInfo (ClusterInfo const&);    // not implemented
        void operator= (ClusterInfo const&);  // not implemented

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down library
////////////////////////////////////////////////////////////////////////////////
      
      public:

        ~ClusterInfo ();

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get the unique instance
////////////////////////////////////////////////////////////////////////////////

        static ClusterInfo* instance ();

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////
        
        static void cleanup () {
          delete _theinstance;
          _theinstance = 0;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get a number of cluster-wide unique IDs, returns the first
/// one and guarantees that <number> are reserved for the caller.
////////////////////////////////////////////////////////////////////////////////
        
        uint64_t uniqid (uint64_t = 1);

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing only)
////////////////////////////////////////////////////////////////////////////////

        void flush ();

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

        bool doesDatabaseExist (DatabaseID const& databaseID);

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

        vector<DatabaseID> listDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadCollections ();

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once.
////////////////////////////////////////////////////////////////////////////////

        CollectionInfo getCollectionInfo (DatabaseID const&,
                                          CollectionID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadDBServers ();

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

        std::vector<ServerID> getDBServers ();

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadServers ();

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

        std::string getServerEndpoint (ServerID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server's endpoint by scanning Target/MapIDToEnpdoint for 
/// our id
////////////////////////////////////////////////////////////////////////////////

        std::string getTargetServerEndpoint (ServerID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief find the server who is responsible for a shard
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

        ServerID getResponsibleServer (ShardID const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:
        
        AgencyComm                         _agency;
        triagens::basics::ReadWriteLock    _lock;

////////////////////////////////////////////////////////////////////////////////
/// @brief uniqid sequence
////////////////////////////////////////////////////////////////////////////////

        struct {
          uint64_t _currentValue;
          uint64_t _upperValue;
        }
        _uniqid;

        // Cached data from the agency, we reload whenever necessary:
        AllCollections                     _collections;  // from Current/Collections/
        bool                               _collectionsValid;
        std::map<ServerID, std::string>    _servers;      // from Current/ServersRegistered
        bool                               _serversValid;
        std::map<ServerID, ServerID>       _DBServers;    // from Current/DBServers
        bool                               _DBServersValid;
        std::map<ShardID, ServerID>        _shardIds;     // from Current/ShardLocation

// -----------------------------------------------------------------------------
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the sole instance
////////////////////////////////////////////////////////////////////////////////
        
        static ClusterInfo* _theinstance;

////////////////////////////////////////////////////////////////////////////////
/// @brief how big a batch is for unique ids
////////////////////////////////////////////////////////////////////////////////

        static const uint64_t MinIdsPerBatch = 1000;

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
