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

#ifndef ARANGODB3_CLUSTERREPAIROPERATIONS_H
#define ARANGODB3_CLUSTERREPAIROPERATIONS_H

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <velocypack/velocypack-common.h>

#include "ClusterInfo.h"

namespace arangodb {
namespace velocypack {
template <typename T>
class Buffer;
}

namespace cluster_repairs {
using DBServers = std::vector<ServerID>;
using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

// Elements are (shardId, protoShardId, dbServers). The dbServers are
// the same for both shard and protoShard at this point.
using ShardWithProtoAndDbServers = std::tuple<ShardID, ShardID, DBServers>;

class VersionSort {
  using CharOrInt = boost::variant<char, uint64_t>;

 public:
  bool operator()(std::string const& a, std::string const& b) const;

 private:
  std::vector<CharOrInt> static splitVersion(std::string const& str);
};

// "proto collection" always means the collection referred to in the
// "distributeShardsLike" attribute of "collection"

// All RepairOperations use a named constructor ::create() with named
// parameters, while the default constructor is deleted and one other
// constructor specified to forbid braced initializer lists.
// This is done to assure that all constructions initialize every member
// (defaults really don't make sense here), make constructions more readable
// and avoid mixing up arguments of the same type (most are std::string or
// typedefs thereof).

// The following are used for the named initializers mentioned above.
template <typename Tag, typename Type>
struct tagged_argument {
  Type const& value;
};

template <typename Tag, typename Type>
struct keyword {
  struct tagged_argument<Tag, Type> const operator=(Type const& arg) const {
    return tagged_argument<Tag, Type>{arg};
  } static keyword<Tag, Type> const instance;
};

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
}
namespace {
keyword<tag::database, std::string> _database = decltype(_database)::instance;
keyword<tag::collectionId, std::string> _collectionId =
    decltype(_collectionId)::instance;
keyword<tag::collectionName, std::string> _collectionName =
    decltype(_collectionName)::instance;
keyword<tag::protoCollectionId, std::string> _protoCollectionId =
    decltype(_protoCollectionId)::instance;
keyword<tag::protoCollectionName, std::string> _protoCollectionName =
    decltype(_protoCollectionName)::instance;
keyword<tag::shard, std::string> _shard = decltype(_shard)::instance;
keyword<tag::protoShard, std::string> _protoShard =
    decltype(_protoShard)::instance;
keyword<tag::from, std::string> _from = decltype(_from)::instance;
keyword<tag::to, std::string> _to = decltype(_to)::instance;
keyword<tag::isLeader, bool> _isLeader = decltype(_isLeader)::instance;
keyword<tag::protoReplicationFactor, size_t> _protoReplicationFactor =
    decltype(_protoReplicationFactor)::instance;
keyword<tag::collectionReplicationFactor, size_t> _collectionReplicationFactor =
    decltype(_collectionReplicationFactor)::instance;
keyword<tag::replicationFactor, size_t> _replicationFactor =
    decltype(_replicationFactor)::instance;
keyword<tag::renameDistributeShardsLike, bool> _renameDistributeShardsLike =
    decltype(_renameDistributeShardsLike)::instance;
keyword<tag::shards, std::vector<ShardWithProtoAndDbServers>> _shards =
    decltype(_shards)::instance;
keyword<tag::leader, std::string> _leader = decltype(_leader)::instance;
keyword<tag::followers, std::vector<std::string>> _followers =
    decltype(_followers)::instance;
keyword<tag::protoFollowers, std::vector<std::string>> _protoFollowers =
    decltype(_protoFollowers)::instance;
}

struct BeginRepairsOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  CollectionID protoCollectionId;
  std::string protoCollectionName;
  size_t collectionReplicationFactor;
  size_t protoReplicationFactor;
  bool renameDistributeShardsLike;

  BeginRepairsOperation() = delete;

