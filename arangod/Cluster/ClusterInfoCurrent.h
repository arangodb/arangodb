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
        
        CollectionInfo (ShardID const&, struct TRI_json_s*);

        CollectionInfo (CollectionInfo const&);

        CollectionInfo& operator= (CollectionInfo const&);
        
        ~CollectionInfo ();

      private:
        
        void freeAllJsons ();

        void copyAllJsons ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a new shardID and JSON pair, returns true if OK and false
/// if the shardID already exists. In the latter case nothing happens.
/// The CollectionInfo object takes ownership of the TRI_json_t*.
////////////////////////////////////////////////////////////////////////////////

        bool add (ShardID& shardID, TRI_json_t* json) {
          map<ShardID, TRI_json_t*>::iterator it = _jsons.find(shardID);
          if (it == _jsons.end()) {
            _jsons.insert(make_pair<ShardID, TRI_json_t*>(shardID, json));
            return true;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t id () const {
          // The id will always be the same in every shard
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            return triagens::basics::JsonHelper::stringUInt64(_json, "id");
          }
          else {
            return 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection name for one shardID
////////////////////////////////////////////////////////////////////////////////

        string name (ShardID const& shardID) const {
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getStringValue
                                      (_json, "name", "");
          }
          return string("");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection name for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, string> name () const {
          map<ShardID, string> m;
          map<ShardID, TRI_json_t*>::const_iterator it;
          string n;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            n = triagens::basics::JsonHelper::getStringValue(_json, "name", "");
            m.insert(make_pair<ShardID, string>(it->first,n));
          }
          return m;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a global name of the cluster collection
////////////////////////////////////////////////////////////////////////////////

        string globalName () const {
          // FIXME: do it
          return "Hans";
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection type
////////////////////////////////////////////////////////////////////////////////

        TRI_col_type_e type () const {
          // The type will always be the same in every shard
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            return triagens::basics::JsonHelper::getNumericValue<TRI_col_type_e>
                                      (_json, "type", TRI_COL_TYPE_UNKNOWN);
          }
          else {
            return TRI_COL_TYPE_UNKNOWN;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status for one shardID
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e status (ShardID const& shardID) const {
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getNumericValue
                               <TRI_vocbase_col_status_e>
                               (_json, "status", TRI_VOC_COL_STATUS_CORRUPTED);
          }
          return TRI_VOC_COL_STATUS_CORRUPTED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, TRI_vocbase_col_status_e> status () const {
          map<ShardID, TRI_vocbase_col_status_e> m;
          map<ShardID, TRI_json_t*>::const_iterator it;
          TRI_vocbase_col_status_e s;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            s = triagens::basics::JsonHelper::getNumericValue
                          <TRI_vocbase_col_status_e>
                          (_json, "status", TRI_VOC_COL_STATUS_CORRUPTED);
            m.insert(make_pair<ShardID, TRI_vocbase_col_status_e>(it->first,s));
          }
          return m;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a global status of the cluster collection
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e globalStatus () const {
          // FIXME: do it
          return TRI_VOC_COL_STATUS_CORRUPTED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief local helper to return boolean flags
////////////////////////////////////////////////////////////////////////////////

      private:

        bool getFlag (char const* name, ShardID const& shardID) const {
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getBooleanValue(_json, 
                                                                 name, false);
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief local helper to return a map to boolean
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, bool> getFlag (char const* name ) const {
          map<ShardID, bool> m;
          map<ShardID, TRI_json_t*>::const_iterator it;
          bool b;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            b = triagens::basics::JsonHelper::getBooleanValue(_json, 
                                                              name, false);
            m.insert(make_pair<ShardID, bool>(it->first,b));
          }
          return m;
        }

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the deleted flag for a shardID
////////////////////////////////////////////////////////////////////////////////

        bool deleted (ShardID const& shardID) const {
          return getFlag("deleted", shardID);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the deleted flag for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, bool> deleted () const {
          return getFlag("deleted");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the doCompact flag for a shardID
////////////////////////////////////////////////////////////////////////////////

        bool doCompact (ShardID const& shardID) const {
          return getFlag("doCompact", shardID);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the doCompact flag for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, bool> doCompact () const {
          return getFlag("doCompact");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the isSystem flag for a shardID
////////////////////////////////////////////////////////////////////////////////

        bool isSystem (ShardID const& shardID) const {
          return getFlag("isSystem", shardID);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the isSystem flag for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, bool> isSystem () const {
          return getFlag("isSystem");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the isVolatile flag for a shardID
////////////////////////////////////////////////////////////////////////////////

        bool isVolatile (ShardID const& shardID) const {
          return getFlag("isVolatile", shardID);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the isVolatile flag for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, bool> isVolatile () const {
          return getFlag("isVolatile");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the waitForSync flag for a shardID
////////////////////////////////////////////////////////////////////////////////

        bool waitForSync (ShardID const& shardID) const {
          return getFlag("waitForSync", shardID);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the waitForSync flag for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, bool> waitForSync () const {
          return getFlag("waitForSync");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a copy of the key options
/// the caller is responsible for freeing it 
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* keyOptions () const {
          // The id will always be the same in every shard
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            TRI_json_t const* keyOptions 
                 = triagens::basics::JsonHelper::getArrayElement
                                        (_json, "keyOptions");

            if (keyOptions != 0) {
              return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
            }

            return 0;
          }
          else {
            return 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the maximal journal size for one shardID
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_size_t journalSize (ShardID const& shardID) const {
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getNumericValue
                               <TRI_voc_size_t> (_json, "journalSize", 0);
          }
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the maximal journal size for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        map<ShardID, TRI_voc_size_t> journalSize () const {
          map<ShardID, TRI_voc_size_t> m;
          map<ShardID, TRI_json_t*>::const_iterator it;
          TRI_voc_size_t s;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            s = triagens::basics::JsonHelper::getNumericValue
                          <TRI_voc_size_t> (_json, "journalSize", 0);
            m.insert(make_pair<ShardID, TRI_voc_size_t>(it->first,s));
          }
          return m;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys
////////////////////////////////////////////////////////////////////////////////

        vector<string> shardKeys () const {
          // The shardKeys will always be the same in every shard
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            TRI_json_t* const node 
              = triagens::basics::JsonHelper::getArrayElement
                                  (_json, "shardKeys");
            return triagens::basics::JsonHelper::stringList(node);
          }
          else {
            vector<string> result;
            return result;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids that should be in the collection
////////////////////////////////////////////////////////////////////////////////

        map<string, string> shardIdsPlanned () const {
          map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            TRI_json_t* const node 
              = triagens::basics::JsonHelper::getArrayElement(_json, "shards");
            return triagens::basics::JsonHelper::stringObject(node);
          }
          else {
            map<string, string> result;
            return result;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids that are currently in the collection
////////////////////////////////////////////////////////////////////////////////

        vector<ShardID> shardIDs () const {
          vector<ShardID> v;
          map<ShardID, TRI_json_t*>::const_iterator it;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            v.push_back(it->first);
          }
          return v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
      
      private:
     
        map<ShardID, TRI_json_t*> _jsons;
    };

    
// -----------------------------------------------------------------------------
// --SECTION--                                                 class ClusterInfo
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

    class ClusterInfo {
      private:

        typedef std::map<CollectionID, CollectionInfo*>    DatabaseCollections;
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
