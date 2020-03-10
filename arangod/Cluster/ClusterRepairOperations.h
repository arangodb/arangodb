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

#ifndef ARANGOD_CLUSTER_CLUSTER_REPAIR_OPERATIONS_H
#define ARANGOD_CLUSTER_CLUSTER_REPAIR_OPERATIONS_H

#include <velocypack/velocypack-common.h>
#include <map>
#include <optional>
#include <variant>
#include "ClusterInfo.h"

namespace arangodb {
namespace velocypack {
template <typename T>
class Buffer;
}
class ClusterInfo;

namespace cluster_repairs {
using DBServers = std::vector<ServerID>;
using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

// Elements are (shardId, protoShardId, dbServers). The dbServers are
// the same for both shard and protoShard at this point.
using ShardWithProtoAndDbServers = std::tuple<ShardID, ShardID, DBServers>;

class VersionSort {
 public:
  bool operator()(std::string const& a, std::string const& b) const;

 private:
  // boost::variant cannot discern between ambiguously convertible types
  // (unlike std::variant from C++17). So, for compilers where char is unsigned,
  // using uint64_t results in a compile error (vice versa for signed chars and
  // int64_t) and therefore this distinct type is needed.
  class WrappedUInt64 {
   public:
    uint64_t value;

    explicit inline WrappedUInt64(uint64_t value_) : value(value_) {}

    bool inline operator<(WrappedUInt64 const& other) const {
      return this->value < other.value;
    }
  };

  using CharOrInt = std::variant<char, WrappedUInt64>;

  std::vector<CharOrInt> static splitVersion(std::string const& str);
};

// "proto collection" always means the collection referred to in the
// "distributeShardsLike" attribute of "collection"

// All RepairOperations use a constructor with named parameters. This is done to
// forbid braced initializer lists and assure that all constructions initialize
// every member (defaults really don't make sense here), make constructions more
// readable and avoid mixing up arguments of the same type (most are std::string
// or typedefs thereof).

// The following are used for the named parameters mentioned above.
template <typename Tag, typename Type>
struct tagged_argument {
  Type const& value;
};

template <typename Tag, typename Type>
struct keyword {
  // NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
  struct tagged_argument<Tag, Type> const operator=(Type const& arg) const {
    return tagged_argument<Tag, Type>{arg};
  }

  static keyword<Tag, Type> const instance;
};

template <typename Tag, typename Type>
struct keyword<Tag, Type> const keyword<Tag, Type>::instance = {};

// Parameters used in Operation-constructors

namespace tag {
struct database;
struct collectionId;
struct collectionName;
struct protoCollectionId;
struct protoCollectionName;
struct shard;
struct protoShard;
struct from;
struct to;
struct isLeader;
struct protoReplicationFactor;
struct collectionReplicationFactor;
struct replicationFactor;
struct renameDistributeShardsLike;
struct shards;
struct leader;
struct followers;
struct protoFollowers;
struct distributeShardsLike;
struct repairingDistributeShardsLike;
struct shardsById;
struct deleted;
struct isSmart;
}  // namespace tag
namespace {
keyword<tag::database, std::string> _database = decltype(_database)::instance;
keyword<tag::collectionId, std::string> _collectionId = decltype(_collectionId)::instance;
keyword<tag::collectionName, std::string> _collectionName =
    decltype(_collectionName)::instance;
keyword<tag::protoCollectionId, std::string> _protoCollectionId =
    decltype(_protoCollectionId)::instance;
keyword<tag::protoCollectionName, std::string> _protoCollectionName =
    decltype(_protoCollectionName)::instance;
keyword<tag::shard, std::string> _shard = decltype(_shard)::instance;
keyword<tag::protoShard, std::string> _protoShard = decltype(_protoShard)::instance;
keyword<tag::from, std::string> _from = decltype(_from)::instance;
keyword<tag::to, std::string> _to = decltype(_to)::instance;
keyword<tag::isLeader, bool> _isLeader = decltype(_isLeader)::instance;
keyword<tag::protoReplicationFactor, uint64_t> _protoReplicationFactor =
    decltype(_protoReplicationFactor)::instance;
keyword<tag::collectionReplicationFactor, uint64_t> _collectionReplicationFactor =
    decltype(_collectionReplicationFactor)::instance;
keyword<tag::replicationFactor, uint64_t> _replicationFactor =
    decltype(_replicationFactor)::instance;
keyword<tag::renameDistributeShardsLike, bool> _renameDistributeShardsLike =
    decltype(_renameDistributeShardsLike)::instance;
keyword<tag::shards, std::vector<ShardWithProtoAndDbServers>> _shards =
    decltype(_shards)::instance;
keyword<tag::leader, std::string> _leader = decltype(_leader)::instance;
keyword<tag::followers, std::vector<std::string>> _followers = decltype(_followers)::instance;
keyword<tag::protoFollowers, std::vector<std::string>> _protoFollowers =
    decltype(_protoFollowers)::instance;
keyword<tag::distributeShardsLike, std::optional<CollectionID>> _distributeShardsLike =
    decltype(_distributeShardsLike)::instance;
keyword<tag::repairingDistributeShardsLike, std::optional<CollectionID>> _repairingDistributeShardsLike =
    decltype(_repairingDistributeShardsLike)::instance;
keyword<tag::shardsById, std::map<ShardID, DBServers, VersionSort>> _shardsById =
    decltype(_shardsById)::instance;
keyword<tag::deleted, bool> _deleted = decltype(_deleted)::instance;
keyword<tag::isSmart, bool> _isSmart = decltype(_isSmart)::instance;
}  // namespace

// Applies the following changes iff renameDistributeShardsLike is true:
//  * Renames "distributeShardsLike" to "repairingDistributeShardsLike"
//  * Sets collection.replicationFactor = `protoReplicationFactor`
// Asserts the following preconditions:
//  if renameDistributeShardsLike:
//    * collection.distributeShardsLike == `protoCollectionId`
//    * collection.repairingDistributeShardsLike == undefined
//    * collection.replicationFactor == `collectionReplicationFactor`
//    * protoCollection.replicationFactor == `protoReplicationFactor`
//  else:
//    * collection.repairingDistributeShardsLike == `protoCollectionId`
//    * collection.distributeShardsLike == undefined
//    * collection.replicationFactor == `protoReplicationFactor` (sic!)
//    * protoCollection.replicationFactor == `protoReplicationFactor`
//
// See RepairOperationToTransactionVisitor for the implementation.
struct BeginRepairsOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  CollectionID protoCollectionId;
  std::string protoCollectionName;
  uint64_t collectionReplicationFactor;
  uint64_t protoReplicationFactor;
  bool renameDistributeShardsLike;

