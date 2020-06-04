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

#ifndef ARANGOD_CLUSTER_CLUSTER_REPAIR_DISTRIBUTE_SHARDS_LIKE_H
#define ARANGOD_CLUSTER_CLUSTER_REPAIR_DISTRIBUTE_SHARDS_LIKE_H

#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-common.h>
#include <optional>
#include <variant>

#include "Agency/AgencyComm.h"
#include "ClusterInfo.h"
#include "ClusterRepairOperations.h"
#include "ResultT.h"

namespace arangodb {
namespace velocypack {
class Slice;
template <typename T>
class Buffer;
}  // namespace velocypack

namespace cluster_repairs {

using DBServers = std::vector<ServerID>;
using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

template <typename T, std::size_t N>
std::ostream& operator<<(std::ostream& stream, std::array<T, N> array) {
  stream << "std::array<" << typeid(T).name() << "> { ";
  std::for_each(array.begin(), array.end(),
                [&stream](T const& val) { stream << val << ", "; });

  stream << "}";

  return stream;
}

inline std::ostream& operator<<(std::ostream& stream, VPackBufferPtr const& vpack) {
  return stream << "std::shared_ptr<VPackBuffer> { "
                << velocypack::Slice(vpack->data()).toJson() << " "
                << "}";
}

struct Collection {
  DatabaseID database;
  std::string name;
  CollectionID id;
  uint64_t replicationFactor;
  bool deleted;
  bool isSmart;
  std::optional<CollectionID> distributeShardsLike;
  std::optional<CollectionID> repairingDistributeShardsLike;
  std::map<ShardID, DBServers, VersionSort> shardsById;

  std::string inline fullName() const {
    return this->database + "/" + this->name;
  }

  Collection() = delete;

  // constructor with named parameters
  Collection(tagged_argument<tag::database, DatabaseID> database_,
             tagged_argument<tag::collectionId, CollectionID> collectionId_,
             tagged_argument<tag::collectionName, std::string> collectionName_,
             tagged_argument<tag::replicationFactor, uint64_t> replicationFactor_,
             tagged_argument<tag::deleted, bool> deleted_,
             tagged_argument<tag::isSmart, bool> isSmart_,
             tagged_argument<tag::distributeShardsLike, std::optional<CollectionID>> distributeShardsLike_,
             tagged_argument<tag::repairingDistributeShardsLike, std::optional<CollectionID>> repairingDistributeShardsLike_,
             tagged_argument<tag::shardsById, std::map<ShardID, DBServers, VersionSort>> shardsById_);
};

class DistributeShardsLikeRepairer {
 public:
  ResultT<std::map<CollectionID, ResultT<std::list<RepairOperation>>>> static repairDistributeShardsLike(
      velocypack::Slice const& planCollections, velocypack::Slice const& supervisionHealth);

 private:
  std::map<ShardID, DBServers, VersionSort> static readShards(velocypack::Slice const& shards);

  DBServers static readDatabases(velocypack::Slice const& planDbServers);

  ResultT<std::map<CollectionID, struct Collection>> static readCollections(
      velocypack::Slice const& collectionsByDatabase);

  std::optional<ServerID const> static findFreeServer(DBServers const& availableDbServers,
                                                        DBServers const& shardDbServers);

  std::vector<std::pair<CollectionID, Result>> static findCollectionsToFix(
      std::map<CollectionID, struct Collection> collections);

  DBServers static serverSetDifference(DBServers setA, DBServers setB);

  DBServers static serverSetSymmetricDifference(DBServers setA, DBServers setB);

  MoveShardOperation static createMoveShardOperation(struct Collection& collection,
                                                     ShardID const& shardId,
                                                     ServerID const& fromServerId,
                                                     ServerID const& toServerId,
                                                     bool isLeader);

  // "proto collection" always means the collection referred to in the
  // "distributeShardsLike" attribute of "collection"

  ResultT<std::list<RepairOperation>> static fixLeader(DBServers const& availableDbServers,
                                                       struct Collection& collection,
                                                       struct Collection const& proto,
                                                       ShardID const& shardId,
                                                       ShardID const& protoShardId);

  ResultT<std::list<RepairOperation>> static fixShard(DBServers const& availableDbServers,
                                                      struct Collection& collection,
                                                      struct Collection const& proto,
                                                      ShardID const& shardId,
                                                      ShardID const& protoShardId);

  ResultT<std::optional<FixServerOrderOperation>> static createFixServerOrderOperation(
      struct Collection& collection, struct Collection const& proto,
      ShardID const& shardId, ShardID const& protoShardId);

  ResultT<BeginRepairsOperation> static createBeginRepairsOperation(
      struct Collection& collection, struct Collection const& proto);

  std::vector<cluster_repairs::ShardWithProtoAndDbServers> static createShardVector(
      std::map<ShardID, DBServers, VersionSort> const& shardsById,
      std::map<ShardID, DBServers, VersionSort> const& protoShardsById);

  ResultT<FinishRepairsOperation> static createFinishRepairsOperation(
      struct Collection& collection, struct Collection const& proto);

  ResultT<std::list<RepairOperation>> static fixAllShardsOfCollection(
      struct Collection& collection, struct Collection const& proto,
      DBServers const& availableDbServers);
};
}  // namespace cluster_repairs
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_CLUSTER_REPAIR_DISTRIBUTE_SHARDS_LIKE_H