  // named constructor with named parameters
  static inline BeginRepairsOperation create(
      const tagged_argument<tag::database, DatabaseID> database,
      const tagged_argument<tag::collectionId, CollectionID> collectionId,
      const tagged_argument<tag::collectionName, std::string> collectionName,
      const tagged_argument<tag::protoCollectionId, CollectionID>
          protoCollectionId,
      const tagged_argument<tag::protoCollectionName, std::string>
          protoCollectionName,
      const tagged_argument<tag::collectionReplicationFactor, size_t>
          collectionReplicationFactor,
      const tagged_argument<tag::protoReplicationFactor, size_t>
          protoReplicationFactor,
      const tagged_argument<tag::renameDistributeShardsLike, bool>
          renameDistributeShardsLike) {
    return BeginRepairsOperation(
        database.value, collectionId.value, collectionName.value,
        protoCollectionId.value, protoCollectionName.value,
        collectionReplicationFactor.value, protoReplicationFactor.value,
        renameDistributeShardsLike.value);
  }

 private:
  BeginRepairsOperation(const DatabaseID& database,
                        const CollectionID& collectionId,
                        const std::string& collectionName,
                        const CollectionID& protoCollectionId,
                        const std::string& protoCollectionName,
                        size_t collectionReplicationFactor,
                        size_t protoReplicationFactor,
                        bool renameDistributeShardsLike);
};

struct FinishRepairsOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  CollectionID protoCollectionId;
  std::string protoCollectionName;
  std::vector<ShardWithProtoAndDbServers> shards;
  size_t replicationFactor;

  FinishRepairsOperation() = delete;

  // named constructor with named parameters
  static FinishRepairsOperation create(
      const tagged_argument<tag::database, DatabaseID> database,
      const tagged_argument<tag::collectionId, CollectionID> collectionId,
      const tagged_argument<tag::collectionName, std::string> collectionName,
      const tagged_argument<tag::protoCollectionId, CollectionID>
          protoCollectionId,
      const tagged_argument<tag::protoCollectionName, std::string>
          protoCollectionName,
      const tagged_argument<tag::shards,
                            std::vector<ShardWithProtoAndDbServers>>
          shards,
      const tagged_argument<tag::replicationFactor, size_t> replicationFactor) {
    return FinishRepairsOperation{database.value,
                                  collectionId.value,
                                  collectionName.value,
                                  protoCollectionId.value,
                                  protoCollectionName.value,
                                  shards.value,
                                  replicationFactor.value};
  }

 private:
  FinishRepairsOperation(const DatabaseID& database,
                         const CollectionID& collectionId,
                         const std::string& collectionName,
                         const CollectionID& protoCollectionId,
                         const std::string& protoCollectionName,
                         const std::vector<ShardWithProtoAndDbServers>& shards,
                         size_t replicationFactor);
};

struct MoveShardOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  ShardID shard;
  ServerID from;
  ServerID to;
  bool isLeader;

  MoveShardOperation() = delete;

  // named constructor with named parameters
  static MoveShardOperation create(
      const tagged_argument<tag::database, DatabaseID> database,
      const tagged_argument<tag::collectionId, CollectionID> collectionId,
      const tagged_argument<tag::collectionName, std::string> collectionName,
      const tagged_argument<tag::shard, ShardID> shard,
      const tagged_argument<tag::from, ServerID> from,
      const tagged_argument<tag::to, ServerID> to,
      const tagged_argument<tag::isLeader, bool> isLeader) {
    return MoveShardOperation(database.value, collectionId.value,
                              collectionName.value, shard.value, from.value,
                              to.value, isLeader.value);
  }

  VPackBufferPtr toVpackTodo(
      uint64_t jobId,
      std::chrono::system_clock::time_point jobCreationTimestamp) const;

 private:
  MoveShardOperation(const DatabaseID& database,
                     const CollectionID& collectionId,
                     const std::string& collectionName, const ShardID& shard,
                     const ServerID& from, const ServerID& to, bool isLeader);
};

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

  // named constructor with named parameters
  static FixServerOrderOperation create(
      const tagged_argument<tag::database, DatabaseID> database,
      const tagged_argument<tag::collectionId, CollectionID> collectionId,
      const tagged_argument<tag::collectionName, std::string> collectionName,
      const tagged_argument<tag::protoCollectionId, CollectionID>
          protoCollectionId,
      const tagged_argument<tag::protoCollectionName, std::string>
          protoCollectionName,
      const tagged_argument<tag::shard, ShardID> shard,
      const tagged_argument<tag::protoShard, ShardID> protoShard,
      const tagged_argument<tag::leader, ServerID> leader,
      const tagged_argument<tag::followers, std::vector<ServerID>> followers,
      const tagged_argument<tag::protoFollowers, std::vector<ServerID>>
          protoFollowers) {
    return FixServerOrderOperation(
        database.value, collectionId.value, collectionName.value,
        protoCollectionId.value, protoCollectionName.value, shard.value,
        protoShard.value, leader.value, followers.value, protoFollowers.value);
  }

 private:
  FixServerOrderOperation(const DatabaseID& database,
                          const CollectionID& collectionId,
                          const std::string& collectionName,
                          const CollectionID& protoCollectionId,
                          const std::string& protoCollectionName,
                          const ShardID& shard, const ShardID& protoShard,
                          const ServerID& leader,
                          const std::vector<ServerID>& followers,
                          const std::vector<ServerID>& protoFollowers);
};

