////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_INFO_H
#define ARANGOD_CLUSTER_CLUSTER_INFO_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>

struct TRI_json_t;

namespace arangodb {
namespace velocypack {
class Slice;
}
class ClusterInfo;

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard

class CollectionInfo {
  friend class ClusterInfo;

 public:
  CollectionInfo();

  explicit CollectionInfo(struct TRI_json_t*);

  CollectionInfo(CollectionInfo const&);

  CollectionInfo(CollectionInfo&&);

  CollectionInfo& operator=(CollectionInfo const&);

  CollectionInfo& operator=(CollectionInfo&&);

  ~CollectionInfo();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the replication factor
  //////////////////////////////////////////////////////////////////////////////

  int replicationFactor () const {
    TRI_json_t* const node 
        = arangodb::basics::JsonHelper::getObjectElement(_json,
                                                         "replicationFactor");

    if (TRI_IsNumberJson(node)) {
      return (int) (node->_value._number);
    }
    return 1;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the replication quorum
  //////////////////////////////////////////////////////////////////////////////

  int replicationQuorum () const {
    TRI_json_t* const node 
        = arangodb::basics::JsonHelper::getObjectElement(_json,
                                                         "replicationQuorum");

    if (TRI_IsNumberJson(node)) {
      return (int) (node->_value._number);
    }
    return 1;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether there is no info contained
  //////////////////////////////////////////////////////////////////////////////

  bool empty() const {
    return (nullptr == _json);  //|| (id() == 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the collection id
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_cid_t id() const {
    return arangodb::basics::JsonHelper::stringUInt64(_json, "id");
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the collection id as a string
  //////////////////////////////////////////////////////////////////////////////

  std::string id_as_string() const {
    return arangodb::basics::JsonHelper::getStringValue(_json, "id", "");
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the collection name
  //////////////////////////////////////////////////////////////////////////////

  std::string name() const {
    return arangodb::basics::JsonHelper::getStringValue(_json, "name", "");
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the collection type
  //////////////////////////////////////////////////////////////////////////////

  TRI_col_type_e type() const {
    return (TRI_col_type_e)arangodb::basics::JsonHelper::getNumericValue<int>(
        _json, "type", (int)TRI_COL_TYPE_UNKNOWN);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the collection status
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_col_status_e status() const {
    return (TRI_vocbase_col_status_e)
        arangodb::basics::JsonHelper::getNumericValue<int>(
            _json, "status", (int)TRI_VOC_COL_STATUS_CORRUPTED);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the collection status as a string
  //////////////////////////////////////////////////////////////////////////////

  std::string statusString() const {
    return TRI_GetStatusStringCollectionVocBase(status());
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the deleted flag
  //////////////////////////////////////////////////////////////////////////////

  bool deleted() const {
    return arangodb::basics::JsonHelper::getBooleanValue(_json, "deleted",
                                                         false);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the docompact flag
  //////////////////////////////////////////////////////////////////////////////

  bool doCompact() const {
    return arangodb::basics::JsonHelper::getBooleanValue(_json, "doCompact",
                                                         false);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the issystem flag
  //////////////////////////////////////////////////////////////////////////////

  bool isSystem() const {
    return arangodb::basics::JsonHelper::getBooleanValue(_json, "isSystem",
                                                         false);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the isvolatile flag
  //////////////////////////////////////////////////////////////////////////////

  bool isVolatile() const {
    return arangodb::basics::JsonHelper::getBooleanValue(_json, "isVolatile",
                                                         false);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the indexes
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t const* getIndexes() const {
    return arangodb::basics::JsonHelper::getObjectElement(_json, "indexes");
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a copy of the key options
  /// the caller is responsible for freeing it
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t* keyOptions() const {
    TRI_json_t const* keyOptions =
        arangodb::basics::JsonHelper::getObjectElement(_json, "keyOptions");

    if (keyOptions != nullptr) {
      return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
    }

    return nullptr;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not a collection allows user-defined keys
  //////////////////////////////////////////////////////////////////////////////

  bool allowUserKeys() const {
    TRI_json_t const* keyOptions =
        arangodb::basics::JsonHelper::getObjectElement(_json, "keyOptions");

    if (keyOptions != nullptr) {
      return arangodb::basics::JsonHelper::getBooleanValue(
          keyOptions, "allowUserKeys", true);
    }

    return true;  // the default value
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the waitforsync flag
  //////////////////////////////////////////////////////////////////////////////

  bool waitForSync() const {
    return arangodb::basics::JsonHelper::getBooleanValue(_json, "waitForSync",
                                                         false);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the maximal journal size
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_size_t journalSize() const {
    return arangodb::basics::JsonHelper::getNumericValue<TRI_voc_size_t>(
        _json, "journalSize", 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the number of buckets for indexes
  //////////////////////////////////////////////////////////////////////////////

  uint32_t indexBuckets() const {
    return arangodb::basics::JsonHelper::getNumericValue<uint32_t>(
        _json, "indexBuckets", 1);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the shard keys
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> shardKeys() const {
    TRI_json_t* const node =
        arangodb::basics::JsonHelper::getObjectElement(_json, "shardKeys");
    return arangodb::basics::JsonHelper::stringArray(node);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns true if the default shard key is used
  //////////////////////////////////////////////////////////////////////////////

  bool usesDefaultShardKeys() const {
    TRI_json_t* const node =
        arangodb::basics::JsonHelper::getObjectElement(_json, "shardKeys");
    if (TRI_LengthArrayJson(node) != 1) {
      return false;
    }
    TRI_json_t* firstKey = TRI_LookupArrayJson(node, 0);
    TRI_ASSERT(TRI_IsStringJson(firstKey));
    std::string shardKey =
        arangodb::basics::JsonHelper::getStringValue(firstKey, "");
    return shardKey == TRI_VOC_ATTRIBUTE_KEY;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the shard ids
  //////////////////////////////////////////////////////////////////////////////

  typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

  std::shared_ptr<ShardMap> shardIds() const {
    std::shared_ptr<ShardMap> res;
    {
      MUTEX_LOCKER(locker, _mutex);
      res = _shardMapCache;
    }
    if (res.get() != nullptr) {
      return res;
    }
    res.reset(new ShardMap());
    TRI_json_t* const node =
        arangodb::basics::JsonHelper::getObjectElement(_json, "shards");
    if (node != nullptr && TRI_IsObjectJson(node)) {
      size_t len = TRI_LengthVector(&node->_value._objects);
      for (size_t i = 0; i < len; i += 2) {
        auto key =
            static_cast<TRI_json_t*>(TRI_AtVector(&node->_value._objects, i));
        auto value = static_cast<TRI_json_t*>(
            TRI_AtVector(&node->_value._objects, i + 1));
        if (TRI_IsStringJson(key) && TRI_IsArrayJson(value)) {
          ShardID shard = arangodb::basics::JsonHelper::getStringValue(key, "");
          std::vector<ServerID> servers =
              arangodb::basics::JsonHelper::stringArray(value);
          if (shard != "") {
            (*res).insert(make_pair(shard, servers));
          }
        }
      }
    }
    {
      MUTEX_LOCKER(locker, _mutex);
      _shardMapCache = res;
    }
    return res;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the number of shards
  //////////////////////////////////////////////////////////////////////////////

  int numberOfShards() const {
    TRI_json_t* const node =
        arangodb::basics::JsonHelper::getObjectElement(_json, "shards");

    if (TRI_IsObjectJson(node)) {
      return (int)(TRI_LengthVector(&node->_value._objects) / 2);
    }
    return 0;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the json
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t const* getJson() const { return _json; }

 private:
  TRI_json_t* _json;

  // Only to protect the cache:
  mutable Mutex _mutex;

  // Just a cache
  mutable std::shared_ptr<ShardMap> _shardMapCache;
};

class CollectionInfoCurrent {
  friend class ClusterInfo;

 public:
  CollectionInfoCurrent();

  CollectionInfoCurrent(ShardID const&, struct TRI_json_t*);

  CollectionInfoCurrent(CollectionInfoCurrent const&);

  CollectionInfoCurrent(CollectionInfoCurrent&&);

  CollectionInfoCurrent& operator=(CollectionInfoCurrent const&);

  CollectionInfoCurrent& operator=(CollectionInfoCurrent&&);

  ~CollectionInfoCurrent();

 private:
  void freeAllJsons();

  void copyAllJsons();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a new shardID and JSON pair, returns true if OK and false
  /// if the shardID already exists. In the latter case nothing happens.
  /// The CollectionInfoCurrent object takes ownership of the TRI_json_t*.
  //////////////////////////////////////////////////////////////////////////////

 public:
  bool add(ShardID const& shardID, TRI_json_t* json) {
    auto it = _jsons.find(shardID);
    if (it == _jsons.end()) {
      _jsons.insert(std::make_pair(shardID, json));
      return true;
    }
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the indexes
  //////////////////////////////////////////////////////////////////////////////

  TRI_json_t const* getIndexes(ShardID const& shardID) const {
    auto it = _jsons.find(shardID);
    if (it != _jsons.end()) {
      TRI_json_t* json = it->second;
      return arangodb::basics::JsonHelper::getObjectElement(json, "indexes");
    }
    return nullptr;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the error flag for a shardID
  //////////////////////////////////////////////////////////////////////////////

  bool error(ShardID const& shardID) const { return getFlag("error", shardID); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the error flag for all shardIDs
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<ShardID, bool> error() const { return getFlag("error"); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the errorNum for one shardID
  //////////////////////////////////////////////////////////////////////////////

  int errorNum(ShardID const& shardID) const {
    auto it = _jsons.find(shardID);
    if (it != _jsons.end()) {
      TRI_json_t* json = it->second;
      return arangodb::basics::JsonHelper::getNumericValue<int>(json,
                                                                "errorNum", 0);
    }
    return 0;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the errorNum for all shardIDs
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<ShardID, int> errorNum() const {
    std::unordered_map<ShardID, int> m;
    TRI_voc_size_t s;
    for (auto it = _jsons.begin(); it != _jsons.end(); ++it) {
      TRI_json_t* json = it->second;
      s = arangodb::basics::JsonHelper::getNumericValue<int>(json, "errorNum",
                                                             0);
      m.insert(std::make_pair(it->first, s));
    }
    return m;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the current leader and followers for a shard
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> servers(ShardID const& shardID) const {
    std::vector<ServerID> v;
    auto it = _jsons.find(shardID);
    if (it != _jsons.end()) {
      TRI_json_t const* json =
          arangodb::basics::JsonHelper::getObjectElement(it->second, "servers");
      if (json != nullptr) {
        v = arangodb::basics::JsonHelper::stringArray(json);
      }
    }
    return v;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the errorMessage entry for one shardID
  //////////////////////////////////////////////////////////////////////////////

  std::string errorMessage(ShardID const& shardID) const {
    auto it = _jsons.find(shardID);
    if (it != _jsons.end()) {
      TRI_json_t* json = it->second;
      return arangodb::basics::JsonHelper::getStringValue(json, "errorMessage",
                                                          "");
    }
    return std::string();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief local helper to return boolean flags
  //////////////////////////////////////////////////////////////////////////////

 private:
  bool getFlag(char const* name, ShardID const& shardID) const {
    auto it = _jsons.find(shardID);
    if (it != _jsons.end()) {
      TRI_json_t* json = it->second;
      return arangodb::basics::JsonHelper::getBooleanValue(json, name, false);
    }
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief local helper to return a map to boolean
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<ShardID, bool> getFlag(char const* name) const {
    std::unordered_map<ShardID, bool> m;
    for (auto it = _jsons.begin(); it != _jsons.end(); ++it) {
      TRI_json_t* json = it->second;
      bool b = arangodb::basics::JsonHelper::getBooleanValue(json, name, false);
      m.insert(std::make_pair(it->first, b));
    }
    return m;
  }

 private:
  std::unordered_map<ShardID, TRI_json_t*> _jsons;
};

class ClusterInfo {
 private:
  typedef std::unordered_map<CollectionID, std::shared_ptr<CollectionInfo>>
      DatabaseCollections;
  typedef std::unordered_map<DatabaseID, DatabaseCollections> AllCollections;
  typedef std::unordered_map<CollectionID,
                             std::shared_ptr<CollectionInfoCurrent>>
      DatabaseCollectionsCurrent;
  typedef std::unordered_map<DatabaseID, DatabaseCollectionsCurrent>
      AllCollectionsCurrent;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief initializes library
  /// We are a singleton class, therefore nobody is allowed to create
  /// new instances or copy them, except we ourselves.
  //////////////////////////////////////////////////////////////////////////////

  ClusterInfo(ClusterInfo const&) = delete;             // not implemented
  ClusterInfo& operator=(ClusterInfo const&) = delete;  // not implemented

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates library
  //////////////////////////////////////////////////////////////////////////////

  explicit ClusterInfo(AgencyCallbackRegistry*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shuts down library
  //////////////////////////////////////////////////////////////////////////////

  ~ClusterInfo();

 public:
  static void createInstance(AgencyCallbackRegistry*);
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the unique instance
  //////////////////////////////////////////////////////////////////////////////

  static ClusterInfo* instance();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a number of cluster-wide unique IDs, returns the first
  /// one and guarantees that <number> are reserved for the caller.
  //////////////////////////////////////////////////////////////////////////////

  uint64_t uniqid(uint64_t = 1);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flush the caches (used for testing only)
  //////////////////////////////////////////////////////////////////////////////

  void flush();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask whether a cluster database exists
  //////////////////////////////////////////////////////////////////////////////

  bool doesDatabaseExist(DatabaseID const&, bool = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get list of databases in the cluster
  //////////////////////////////////////////////////////////////////////////////

  std::vector<DatabaseID> listDatabases(bool = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about planned collections from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadPlannedCollections();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about planned databases
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadPlannedDatabases();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about current databases
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentDatabases();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection
  /// If it is not found in the cache, the cache is reloaded once. The second
  /// argument can be a collection ID or a collection name (both cluster-wide).
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<CollectionInfo> getCollection(DatabaseID const&,
                                                CollectionID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get properties of a collection
  //////////////////////////////////////////////////////////////////////////////

  VocbaseCollectionInfo getCollectionProperties(CollectionInfo const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get properties of a collection
  //////////////////////////////////////////////////////////////////////////////

  VocbaseCollectionInfo getCollectionProperties(DatabaseID const&,
                                                CollectionID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about all collections
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::shared_ptr<CollectionInfo>> const getCollections(
      DatabaseID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about current collections from the agency
  /// Usually one does not have to call this directly. Note that this is
  /// necessarily complicated, since here we have to consider information
  /// about all shards of a collection.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentCollections();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection in current. This returns information about
  /// all shards in the collection.
  /// If it is not found in the cache, the cache is reloaded once.
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<CollectionInfoCurrent> getCollectionCurrent(
      DatabaseID const&, CollectionID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create database in coordinator
  //////////////////////////////////////////////////////////////////////////////

  int createDatabaseCoordinator(std::string const&,
                                arangodb::velocypack::Slice const&,
                                std::string&, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop database in coordinator
  //////////////////////////////////////////////////////////////////////////////

  int dropDatabaseCoordinator(std::string const& name, std::string& errorMsg,
                              double timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create collection in coordinator
  //////////////////////////////////////////////////////////////////////////////

  int createCollectionCoordinator(std::string const& databaseName,
                                  std::string const& collectionID,
                                  uint64_t numberOfShards,
                                  arangodb::velocypack::Slice const& json,
                                  std::string& errorMsg, double timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop collection in coordinator
  //////////////////////////////////////////////////////////////////////////////

  int dropCollectionCoordinator(std::string const& databaseName,
                                std::string const& collectionID,
                                std::string& errorMsg, double timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collection properties in coordinator
  //////////////////////////////////////////////////////////////////////////////

  int setCollectionPropertiesCoordinator(std::string const& databaseName,
                                         std::string const& collectionID,
                                         VocbaseCollectionInfo const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collection status in coordinator
  //////////////////////////////////////////////////////////////////////////////

  int setCollectionStatusCoordinator(std::string const& databaseName,
                                     std::string const& collectionID,
                                     TRI_vocbase_col_status_e status);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////

  int ensureIndexCoordinator(
      std::string const& databaseName, std::string const& collectionID,
      arangodb::velocypack::Slice const& slice, bool create,
      bool (*compare)(arangodb::velocypack::Slice const&,
                      arangodb::velocypack::Slice const&),
      arangodb::velocypack::Builder& resultBuilder, std::string& errorMsg, double timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////

  int ensureIndexCoordinator(
      std::string const& databaseName, std::string const& collectionID,
      TRI_json_t const* json, bool create,
      bool (*compare)(TRI_json_t const*, TRI_json_t const*),
      TRI_json_t*& resultJson, std::string& errorMsg, double timeout) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////

  int dropIndexCoordinator(std::string const& databaseName,
                           std::string const& collectionID, TRI_idx_iid_t iid,
                           std::string& errorMsg, double timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about servers from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadServers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the endpoint of a server from its ID.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  std::string getServerEndpoint(ServerID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the server ID for an endpoint.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  std::string getServerName(std::string const& endpoint);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about all coordinators from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about all DBservers from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrentDBServers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a list of all DBServers in the cluster that have
  /// currently registered
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> getCurrentDBServers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookup the server's endpoint by scanning Target/MapIDToEnpdoint for
  /// our id
  //////////////////////////////////////////////////////////////////////////////

  std::string getTargetServerEndpoint(ServerID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the servers who are responsible for a shard (one leader
  /// and possibly multiple followers).
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there a pointer to an empty vector is returned as
  /// an error.
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ServerID>> getResponsibleServer(ShardID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the shard list of a collection, sorted numerically
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ShardID>> getShardList(CollectionID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the shard that is responsible for a document
  //////////////////////////////////////////////////////////////////////////////

  int getResponsibleShard(CollectionID const&, arangodb::velocypack::Slice,
                          bool docComplete, ShardID& shardID,
                          bool& usesDefaultShardingAttributes);


  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the shard that is responsible for a document
  //////////////////////////////////////////////////////////////////////////////

  int getResponsibleShard(CollectionID const&, TRI_json_t const*,
                          bool docComplete, ShardID& shardID,
                          bool& usesDefaultShardingAttributes);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the list of coordinator server names
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> getCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate current
  //////////////////////////////////////////////////////////////////////////////
  
  void invalidateCurrent();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief actually clears a list of planned databases
  //////////////////////////////////////////////////////////////////////////////

  void clearPlannedDatabases(
      std::unordered_map<DatabaseID, TRI_json_t*>& databases);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief actually clears a list of current databases
  //////////////////////////////////////////////////////////////////////////////

  void clearCurrentDatabases(std::unordered_map<
      DatabaseID, std::unordered_map<ServerID, TRI_json_t*>>& databases);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get an operation timeout
  //////////////////////////////////////////////////////////////////////////////

  double getTimeout(double timeout) const {
    if (timeout == 0.0) {
      return 24.0 * 3600.0;
    }
    return timeout;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the poll interval
  //////////////////////////////////////////////////////////////////////////////

  double getPollInterval() const { return 5.0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the timeout for reloading the server list
  //////////////////////////////////////////////////////////////////////////////

  double getReloadServerListTimeout() const { return 60.0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief object for agency communication
  //////////////////////////////////////////////////////////////////////////////

  AgencyComm _agency;

  AgencyCallbackRegistry* _agencyCallbackRegistry;

  // Cached data from the agency, we reload whenever necessary:

  // We group the data, each group has an atomic "valid-flag"
  // which is used for lazy loading in the beginning. It starts
  // as false, is set to true at each reload and is never reset
  // to false in the lifetime of the server. The variable is
  // atomic to be able to check it without acquiring
  // the read lock (see below). Flush is just an explicit reload
  // for all data and is only used in tests.
  // Furthermore, each group has a mutex that protects against
  // simultaneously contacting the agency for an update.
  // In addition, each group has an atomic version number, this is used
  // to prevent a stampede if multiple threads notice concurrently
  // that an update from the agency is necessary. Finally, there is
  // a read/write lock which protects the actual data structure.
  // We encapsulate this protection in the struct ProtectionData:

  struct ProtectionData {
    std::atomic<bool> isValid;
    Mutex mutex;
    std::atomic<uint64_t> version;
    arangodb::basics::ReadWriteLock lock;

    ProtectionData() : isValid(false), version(0) {}
  };

  // The servers, first all, we only need Current here:
  std::unordered_map<ServerID, std::string>
      _servers;  // from Current/ServersRegistered
  ProtectionData _serversProt;

  // The DBServers, also from Current:
  std::unordered_map<ServerID, ServerID> _DBServers;  // from Current/DBServers
  ProtectionData _DBServersProt;

  // The Coordinators, also from Current:
  std::unordered_map<ServerID, ServerID>
      _coordinators;  // from Current/Coordinators
  ProtectionData _coordinatorsProt;

  // First the databases, there is Plan and Current information:
  std::unordered_map<DatabaseID, struct TRI_json_t*>
      _plannedDatabases;  // from Plan/Databases
  ProtectionData _plannedDatabasesProt;

  std::unordered_map<DatabaseID,
                     std::unordered_map<ServerID, struct TRI_json_t*>>
      _currentDatabases;  // from Current/Databases
  ProtectionData _currentDatabasesProt;

  // We need information about collections, again we have
  // data from Plan and from Current.
  // The information for _shards and _shardKeys are filled from the
  // Plan (since they are fixed for the lifetime of the collection).
  // _shardIds is filled from Current, since we have to be able to
  // move shards between servers, and Plan contains who ought to be
  // responsible and Current contains the actual current responsibility.

  // The Plan state:
  AllCollections _plannedCollections;  // from Plan/Collections/
  ProtectionData _plannedCollectionsProt;
  std::unordered_map<CollectionID,
                     std::shared_ptr<std::vector<std::string>>>
      _shards;  // from Plan/Collections/
                // (may later come from Current/Collections/ )
  std::unordered_map<CollectionID,
                     std::shared_ptr<std::vector<std::string>>>
      _shardKeys;  // from Plan/Collections/

  // The Current state:
  AllCollectionsCurrent _currentCollections;  // from Current/Collections/
  ProtectionData _currentCollectionsProt;
  std::unordered_map<ShardID, std::shared_ptr<std::vector<ServerID>>>
      _shardIds;  // from Current/Collections/

  //////////////////////////////////////////////////////////////////////////////
  /// @brief uniqid sequence
  //////////////////////////////////////////////////////////////////////////////

  struct {
    uint64_t _currentValue;
    uint64_t _upperValue;
  } _uniqid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock for uniqid sequence
  //////////////////////////////////////////////////////////////////////////////

  Mutex _idLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the sole instance
  //////////////////////////////////////////////////////////////////////////////

  static ClusterInfo* _theinstance;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief how big a batch is for unique ids
  //////////////////////////////////////////////////////////////////////////////

  static uint64_t const MinIdsPerBatch = 1000000;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief default wait timeout
  //////////////////////////////////////////////////////////////////////////////

  static double const operationTimeout;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reload timeout
  //////////////////////////////////////////////////////////////////////////////

  static double const reloadServerListTimeout;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to track followers that are in sync for a shard
////////////////////////////////////////////////////////////////////////////////

class FollowerInfo {
  std::shared_ptr<std::vector<ServerID> const> _followers;
  Mutex                                        _mutex;
  TRI_document_collection_t*                   _docColl;

 public:

  explicit FollowerInfo(TRI_document_collection_t* d) 
    : _followers(new std::vector<ServerID>()), _docColl(d) { }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get information about current followers of a shard.
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ServerID> const> get();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a follower to a shard, this is only done by the server side
  /// of the "get-in-sync" capabilities. This reports to the agency under
  /// `/Current` but in asynchronous "fire-and-forget" way. The method
  /// fails silently, if the follower information has since been dropped
  /// (see `dropFollowerInfo` below).
  //////////////////////////////////////////////////////////////////////////////

  void add(ServerID const& s);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove a follower from a shard, this is only done by the
  /// server if a synchronous replication request fails. This reports to
  /// the agency under `/Current` but in an asynchronous "fire-and-forget"
  /// way.
  //////////////////////////////////////////////////////////////////////////////

  void remove(ServerID const& s);

};

}  // end namespace arangodb

#endif
