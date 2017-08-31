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

#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Cluster/AgencyCallbackRegistry.h"
#include "Agency/AgencyComm.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace velocypack {
class Slice;
}
class ClusterInfo;
class LogicalCollection;

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard

class CollectionInfoCurrent {
  friend class ClusterInfo;

 public:
  CollectionInfoCurrent(uint64_t currentVersion);

  CollectionInfoCurrent(CollectionInfoCurrent const&) = delete;

  CollectionInfoCurrent(CollectionInfoCurrent&&) = delete;

  CollectionInfoCurrent& operator=(CollectionInfoCurrent const&) = delete;

  CollectionInfoCurrent& operator=(CollectionInfoCurrent&&) = delete;

  ~CollectionInfoCurrent();

 public:
  bool add(ShardID const& shardID, VPackSlice slice) {
    auto it = _vpacks.find(shardID);
    if (it == _vpacks.end()) {
      auto builder = std::make_shared<VPackBuilder>();
      builder->add(slice);
      _vpacks.insert(std::make_pair(shardID, builder));
      return true;
    }
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the indexes
  //////////////////////////////////////////////////////////////////////////////

  VPackSlice const getIndexes(ShardID const& shardID) const {
    auto it = _vpacks.find(shardID);
    if (it != _vpacks.end()) {
      VPackSlice slice = it->second->slice();
      return slice.get("indexes");
    }
    return VPackSlice::noneSlice();
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
    auto it = _vpacks.find(shardID);
    if (it != _vpacks.end()) {
      VPackSlice slice = it->second->slice();
      return arangodb::basics::VelocyPackHelper::getNumericValue<int>(slice,
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

    for (auto const& it: _vpacks) {
      s = arangodb::basics::VelocyPackHelper::getNumericValue<int>(it.second->slice(), "errorNum", 0);
      m.insert(std::make_pair(it.first, s));
    }
    return m;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the current leader and followers for a shard
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> servers(ShardID const& shardID) const {
    std::vector<ServerID> v;

    auto it = _vpacks.find(shardID);
    if (it != _vpacks.end()) {
      VPackSlice slice = it->second->slice();

      VPackSlice servers = slice.get("servers");
      if (servers.isArray()) {
        for (auto const& server: VPackArrayIterator(servers)) {
          if (server.isString()) {
            v.push_back(server.copyString());
          }
        }
      }
    }
    return v;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the errorMessage entry for one shardID
  //////////////////////////////////////////////////////////////////////////////

  std::string errorMessage(ShardID const& shardID) const {
    auto it = _vpacks.find(shardID);
    if (it != _vpacks.end()) {
      VPackSlice slice = it->second->slice();
      if (slice.isObject() && slice.hasKey("errorMessage")) {
        return slice.get("errorMessage").copyString();
      }
    }
    return std::string();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get version that underlies this info in Current in the agency
  //////////////////////////////////////////////////////////////////////////////

  uint64_t getCurrentVersion() const {
    return _currentVersion;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief local helper to return boolean flags
  //////////////////////////////////////////////////////////////////////////////

 private:
  bool getFlag(char const* name, ShardID const& shardID) const {
    auto it = _vpacks.find(shardID);
    if (it != _vpacks.end()) {
      return arangodb::basics::VelocyPackHelper::getBooleanValue(it->second->slice(), name, false);
    }
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief local helper to return a map to boolean
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<ShardID, bool> getFlag(char const* name) const {
    std::unordered_map<ShardID, bool> m;
    for (auto const& it: _vpacks) {
      auto vpack = it.second;
      bool b = arangodb::basics::VelocyPackHelper::getBooleanValue(vpack->slice(), name, false);
      m.insert(std::make_pair(it.first, b));
    }
    return m;
  }

 private:
  std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> _vpacks;

  uint64_t _currentVersion;    // Version of Current in the agency that
                               // underpins the data presented in this object
};

class ClusterInfo {
 private:

  typedef std::unordered_map<CollectionID, std::shared_ptr<LogicalCollection>>
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
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief cleanup method which frees cluster-internal shared ptrs on shutdown
  //////////////////////////////////////////////////////////////////////////////

  static void cleanup();

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

  std::vector<DatabaseID> databases(bool = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about our plan
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadPlan();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about current state
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  void loadCurrent();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection
  /// If it is not found in the cache, the cache is reloaded once. The second
  /// argument can be a collection ID or a collection name (both cluster-wide).
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<LogicalCollection> getCollection(DatabaseID const&,
                                                   CollectionID const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about all collections
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::shared_ptr<LogicalCollection>> const getCollections(
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
                                  uint64_t replicationFactor,
                                  bool waitForReplication,
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
                                         LogicalCollection const*);

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

  int getResponsibleShard(LogicalCollection*, arangodb::velocypack::Slice,
                          bool docComplete, ShardID& shardID,
                          bool& usesDefaultShardingAttributes,
                          std::string const& key = "");


  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the list of coordinator server names
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ServerID> getCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate planned
  //////////////////////////////////////////////////////////////////////////////

  void invalidatePlan();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate current
  //////////////////////////////////////////////////////////////////////////////

  void invalidateCurrent();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate current coordinators
  //////////////////////////////////////////////////////////////////////////////

  void invalidateCurrentCoordinators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current "Plan" structure
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<VPackBuilder> getPlan();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current "Current" structure
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<VPackBuilder> getCurrent();

  std::vector<std::string> getFailedServers() { MUTEX_LOCKER(guard, _failedServersMutex); return _failedServers; }
  void setFailedServers(std::vector<std::string> const& failedServers) { MUTEX_LOCKER(guard, _failedServersMutex); _failedServers = failedServers; }

  std::unordered_map<ServerID, std::string> getServerAliases();
  
 private:

  void loadClusterId();

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
  /// @brief ensure an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////

  int ensureIndexCoordinatorWithoutRollback(
      std::string const& databaseName, std::string const& collectionID,
      std::string const& idSlice, arangodb::velocypack::Slice const& slice, bool create,
      bool (*compare)(arangodb::velocypack::Slice const&,
                      arangodb::velocypack::Slice const&),
      arangodb::velocypack::Builder& resultBuilder, std::string& errorMsg, double timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief object for agency communication
  //////////////////////////////////////////////////////////////////////////////

  AgencyComm _agency;

  AgencyCallbackRegistry* _agencyCallbackRegistry;

  // Cached data from the agency, we reload whenever necessary:

  // We group the data, each group has an atomic "valid-flag" which is
  // used for lazy loading in the beginning. It starts as false, is set
  // to true at each reload and is only reset to false if the cache
  // needs to be invalidated. The variable is atomic to be able to check
  // it without acquiring the read lock (see below). Flush is just an
  // explicit reload for all data and is only used in tests.
  // Furthermore, each group has a mutex that protects against
  // simultaneously contacting the agency for an update.
  // In addition, each group has two atomic version numbers, these are
  // used to prevent a stampede if multiple threads notice concurrently
  // that an update from the agency is necessary. Finally, there is a
  // read/write lock which protects the actual data structure.
  // We encapsulate this protection in the struct ProtectionData:

  struct ProtectionData {
    std::atomic<bool> isValid;
    Mutex mutex;
    std::atomic<uint64_t> wantedVersion;
    std::atomic<uint64_t> doneVersion;
    arangodb::basics::ReadWriteLock lock;

    ProtectionData() : isValid(false), wantedVersion(0), doneVersion(0) {}
  };

  // The servers, first all, we only need Current here:
  std::unordered_map<ServerID, std::string>
      _servers;  // from Current/ServersRegistered
  std::unordered_map<ServerID, std::string>
      _serverAliases;  // from Current/ServersRegistered
  ProtectionData _serversProt;

  // The DBServers, also from Current:
  std::unordered_map<ServerID, ServerID> _DBServers;  // from Current/DBServers
  ProtectionData _DBServersProt;

  // The Coordinators, also from Current:
  std::unordered_map<ServerID, ServerID>
      _coordinators;  // from Current/Coordinators
  ProtectionData _coordinatorsProt;

  std::shared_ptr<VPackBuilder> _plan;
  std::shared_ptr<VPackBuilder> _current;

  std::string _clusterId;

  std::unordered_map<DatabaseID, VPackSlice> _plannedDatabases;  // from Plan/Databases

  ProtectionData _planProt;

  uint64_t _planVersion;   // This is the version in the Plan which underlies
                           // the data in _plannedCollections, _shards and
                           // _shardKeys
  uint64_t _currentVersion;  // This is the version in Current which underlies
                             // the data in _currentDatabases,
                             // _currentCollections and _shardsIds
  std::unordered_map<DatabaseID,
                     std::unordered_map<ServerID, VPackSlice>>
      _currentDatabases;  // from Current/Databases
  ProtectionData _currentProt;

  // We need information about collections, again we have
  // data from Plan and from Current.
  // The information for _shards and _shardKeys are filled from the
  // Plan (since they are fixed for the lifetime of the collection).
  // _shardIds is filled from Current, since we have to be able to
  // move shards between servers, and Plan contains who ought to be
  // responsible and Current contains the actual current responsibility.

  // The Plan state:
  AllCollections _plannedCollections;  // from Plan/Collections/
  std::unordered_map<CollectionID,
                     std::shared_ptr<std::vector<std::string>>>
      _shards;  // from Plan/Collections/
                // (may later come from Current/Collections/ )
  std::unordered_map<CollectionID,
                     std::shared_ptr<std::vector<std::string>>>
      _shardKeys;  // from Plan/Collections/
  // planned shard => servers map
  std::unordered_map<ShardID, std::vector<ServerID>> _shardServers;

  // The Current state:
  AllCollectionsCurrent _currentCollections;  // from Current/Collections/
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
  
  arangodb::Mutex _failedServersMutex;  
  std::vector<std::string> _failedServers;
};

}  // end namespace arangodb

#endif