bool operator==(BeginRepairsOperation const& left,
                BeginRepairsOperation const& right);

bool operator==(FinishRepairsOperation const& left,
                FinishRepairsOperation const& right);

bool operator==(MoveShardOperation const& left,
                MoveShardOperation const& right);

bool operator==(FixServerOrderOperation const& left,
                FixServerOrderOperation const& right);

std::ostream& operator<<(std::ostream& ostream,
                         BeginRepairsOperation const& operation);

std::ostream& operator<<(std::ostream& ostream,
                         FinishRepairsOperation const& operation);

std::ostream& operator<<(std::ostream& ostream,
                         MoveShardOperation const& operation);

std::ostream& operator<<(std::ostream& ostream,
                         FixServerOrderOperation const& operation);

using RepairOperation =
    boost::variant<BeginRepairsOperation const, FinishRepairsOperation const,
                   MoveShardOperation const, FixServerOrderOperation const>;

std::string getTypeAsString(RepairOperation const& op);

std::ostream& operator<<(std::ostream& ostream,
                         RepairOperation const& operation);

// Converts any RepairOperation to a Transaction. If its a job (i.e. put in
// Target/ToDo/), it returns the corresponding job id as well.
class RepairOperationToTransactionVisitor
    : public boost::static_visitor<
          std::pair<AgencyWriteTransaction, boost::optional<uint64_t>>> {
  using ReturnValueT =
      std::pair<AgencyWriteTransaction, boost::optional<uint64_t>>;

 public:
  RepairOperationToTransactionVisitor();
  RepairOperationToTransactionVisitor(
      std::function<uint64_t()> getJobId,
      std::function<std::chrono::system_clock::time_point()>
          getJobCreationTimestamp);

  ReturnValueT operator()(BeginRepairsOperation const& op);

  ReturnValueT operator()(FinishRepairsOperation const& op);

  ReturnValueT operator()(MoveShardOperation const& op);

  ReturnValueT operator()(FixServerOrderOperation const& op);

 private:
  std::vector<VPackBufferPtr> _vpackBufferArray;
  std::function<uint64_t()> _getJobId;
  std::function<std::chrono::system_clock::time_point()>
      _getJobCreationTimestamp;

  std::vector<VPackBufferPtr>&& steal();

  std::string agencyCollectionId(DatabaseID database,
                                 CollectionID collection) const;

  VPackBufferPtr createShardDbServerArray(ServerID const& leader,
                                          DBServers const& followers) const;

  template <typename T>
  VPackSlice createSingleValueVpack(T val);
};

// Adds any RepairOperation to a VPack as an object, suitable for users to see.
// Doesn't contain all data, some members are named differently.
// TODO Maybe it would still be good to add all members, at least for the
// functional tests?
class RepairOperationToVPackVisitor : public boost::static_visitor<void> {
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
}
}

#endif  // ARANGODB3_CLUSTERREPAIROPERATIONS_H
