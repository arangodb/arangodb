////////////////////////////////////////////////////////////////////////////////
/// @brief Interface for ClusterInfo
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_INFO_INTERFACE_H
#define ARANGOD_CLUSTER_CLUSTER_INFO_INTERFACE_H 1

#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace velocypack {
class Slice;
}
class ClusterInfo;

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard

class CollectionInfoCurrent {
  friend class ClusterInfo;

 public:
  CollectionInfoCurrent();

  CollectionInfoCurrent(ShardID const&, VPackSlice);

  CollectionInfoCurrent(CollectionInfoCurrent const&);

  CollectionInfoCurrent(CollectionInfoCurrent&&);

  CollectionInfoCurrent& operator=(CollectionInfoCurrent const&);

  CollectionInfoCurrent& operator=(CollectionInfoCurrent&&);

  ~CollectionInfoCurrent();

 private:
  void copyAllVPacks();

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
      s = arangodb::basics::VelocyPackHelper::getNumericValue<int>(it.second->slice(), "errorNum",
                                                             0);
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
};

class ClusterInfoInterface {
 protected:

  typedef std::unordered_map<CollectionID, std::shared_ptr<LogicalCollection>>
      DatabaseCollections;
  typedef std::unordered_map<DatabaseID, DatabaseCollections> AllCollections;
  typedef std::unordered_map<CollectionID,
                             std::shared_ptr<CollectionInfoCurrent>>
      DatabaseCollectionsCurrent;
  typedef std::unordered_map<DatabaseID, DatabaseCollectionsCurrent>
      AllCollectionsCurrent;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a number of cluster-wide unique IDs, returns the first
  /// one and guarantees that <number> are reserved for the caller.
  //////////////////////////////////////////////////////////////////////////////

  virtual uint64_t uniqid(uint64_t = 1) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flush the caches (used for testing only)
  //////////////////////////////////////////////////////////////////////////////

  virtual void flush() = 0;;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask whether a cluster database exists
  //////////////////////////////////////////////////////////////////////////////

  virtual bool doesDatabaseExist(DatabaseID const&, bool = false) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get list of databases in the cluster
  //////////////////////////////////////////////////////////////////////////////

  virtual std::vector<DatabaseID> databases(bool = false) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about our plan
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  virtual void loadPlan() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about current state
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  virtual void loadCurrent() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection
  /// If it is not found in the cache, the cache is reloaded once. The second
  /// argument can be a collection ID or a collection name (both cluster-wide).
  //////////////////////////////////////////////////////////////////////////////

