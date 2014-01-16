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
#include "Basics/JsonHelper.h"
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
    class ClusterInfo;
    
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
      friend class ClusterInfo;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        CollectionInfo ();
        
        CollectionInfo (struct TRI_json_s*);

        CollectionInfo (CollectionInfo const&);

        CollectionInfo& operator= (CollectionInfo const&);
        
        ~CollectionInfo ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t id () const {
          return triagens::basics::JsonHelper::stringUInt64(_json, "id");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection name
////////////////////////////////////////////////////////////////////////////////

        std::string name () const {
          return triagens::basics::JsonHelper::getStringValue(_json, "name", "");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection type
////////////////////////////////////////////////////////////////////////////////

        TRI_col_type_e type () const {
          return triagens::basics::JsonHelper::getNumericValue<TRI_col_type_e>(_json, "type", TRI_COL_TYPE_UNKNOWN);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e status () const {
          return triagens::basics::JsonHelper::getNumericValue<TRI_vocbase_col_status_e>(_json, "status", TRI_VOC_COL_STATUS_CORRUPTED);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status as a string
////////////////////////////////////////////////////////////////////////////////

        std::string statusString () const {
          return TRI_GetStatusStringCollectionVocBase(status());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the deleted flag
////////////////////////////////////////////////////////////////////////////////

        bool deleted () const {
          return triagens::basics::JsonHelper::getBooleanValue(_json, "deleted", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the docompact flag
////////////////////////////////////////////////////////////////////////////////

        bool doCompact () const {
          return triagens::basics::JsonHelper::getBooleanValue(_json, "doCompact", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the issystem flag
////////////////////////////////////////////////////////////////////////////////

        bool isSystem () const {
          return triagens::basics::JsonHelper::getBooleanValue(_json, "isSystem", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the isvolatile flag
////////////////////////////////////////////////////////////////////////////////

        bool isVolatile () const {
          return triagens::basics::JsonHelper::getBooleanValue(_json, "isVolatile", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a copy of the key options
/// the caller is responsible for freeing it 
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* keyOptions () const {
          TRI_json_t const* keyOptions = triagens::basics::JsonHelper::getArrayElement(_json, "keyOptions");

          if (keyOptions != 0) {
            return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
          }

          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the waitforsync flag
////////////////////////////////////////////////////////////////////////////////

        bool waitForSync () const {
          return triagens::basics::JsonHelper::getBooleanValue(_json, "waitForSync", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the maximal journal size
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_size_t maximalSize () const {
          return triagens::basics::JsonHelper::getNumericValue<TRI_voc_size_t>(_json, "maximalSize", 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> shardKeys () const {
          TRI_json_t* const node = triagens::basics::JsonHelper::getArrayElement(_json, "shardKeys");
          return triagens::basics::JsonHelper::stringList(node);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::string> shardIds () const {
          TRI_json_t* const node = triagens::basics::JsonHelper::getArrayElement(_json, "shards");
          return triagens::basics::JsonHelper::stringObject(node);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      
      private:
     
        TRI_json_t*                        _json;   
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

        bool doesDatabaseExist (DatabaseID const&,
                                bool = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

        vector<DatabaseID> listDatabases (bool = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadCurrentCollections ();

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the list of planned databases
////////////////////////////////////////////////////////////////////////////////

        void clearPlannedDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the list of current databases
////////////////////////////////////////////////////////////////////////////////

        void clearCurrentDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about planned databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadPlannedDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadCurrentDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once.
////////////////////////////////////////////////////////////////////////////////

        CollectionInfo getCollection (DatabaseID const&,
                                      CollectionID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

        TRI_col_info_t getCollectionProperties (CollectionInfo const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

        TRI_col_info_t getCollectionProperties (DatabaseID const&,
                                                CollectionID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

        const std::vector<CollectionInfo> getCollections (DatabaseID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief create database in coordinator
////////////////////////////////////////////////////////////////////////////////

        int createDatabaseCoordinator (string const& name, 
                                       TRI_json_t const* json,
                                       string errorMsg, double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief drop database in coordinator
////////////////////////////////////////////////////////////////////////////////

        int dropDatabaseCoordinator (string const& name, string& errorMsg, 
                                     double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator
////////////////////////////////////////////////////////////////////////////////

        int createCollectionCoordinator (string const& databaseName, 
                                         string const& collectionID,
                                         uint64_t numberOfShards,
                                         TRI_json_t const* json,
                                         string errorMsg, double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collection in coordinator
////////////////////////////////////////////////////////////////////////////////

        int dropCollectionCoordinator (string const& databaseName, 
                                       string const& collectionID,
                                       string& errorMsg, 
                                       double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadCurrentDBServers ();

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

        std::vector<ServerID> getCurrentDBServers ();

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
        std::map<DatabaseID, struct TRI_json_s*> _plannedDatabases; // from Plan/Databases
        std::map<DatabaseID, std::map<ServerID, struct TRI_json_s*> > _currentDatabases; // from Current/Databases

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

        static const uint64_t MinIdsPerBatch = 100;

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