  BeginRepairsOperation() = delete;

  // constructor with named parameters
  BeginRepairsOperation(
      tagged_argument<tag::database, DatabaseID> database_,
      tagged_argument<tag::collectionId, CollectionID> collectionId_,
      tagged_argument<tag::collectionName, std::string> collectionName_,
      tagged_argument<tag::protoCollectionId, CollectionID> protoCollectionId_,
      tagged_argument<tag::protoCollectionName, std::string> protoCollectionName_,
      tagged_argument<tag::collectionReplicationFactor, uint64_t> collectionReplicationFactor_,
      tagged_argument<tag::protoReplicationFactor, uint64_t> protoReplicationFactor_,
      tagged_argument<tag::renameDistributeShardsLike, bool> renameDistributeShardsLike_);
};

// Applies the following changes:
//  * Renames "repairingDistributeShardsLike" to "distributeShardsLike"
// Asserts the following preconditions:
//  * collection.repairingDistributeShardsLike == `protoCollectionId`
//  * collection.distributeShardsLike == undefined
//  * collection.replicationFactor == `replicationFactor`
//  * protoCollection.replicationFactor == `replicationFactor`
//  * shards of both collection and protoCollection match `shards`
//
// `shards` should contain *all* shards or collection and protoCollection,
// so if this transaction succeeds, the collection is guaranteed to be
// completely fixed.
//
// See RepairOperationToTransactionVisitor for the implementation.
struct FinishRepairsOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  CollectionID protoCollectionId;
  std::string protoCollectionName;
  std::vector<ShardWithProtoAndDbServers> shards;
  uint64_t replicationFactor;

  FinishRepairsOperation() = delete;

  // constructor with named parameters
  FinishRepairsOperation(
      tagged_argument<tag::database, DatabaseID> database_,
      tagged_argument<tag::collectionId, CollectionID> collectionId_,
      tagged_argument<tag::collectionName, std::string> collectionName_,
      tagged_argument<tag::protoCollectionId, CollectionID> protoCollectionId_,
      tagged_argument<tag::protoCollectionName, std::string> protoCollectionName_,
      tagged_argument<tag::shards, std::vector<ShardWithProtoAndDbServers>> shards_,
      tagged_argument<tag::replicationFactor, uint64_t> replicationFactor_);
};

// Writes a moveShard job in Target/ToDo/ to move
// the `shard` from the server `from` to server `to`.
//
// See RepairOperationToTransactionVisitor for the implementation.
struct MoveShardOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  ShardID shard;
  ServerID from;
  ServerID to;
  bool isLeader;

  MoveShardOperation() = delete;

  // constructor with named parameters
  MoveShardOperation(tagged_argument<tag::database, DatabaseID> database_,
                     tagged_argument<tag::collectionId, CollectionID> collectionId_,
                     tagged_argument<tag::collectionName, std::string> collectionName_,
                     tagged_argument<tag::shard, ShardID> shard_,
                     tagged_argument<tag::from, ServerID> from_,
                     tagged_argument<tag::to, ServerID> to_,
                     tagged_argument<tag::isLeader, bool> isLeader_);

  VPackBufferPtr toVPackTodo(uint64_t jobId,
                             std::chrono::system_clock::time_point jobCreationTimestamp) const;
};

