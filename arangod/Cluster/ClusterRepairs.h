////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB3_CLUSTERREPAIRS_H
#define ARANGODB3_CLUSTERREPAIRS_H

#include <arangod/Agency/AgencyComm.h>
#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/velocypack-common.h>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include "ClusterInfo.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace cluster_repairs {

using DBServers = std::vector<ServerID>;

class VersionSort {
  using CharOrInt = boost::variant<char, uint64_t>;

 public:

  bool operator()(std::string const &a, std::string const &b) const;

 private:

  std::vector<CharOrInt> static splitVersion(std::string const &str);
};


struct Collection {
  // corresponding slice
  // TODO remove this, it should not be needed
  VPackSlice const slice;

  DatabaseID database;
  std::string name;
  CollectionID id;
  uint64_t replicationFactor;
  boost::optional<CollectionID const> distributeShardsLike;
  boost::optional<CollectionID const> repairingDistributeShardsLike;
  boost::optional<bool> repairingDistributeShardsLikeReplicationFactorReduced;
  std::map<ShardID, DBServers, VersionSort> shardsById;

  // TODO this shouldn't be needed anymore and can be removed
  std::map<std::string, VPackSlice> residualAttributes;

  std::string inline fullName() const {
    return this->database + "/" + this->name;
  }

  std::string inline agencyCollectionId() const {
    return "Plan/Collections/" + this->database + "/" + this->id;
  }

  std::shared_ptr<VPackBuffer<uint8_t>>
  createShardDbServerArray(
    ShardID const &shardId
  ) const;

  // maybe more?
  // isSystem
  // numberOfShards
  // deleted
};

struct MoveShardOperation {
  DatabaseID database;
  CollectionID collection;
  ShardID shard;
  ServerID from;
  ServerID to;
  bool isLeader;

  MoveShardOperation() = delete;
};

using RepairOperation = boost::variant<MoveShardOperation, AgencyWriteTransaction>;

class DistributeShardsLikeRepairer {
 public:
  std::list<RepairOperation> repairDistributeShardsLike(
    VPackSlice const& planCollections, VPackSlice const& planDbServers
  );

 private:
  std::vector< std::shared_ptr<VPackBuffer<uint8_t>> > _vPackBuffers;

  std::map<ShardID, DBServers, VersionSort> static
  readShards(VPackSlice const& shards);

  DBServers static
  readDatabases(VPackSlice const& planDbServers);

  std::map<CollectionID, struct Collection> static
  readCollections(VPackSlice const& collectionsByDatabase);

  boost::optional<ServerID const> static
  findFreeServer(
    DBServers const& availableDbServers,
    DBServers const& shardDbServers
  );

  std::vector<CollectionID> static
  findCollectionsToFix(std::map<CollectionID, struct Collection> collections);

  DBServers static serverSetDifference(
    DBServers setA,
    DBServers setB
  );

  DBServers static serverSetSymmetricDifference(
    DBServers setA,
    DBServers setB
  );

  MoveShardOperation
  createMoveShardOperation(
    Collection& collection,
    ShardID const& shardId,
    ServerID const& fromServerId,
    ServerID const& toServerId,
    bool isLeader
  );

  std::list<RepairOperation>
  fixLeader(
    DBServers const& availableDbServers,
    Collection& collection,
    Collection& proto,
    ShardID const& shardId,
    ShardID const& protoShardId
  );

  std::list<RepairOperation>
  fixShard(
    DBServers const& availableDbServers,
    Collection& collection,
    Collection& proto,
    ShardID const& shardId,
    ShardID const& protoShardId
  );

  boost::optional<AgencyWriteTransaction>
  createFixServerOrderTransaction(
    Collection& collection,
    Collection const& proto,
    ShardID const& shardId,
    ShardID const& protoShardId
  );
};


}
}

#endif  // ARANGODB3_CLUSTERREPAIRS_H