  virtual std::shared_ptr<LogicalCollection> getCollection(DatabaseID const&,
                                                   CollectionID const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about all collections
  //////////////////////////////////////////////////////////////////////////////

  virtual std::vector<std::shared_ptr<LogicalCollection>> const getCollections(
      DatabaseID const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ask about a collection in current. This returns information about
  /// all shards in the collection.
  /// If it is not found in the cache, the cache is reloaded once.
  //////////////////////////////////////////////////////////////////////////////

  virtual std::shared_ptr<CollectionInfoCurrent> getCollectionCurrent(
      DatabaseID const&, CollectionID const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create database in coordinator
  //////////////////////////////////////////////////////////////////////////////

  virtual int createDatabaseCoordinator(std::string const&,
                                arangodb::velocypack::Slice const&,
                                std::string&, double) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop database in coordinator
  //////////////////////////////////////////////////////////////////////////////

  virtual int dropDatabaseCoordinator(std::string const& name, std::string& errorMsg,
                              double timeout) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create collection in coordinator
  //////////////////////////////////////////////////////////////////////////////

  virtual int createCollectionCoordinator(std::string const& databaseName,
                                  std::string const& collectionID,
                                  uint64_t numberOfShards,
                                  uint64_t replicationFactor,
                                  bool waitForReplication,
                                  arangodb::velocypack::Slice const& json,
                                  std::string& errorMsg, double timeout) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop collection in coordinator
  //////////////////////////////////////////////////////////////////////////////

  virtual int dropCollectionCoordinator(std::string const& databaseName,
                                std::string const& collectionID,
                                std::string& errorMsg, double timeout) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collection properties in coordinator
  //////////////////////////////////////////////////////////////////////////////

  virtual int setCollectionPropertiesCoordinator(std::string const& databaseName,
                                         std::string const& collectionID,
                                         LogicalCollection const*) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collection status in coordinator
  //////////////////////////////////////////////////////////////////////////////

  virtual int setCollectionStatusCoordinator(std::string const& databaseName,
                                     std::string const& collectionID,
                                     TRI_vocbase_col_status_e status) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////

  virtual int ensureIndexCoordinator(
      std::string const& databaseName, std::string const& collectionID,
      arangodb::velocypack::Slice const& slice, bool create,
      bool (*compare)(arangodb::velocypack::Slice const&,
                      arangodb::velocypack::Slice const&),
      arangodb::velocypack::Builder& resultBuilder, std::string& errorMsg, double timeout) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop an index in coordinator.
  //////////////////////////////////////////////////////////////////////////////

  virtual int dropIndexCoordinator(std::string const& databaseName,
                           std::string const& collectionID, TRI_idx_iid_t iid,
                           std::string& errorMsg, double timeout) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about servers from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  virtual void loadServers() = 0;;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the endpoint of a server from its ID.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  virtual std::string getServerEndpoint(ServerID const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the server ID for an endpoint.
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there an empty string is returned as an error.
  //////////////////////////////////////////////////////////////////////////////

  virtual std::string getServerName(std::string const& endpoint) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about all coordinators from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  virtual void loadCurrentCoordinators() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief (re-)load the information about all DBservers from the agency
  /// Usually one does not have to call this directly.
  //////////////////////////////////////////////////////////////////////////////

  virtual void loadCurrentDBServers() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a list of all DBServers in the cluster that have
  /// currently registered
  //////////////////////////////////////////////////////////////////////////////

  virtual std::vector<ServerID> getCurrentDBServers() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the servers who are responsible for a shard (one leader
  /// and possibly multiple followers).
  /// If it is not found in the cache, the cache is reloaded once, if
  /// it is still not there a pointer to an empty vector is returned as
  /// an error.
  //////////////////////////////////////////////////////////////////////////////

  virtual std::shared_ptr<std::vector<ServerID>> getResponsibleServer(ShardID const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the shard list of a collection, sorted numerically
  //////////////////////////////////////////////////////////////////////////////

  virtual std::shared_ptr<std::vector<ShardID>> getShardList(CollectionID const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find the shard that is responsible for a document
  //////////////////////////////////////////////////////////////////////////////

  virtual int getResponsibleShard(LogicalCollection*, arangodb::velocypack::Slice,
                          bool docComplete, ShardID& shardID,
                          bool& usesDefaultShardingAttributes,
                          std::string const& key = "") = 0;


  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the list of coordinator server names
  //////////////////////////////////////////////////////////////////////////////

  virtual std::vector<ServerID> getCurrentCoordinators() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate planned
  //////////////////////////////////////////////////////////////////////////////

  virtual void invalidatePlan() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate current
  //////////////////////////////////////////////////////////////////////////////

  virtual void invalidateCurrent() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidate current coordinators
  //////////////////////////////////////////////////////////////////////////////

  virtual void invalidateCurrentCoordinators() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current "Plan" structure
  //////////////////////////////////////////////////////////////////////////////

  virtual std::shared_ptr<VPackBuilder> getPlan() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current "Current" structure
  //////////////////////////////////////////////////////////////////////////////

  virtual std::shared_ptr<VPackBuilder> getCurrent() = 0;

  virtual std::vector<std::string> const& getFailedServers() = 0;
  virtual void setFailedServers(std::vector<std::string> const& failedServers) = 0;

  virtual std::unordered_map<ServerID, std::string> getServerAliases() = 0;

  virtual void clean() = 0;

  virtual TRI_voc_cid_t getCid(std::string const& databaseName, std::string const& collectionName) = 0;

  virtual bool hasDistributeShardsLike(std::string const& databaseName, std::string const& cidString) = 0;

  virtual std::shared_ptr<ShardMap> getShardMap(std::string const& databaseName, std::string const& cidString) = 0;

  virtual ~ClusterInfoInterface() {}
};

}

#endif