////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Result.h"
#include "Containers/FlatHashMap.h"
#include "RestServer/arangod.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <unordered_map>
#include <unordered_set>
#include <atomic>

namespace arangodb {
class LogicalCollection;
class ShardingStrategy;

namespace application_features {
class ApplicationServer;
}

typedef std::string ServerID;  // ID of a server
typedef std::string ShardID;   // ID of a shard
using ShardMap = containers::FlatHashMap<ShardID, std::vector<ServerID>>;

class ShardingInfo {
 public:
  ShardingInfo() = delete;
  ShardingInfo(arangodb::velocypack::Slice info, LogicalCollection* collection);
  ShardingInfo(ShardingInfo const& other, LogicalCollection* collection);
  ShardingInfo& operator=(ShardingInfo const& other) = delete;
  ~ShardingInfo();

  bool usesSameShardingStrategy(ShardingInfo const* other) const;
  std::string shardingStrategyName() const;

  LogicalCollection* collection() const noexcept;
  void toVelocyPack(arangodb::velocypack::Builder& result,
                    bool translateCids) const;

  std::string const& distributeShardsLike() const noexcept;
  void distributeShardsLike(std::string const& cid, ShardingInfo const* other);

  std::vector<std::string> const& avoidServers() const noexcept;
  void avoidServers(std::vector<std::string> const&);

  size_t replicationFactor() const noexcept;
  void replicationFactor(size_t);

  size_t writeConcern() const noexcept;
  void writeConcern(size_t);

  void setWriteConcernAndReplicationFactor(size_t writeConcern,
                                           size_t replicationFactor);

  std::pair<bool, bool>
  makeSatellite();  // Return note booleans: (isError, isASatellite)
  bool isSatellite() const noexcept;

  size_t numberOfShards() const noexcept;

  /// @brief update the number of shards. note that this method
  /// should never be called after a collection was properly initialized
  /// at the moment it is necessary to have it because of the collection
  /// class hierarchy. VirtualClusterSmartEdgeCollection calls this function
  /// in its constructor, after the shardingInfo has been set up already
  void numberOfShards(size_t numberOfShards);

  /// @brief validates the number of shards and the replication factor
  /// in slice against the minimum and maximum configured values
  static Result validateShardsAndReplicationFactor(
      arangodb::velocypack::Slice slice, ArangodServer const& server,
      bool enforceReplicationFactor);

  bool usesDefaultShardKeys() const noexcept;
  std::vector<std::string> const& shardKeys() const noexcept;

  std::shared_ptr<ShardMap> shardIds() const;

  // return a sorted vector of ShardIDs
  std::shared_ptr<std::vector<ShardID>> shardListAsShardID() const;

  // return a filtered list of the collection's shards
  std::shared_ptr<ShardMap> shardIds(
      std::unordered_set<std::string> const& includedShards) const;
  void setShardMap(std::shared_ptr<ShardMap> const& map);

  ErrorCode getResponsibleShard(arangodb::velocypack::Slice slice,
                                bool docComplete, ShardID& shardID,
                                bool& usesDefaultShardKeys,
                                std::string_view key);

  static void sortShardNamesNumerically(std::vector<ShardID>& list);

 private:
  // @brief the logical collection we are working for
  LogicalCollection* _collection;

  // @brief number of shards
  size_t _numberOfShards;

  // _replicationFactor and _writeConcern are set in
  // setWriteConcernAndReplicationFactor, but there are places that might read
  // these values before they are set (e.g.,
  // LogicalCollection::appendVelocyPack), and since these can be executed by a
  // different thread _replicationFactor and _writeConcern must both be atomic
  // to avoid data races.

  // @brief replication factor (1 = no replication, 0 = smart edge collection)
  std::atomic<size_t> _replicationFactor;

  // @brief write concern (_writeConcern <= _replicationFactor)
  // Writes will be disallowed if we know we cannot fulfill
  // minReplicationFactor.
  std::atomic<size_t> _writeConcern;

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
