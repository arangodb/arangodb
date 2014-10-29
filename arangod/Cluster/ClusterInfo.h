////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterInfo.h
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_CLUSTER_INFO_H
#define ARANGODB_CLUSTER_CLUSTER_INFO_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Cluster/AgencyComm.h"
#include "VocBase/collection.h"
#include "VocBase/index.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

struct TRI_json_t;
struct TRI_memory_zone_s;
struct TRI_server_s;

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

        CollectionInfo (struct TRI_json_t*);

        CollectionInfo (CollectionInfo const&);

        CollectionInfo& operator= (CollectionInfo const&);

        ~CollectionInfo ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether there is no info contained
////////////////////////////////////////////////////////////////////////////////

        bool empty () const {
          return (nullptr == _json); //|| (id() == 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t id () const {
          return triagens::basics::JsonHelper::stringUInt64(_json, "id");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection id as a string
////////////////////////////////////////////////////////////////////////////////

        std::string id_as_string () const {
          return triagens::basics::JsonHelper::getStringValue(_json, "id", "");
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
          return (TRI_col_type_e) triagens::basics::JsonHelper::getNumericValue<int>(_json, "type", (int) TRI_COL_TYPE_UNKNOWN);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e status () const {
          return (TRI_vocbase_col_status_e) triagens::basics::JsonHelper::getNumericValue<int>(_json, "status", (int) TRI_VOC_COL_STATUS_CORRUPTED);
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
/// @brief returns the indexes
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t const* getIndexes () const {
          return triagens::basics::JsonHelper::getArrayElement(_json, "indexes");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a copy of the key options
/// the caller is responsible for freeing it
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* keyOptions () const {
          TRI_json_t const* keyOptions = triagens::basics::JsonHelper::getArrayElement(_json, "keyOptions");

          if (keyOptions != nullptr) {
            return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
          }

          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection allows user-defined keys
////////////////////////////////////////////////////////////////////////////////

        bool allowUserKeys () const {
          TRI_json_t const* keyOptions = triagens::basics::JsonHelper::getArrayElement(_json, "keyOptions");

          if (keyOptions != nullptr) {
            return triagens::basics::JsonHelper::getBooleanValue(keyOptions, "allowUserKeys", true);
          }

          return true; // the default value
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

        TRI_voc_size_t journalSize () const {
          return triagens::basics::JsonHelper::getNumericValue<TRI_voc_size_t>(_json, "journalSize", 0);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of shards
////////////////////////////////////////////////////////////////////////////////

        int numberOfShards () const {
          TRI_json_t* const node = triagens::basics::JsonHelper::getArrayElement(_json, "shards");
          if (TRI_IsArrayJson(node)) {
            return (int) (node->_value._objects._length / 2);
          }
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the json
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t const* getJson () const {
          return _json;
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
// --SECTION--                                       class CollectionInfoCurrent
// -----------------------------------------------------------------------------

    class CollectionInfoCurrent {
      friend class ClusterInfo;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        CollectionInfoCurrent ();

        CollectionInfoCurrent (ShardID const&, struct TRI_json_t*);

        CollectionInfoCurrent (CollectionInfoCurrent const&);

        CollectionInfoCurrent& operator= (CollectionInfoCurrent const&);

        ~CollectionInfoCurrent ();

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
/// The CollectionInfoCurrent object takes ownership of the TRI_json_t*.
////////////////////////////////////////////////////////////////////////////////

        bool add (ShardID const& shardID, TRI_json_t* json) {
          std::map<ShardID, TRI_json_t*>::iterator it = _jsons.find(shardID);
          if (it == _jsons.end()) {
            _jsons.insert(make_pair(shardID, json));
            return true;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_cid_t id () const {
          // The id will always be the same in every shard
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            return triagens::basics::JsonHelper::stringUInt64(_json, "id");
          }
          else {
            return 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection type
////////////////////////////////////////////////////////////////////////////////

        TRI_col_type_e type () const {
          // The type will always be the same in every shard
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            return (TRI_col_type_e) triagens::basics::JsonHelper::getNumericValue<int>
                                      (_json, "type", (int) TRI_COL_TYPE_UNKNOWN);
          }
          else {
            return TRI_COL_TYPE_UNKNOWN;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status for one shardID
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_col_status_e status (ShardID const& shardID) const {
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return (TRI_vocbase_col_status_e) triagens::basics::JsonHelper::getNumericValue
                               <int>
                               (_json, "status", (int) TRI_VOC_COL_STATUS_CORRUPTED);
          }
          return TRI_VOC_COL_STATUS_CORRUPTED;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection status for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        std::map<ShardID, TRI_vocbase_col_status_e> status () const {
          std::map<ShardID, TRI_vocbase_col_status_e> m;
          std::map<ShardID, TRI_json_t*>::const_iterator it;
          TRI_vocbase_col_status_e s;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            s = (TRI_vocbase_col_status_e) triagens::basics::JsonHelper::getNumericValue
                          <int>
                          (_json, "status", (int) TRI_VOC_COL_STATUS_CORRUPTED);
            m.insert(make_pair(it->first,s));
          }
          return m;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief local helper to return boolean flags
////////////////////////////////////////////////////////////////////////////////

      private:

        bool getFlag (char const* name, ShardID const& shardID) const {
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
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

        std::map<ShardID, bool> getFlag (char const* name ) const {
          std::map<ShardID, bool> m;
          std::map<ShardID, TRI_json_t*>::const_iterator it;
          bool b;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            b = triagens::basics::JsonHelper::getBooleanValue(_json,
                                                              name, false);
            m.insert(make_pair(it->first, b));
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

        std::map<ShardID, bool> deleted () const {
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

        std::map<ShardID, bool> doCompact () const {
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

        std::map<ShardID, bool> isSystem () const {
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

        std::map<ShardID, bool> isVolatile () const {
          return getFlag("isVolatile");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the error flag for a shardID
////////////////////////////////////////////////////////////////////////////////

        bool error (ShardID const& shardID) const {
          return getFlag("error", shardID);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the error flag for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        std::map<ShardID, bool> error () const {
          return getFlag("error");
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

        std::map<ShardID, bool> waitForSync () const {
          return getFlag("waitForSync");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a copy of the key options
/// the caller is responsible for freeing it
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* keyOptions () const {
          // The id will always be the same in every shard
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            TRI_json_t const* keyOptions
                 = triagens::basics::JsonHelper::getArrayElement
                                        (_json, "keyOptions");

            if (keyOptions != nullptr) {
              return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
            }
          }

          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the maximal journal size for one shardID
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_size_t journalSize (ShardID const& shardID) const {
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
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

        std::map<ShardID, TRI_voc_size_t> journalSize () const {
          std::map<ShardID, TRI_voc_size_t> m;
          std::map<ShardID, TRI_json_t*>::const_iterator it;
          TRI_voc_size_t s;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            s = triagens::basics::JsonHelper::getNumericValue
                          <TRI_voc_size_t> (_json, "journalSize", 0);
            m.insert(make_pair(it->first,s));
          }
          return m;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the errorNum for one shardID
////////////////////////////////////////////////////////////////////////////////

        int errorNum (ShardID const& shardID) const {
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getNumericValue
                               <int> (_json, "errorNum", 0);
          }
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the errorNum for all shardIDs
////////////////////////////////////////////////////////////////////////////////

        std::map<ShardID, int> errorNum () const {
          std::map<ShardID, int> m;
          std::map<ShardID, TRI_json_t*>::const_iterator it;
          TRI_voc_size_t s;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            TRI_json_t* _json = it->second;
            s = triagens::basics::JsonHelper::getNumericValue
                          <int> (_json, "errorNum", 0);
            m.insert(make_pair(it->first,s));
          }
          return m;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard keys
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> shardKeys () const {
          // The shardKeys will always be the same in every shard
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.begin();
          if (it != _jsons.end()) {
            TRI_json_t* _json = it->second;
            TRI_json_t* const node
              = triagens::basics::JsonHelper::getArrayElement
                                  (_json, "shardKeys");
            return triagens::basics::JsonHelper::stringList(node);
          }
          else {
            std::vector<std::string> result;
            return result;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the shard ids that are currently in the collection
////////////////////////////////////////////////////////////////////////////////

        std::vector<ShardID> shardIDs () const {
          std::vector<ShardID> v;
          std::map<ShardID, TRI_json_t*>::const_iterator it;
          for (it = _jsons.begin(); it != _jsons.end(); ++it) {
            v.push_back(it->first);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the responsible server for one shardID
////////////////////////////////////////////////////////////////////////////////

        std::string responsibleServer (ShardID const& shardID) const {
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getStringValue
                               (_json, "DBServer", "");
          }
          return std::string("");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the errorMessage entry for one shardID
////////////////////////////////////////////////////////////////////////////////

        std::string errorMessage (ShardID const& shardID) const {
          std::map<ShardID, TRI_json_t*>::const_iterator it = _jsons.find(shardID);
          if (it != _jsons.end()) {
            TRI_json_t* _json = _jsons.begin()->second;
            return triagens::basics::JsonHelper::getStringValue
                               (_json, "errorMessage", "");
          }
          return std::string("");
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        std::map<ShardID, TRI_json_t*> _jsons;
    };


// -----------------------------------------------------------------------------
// --SECTION--                                                 class ClusterInfo
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                          typedefs
// -----------------------------------------------------------------------------

    class ClusterInfo {
      private:

        typedef std::map<CollectionID, std::shared_ptr<CollectionInfo> >
                DatabaseCollections;
        typedef std::map<DatabaseID, DatabaseCollections>
                AllCollections;
        typedef std::map<CollectionID, std::shared_ptr<CollectionInfoCurrent> >
                DatabaseCollectionsCurrent;
        typedef std::map<DatabaseID, DatabaseCollectionsCurrent>
                AllCollectionsCurrent;

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
/// @brief initialise function to call once when still single-threaded
////////////////////////////////////////////////////////////////////////////////

        static void initialise ();

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

        static void cleanup ();

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

        std::vector<DatabaseID> listDatabases (bool = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about planned collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

        void loadPlannedCollections (bool = true);

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
/// If it is not found in the cache, the cache is reloaded once. The second
/// argument can be a collection ID or a collection name (both cluster-wide).
////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<CollectionInfo> getCollection (DatabaseID const&,
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

        const std::vector<std::shared_ptr<CollectionInfo>> getCollections
                                                           (DatabaseID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current collections from the agency
/// Usually one does not have to call this directly. Note that this is
/// necessarily complicated, since here we have to consider information
/// about all shards of a collection.
////////////////////////////////////////////////////////////////////////////////

        void loadCurrentCollections (bool = true);

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection in current. This returns information about
/// all shards in the collection.
/// If it is not found in the cache, the cache is reloaded once.
////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<CollectionInfoCurrent> getCollectionCurrent (
                                                    DatabaseID const&,
                                                    CollectionID const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief create database in coordinator
////////////////////////////////////////////////////////////////////////////////

        int createDatabaseCoordinator (std::string const& name,
                                       TRI_json_t const* json,
                                       std::string& errorMsg, double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief drop database in coordinator
////////////////////////////////////////////////////////////////////////////////

        int dropDatabaseCoordinator (std::string const& name, 
                                     std::string& errorMsg,
                                     double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator
////////////////////////////////////////////////////////////////////////////////

        int createCollectionCoordinator (std::string const& databaseName,
                                         std::string const& collectionID,
                                         uint64_t numberOfShards,
                                         TRI_json_t const* json,
                                         std::string& errorMsg, 
                                         double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collection in coordinator
////////////////////////////////////////////////////////////////////////////////

        int dropCollectionCoordinator (std::string const& databaseName,
                                       std::string const& collectionID,
                                       std::string& errorMsg,
                                       double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

        int setCollectionPropertiesCoordinator (std::string const& databaseName,
                                                std::string const& collectionID,
                                                TRI_col_info_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection status in coordinator
////////////////////////////////////////////////////////////////////////////////

        int setCollectionStatusCoordinator (std::string const& databaseName,
                                            std::string const& collectionID,
                                            TRI_vocbase_col_status_e status);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

        int ensureIndexCoordinator (std::string const& databaseName,
                                    std::string const& collectionID,
                                    TRI_json_t const* json,
                                    bool create,
                                    bool (*compare)(TRI_json_t const*, TRI_json_t const*),
                                    TRI_json_t*& resultJson,
                                    std::string& errorMsg,
                                    double timeout);

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

        int dropIndexCoordinator (std::string const& databaseName,
                                  std::string const& collectionID,
                                  TRI_idx_iid_t iid,
                                  std::string& errorMsg,
                                  double timeout);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard that is responsible for a document
////////////////////////////////////////////////////////////////////////////////

        int getResponsibleShard (CollectionID const&, TRI_json_t const*,
                                 bool docComplete,
                                 ShardID& shardID,
                                 bool& usesDefaultShardingAttributes);

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the list of planned databases
////////////////////////////////////////////////////////////////////////////////

        void clearPlannedDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the list of current databases
////////////////////////////////////////////////////////////////////////////////

        void clearCurrentDatabases ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get an operation timeout
////////////////////////////////////////////////////////////////////////////////

        double getTimeout (double timeout) const {
          if (timeout == 0.0) {
            return 24.0 * 3600.0;
          }
          return timeout;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the poll interval
////////////////////////////////////////////////////////////////////////////////

        double getPollInterval () const {
          return 5.0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the timeout for reloading the server list
////////////////////////////////////////////////////////////////////////////////

        double getReloadServerListTimeout () const {
          return 60.0;
        }

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
        std::map<DatabaseID, struct TRI_json_t*> _plannedDatabases;
              // from Plan/Databases
        std::map<DatabaseID, std::map<ServerID, struct TRI_json_t*> >
              _currentDatabases;        // from Current/Databases

        AllCollections                  _collections;
                                        // from Plan/Collections/
        bool                            _collectionsValid;
        AllCollectionsCurrent           _collectionsCurrent;
                                        // from Current/Collections/
        bool                            _collectionsCurrentValid;
        std::map<ServerID, std::string> _servers;
                                        // from Current/ServersRegistered
        bool                            _serversValid;
        std::map<ServerID, ServerID>    _DBServers;
                                        // from Current/DBServers
        bool                            _DBServersValid;
        std::map<ShardID, ServerID>     _shardIds;
                                        // from Current/Collections/
        std::map<CollectionID, std::shared_ptr<std::vector<std::string>>>
                                        _shards;
                                        // from Plan/Collections/
                               // (may later come from Current/Colletions/ )
        std::map<CollectionID, std::shared_ptr<std::vector<std::string>>>
                                        _shardKeys;
                                        // from Plan/Collections/

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

        static const uint64_t MinIdsPerBatch = 1000000;

////////////////////////////////////////////////////////////////////////////////
/// @brief default wait timeout
////////////////////////////////////////////////////////////////////////////////

        static const double operationTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief reload timeout
////////////////////////////////////////////////////////////////////////////////

        static const double reloadServerListTimeout;

    };

  }  // end namespace arango
}  // end namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