// Applies the following changes:
//  * Sets collection/shards/`shard` to leader :: protoFollowers
// Asserts the following preconditions:
//  * collection/shards/`shard` == leader :: followers
//  * collection/shards/`shard` == leader :: protoFollowers
//
// See RepairOperationToTransactionVisitor for the implementation.
struct FixServerOrderOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  CollectionID protoCollectionId;
  std::string protoCollectionName;
  ShardID shard;
  ShardID protoShard;
  ServerID leader;
  std::vector<ServerID> followers;
  std::vector<ServerID> protoFollowers;

  FixServerOrderOperation() = delete;

  // constructor with named parameters
  FixServerOrderOperation(
      tagged_argument<tag::database, DatabaseID> database_,
      tagged_argument<tag::collectionId, CollectionID> collectionId_,
      tagged_argument<tag::collectionName, std::string> collectionName_,
      tagged_argument<tag::protoCollectionId, CollectionID> protoCollectionId_,
      tagged_argument<tag::protoCollectionName, std::string> protoCollectionName_,
      tagged_argument<tag::shard, ShardID> shard_,
      tagged_argument<tag::protoShard, ShardID> protoShard_,
      tagged_argument<tag::leader, ServerID> leader_,
      tagged_argument<tag::followers, std::vector<ServerID>> followers_,
      tagged_argument<tag::protoFollowers, std::vector<ServerID>> protoFollowers_);
};

bool operator==(BeginRepairsOperation const& left, BeginRepairsOperation const& right);

bool operator==(FinishRepairsOperation const& left, FinishRepairsOperation const& right);

bool operator==(MoveShardOperation const& left, MoveShardOperation const& right);

bool operator==(FixServerOrderOperation const& left, FixServerOrderOperation const& right);

std::ostream& operator<<(std::ostream& ostream, BeginRepairsOperation const& operation);

std::ostream& operator<<(std::ostream& ostream, FinishRepairsOperation const& operation);

std::ostream& operator<<(std::ostream& ostream, MoveShardOperation const& operation);

std::ostream& operator<<(std::ostream& ostream, FixServerOrderOperation const& operation);

using RepairOperation =
    std::variant<BeginRepairsOperation const, FinishRepairsOperation const, MoveShardOperation const, FixServerOrderOperation const>;

std::string getTypeAsString(RepairOperation const& op);

std::ostream& operator<<(std::ostream& ostream, RepairOperation const& operation);

// Converts any RepairOperation to a Transaction. If its a job (i.e. put in
// Target/ToDo/), it returns the corresponding job id as well.
class RepairOperationToTransactionVisitor {
  using ReturnValueT = std::pair<AgencyWriteTransaction, std::optional<uint64_t>>;

 public:
  RepairOperationToTransactionVisitor(ClusterInfo&);
  RepairOperationToTransactionVisitor(std::function<uint64_t()> getJobId,
                                      std::function<std::chrono::system_clock::time_point()> getJobCreationTimestamp);

  ReturnValueT operator()(BeginRepairsOperation const& op);

  ReturnValueT operator()(FinishRepairsOperation const& op);

  ReturnValueT operator()(MoveShardOperation const& op);

  ReturnValueT operator()(FixServerOrderOperation const& op);

 private:
  std::vector<VPackBufferPtr> _vpackBufferArray;
  std::function<uint64_t()> _getJobId;
  std::function<std::chrono::system_clock::time_point()> _getJobCreationTimestamp;

  std::vector<VPackBufferPtr>&& steal();

  std::string agencyCollectionId(DatabaseID database, CollectionID collection) const;

  VPackBufferPtr createShardDbServerArray(ServerID const& leader,
                                          DBServers const& followers) const;

  template <typename T>
  VPackSlice createSingleValueVPack(T val);
};

// Adds any RepairOperation to a VPack as an object, suitable for users to see.
// Doesn't contain all data, some members are named differently.
// TODO Maybe it would still be good to add all members, at least for the
// functional tests?
class RepairOperationToVPackVisitor {
 public:
  RepairOperationToVPackVisitor() = delete;
  explicit RepairOperationToVPackVisitor(VPackBuilder& builder);

  void operator()(BeginRepairsOperation const& op);

  void operator()(FinishRepairsOperation const& op);

  void operator()(MoveShardOperation const& op);

  void operator()(FixServerOrderOperation const& op);

 private:
  VPackBuilder& _builder;

  VPackBuilder& builder();
};
}  // namespace cluster_repairs
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_CLUSTER_REPAIR_OPERATIONS_H
