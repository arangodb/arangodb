////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_SHARDING_INFO_H
#define ARANGOD_CLUSTER_SHARDING_INFO_H 1

#include "Basics/Result.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <unordered_map>
#include <unordered_set>

namespace arangodb {
class LogicalCollection;
class ShardingStrategy;

namespace application_features {
class ApplicationServer;
}

typedef std::string ServerID;  // ID of a server
typedef std::string ShardID;   // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class ShardingInfo {
 public:
  ShardingInfo() = delete;
  ShardingInfo(arangodb::velocypack::Slice info, LogicalCollection* collection);
  ShardingInfo(ShardingInfo const& other, LogicalCollection* collection);
  ShardingInfo& operator=(ShardingInfo const& other) = delete;
  ~ShardingInfo();

  bool usesSameShardingStrategy(ShardingInfo const* other) const;
  std::string shardingStrategyName() const;

  LogicalCollection* collection() const;
  void toVelocyPack(arangodb::velocypack::Builder& result, bool translateCids) const;

  std::string const& distributeShardsLike() const;
  void distributeShardsLike(std::string const& cid, ShardingInfo const* other);

  std::vector<std::string> const& avoidServers() const;
  void avoidServers(std::vector<std::string> const&);

  size_t replicationFactor() const;
  void replicationFactor(size_t);

  size_t writeConcern() const;
  void writeConcern(size_t);

  void setWriteConcernAndReplicationFactor(size_t writeConcern, size_t replicationFactor);

  bool isSatellite() const;

  size_t numberOfShards() const;

  /// @brief update the number of shards. note that this method
  /// should never be called after a collection was properly initialized
  /// at the moment it is necessary to have it because of the collection
  /// class hierarchy. VirtualSmartEdgeCollection calls this function
  /// in its constructor, after the shardingInfo has been set up already
  void numberOfShards(size_t numberOfShards);

  /// @brief validates the number of shards and the replication factor
  /// in slice against the minimum and maximum configured values
  static Result validateShardsAndReplicationFactor(arangodb::velocypack::Slice slice,
                                                   application_features::ApplicationServer const& server,
                                                   bool enforceReplicationFactor);

  bool usesDefaultShardKeys() const;
  std::vector<std::string> const& shardKeys() const;

  std::shared_ptr<ShardMap> shardIds() const;

  // return a sorted vector of ShardIDs
  std::shared_ptr<std::vector<ShardID>> shardListAsShardID() const;

  // return a filtered list of the collection's shards
  std::shared_ptr<ShardMap> shardIds(std::unordered_set<std::string> const& includedShards) const;
  void setShardMap(std::shared_ptr<ShardMap> const& map);

  ErrorCode getResponsibleShard(arangodb::velocypack::Slice slice, bool docComplete,
                                ShardID& shardID, bool& usesDefaultShardKeys,
                                arangodb::velocypack::StringRef const& key);

  static void sortShardNamesNumerically(std::vector<ShardID>& list);

 private:
  // @brief the logical collection we are working for
  LogicalCollection* _collection;

  // @brief number of shards
  size_t _numberOfShards;

  // @brief replication factor (1 = no replication, 0 = smart edge collection)
  size_t _replicationFactor;

  // @brief write concern (_writeConcern <= _replicationFactor)
  // Writes will be disallowed if we know we cannot fulfill minReplicationFactor.
  size_t _writeConcern;

  // @brief name of other collection this collection's shards should be
  // distributed like
  std::string _distributeShardsLike;

  // @brief servers that will be ignored when distributing shards
  std::vector<std::string> _avoidServers;

  // @brief vector of shard keys in use. this is immutable after initial setup
  std::vector<std::string> _shardKeys;

  // @brief current shard ids
  std::shared_ptr<ShardMap> _shardIds;

  // @brief vector of shard keys in use. this is immutable after initial setup
  std::unique_ptr<ShardingStrategy> _shardingStrategy;
};
}  // namespace arangodb

#endif
