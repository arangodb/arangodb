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
using VPackBufferPtr = std::shared_ptr<VPackBuffer<uint8_t>>;

template<typename T>
class ResultT : public arangodb::Result {
 public:

  ResultT static success(T val) {
    return ResultT(val, TRI_ERROR_NO_ERROR);
  }

  ResultT static error(int errorNumber) {
    return ResultT(boost::none, errorNumber);
  }

  ResultT static error(int errorNumber, std::__cxx11::string const &errorMessage) {
    return ResultT(boost::none, errorNumber, errorMessage);
  }

  ResultT(Result const& other)
    : Result(other) {
    // .ok() is not allowed here, as _val should be expected to be initialized
    // iff .ok() is true.
    TRI_ASSERT(other.fail());
  }

  ResultT(T const& val)
    : ResultT(val, TRI_ERROR_NO_ERROR) { }

  ResultT() = delete;

  ResultT &operator=(T const &val_) {
    _val = val_;
    return *this;
  }

  ResultT &operator=(T&& val_) {
    _val = std::move(val_);
    return *this;
  }

// These are very convenient, but also make it very easy to accidentally use
// the value of an error-result...
//
//  operator T() const { return get(); }
//  operator T &() { return get(); }

  T *operator->() { return &get(); }
  T const *operator->() const { return &get(); }

  T &operator*() &{ return get(); }
  T const &operator*() const &{ return get(); }

  T&& operator*() && { return get(); }
  T const&& operator*() const && { return get(); }

  explicit operator bool() const {
    return ok();
  }

  T const& get() const {
    return _val.get();
  }
  T& get() {
    return _val.get();
  }

 protected:
  boost::optional<T> _val;

  ResultT(boost::optional<T> val_, int errorNumber)
    : Result(errorNumber), _val(val_) {}

  ResultT(boost::optional<T> val_, int errorNumber, std::__cxx11::string const &errorMessage)
    : Result(errorNumber, errorMessage), _val(val_) {}
};


class VersionSort {
  using CharOrInt = boost::variant<char, uint64_t>;

 public:

  bool operator()(std::string const &a, std::string const &b) const;

 private:

  std::vector<CharOrInt> static splitVersion(std::string const &str);
};


struct Collection {
  DatabaseID database;
  std::string name;
  CollectionID id;
  uint64_t replicationFactor;
  bool deleted;
  bool isSmart;
  boost::optional<CollectionID> distributeShardsLike;
  boost::optional<CollectionID> repairingDistributeShardsLike;
  std::map<ShardID, DBServers, VersionSort> shardsById;

  std::string inline fullName() const {
    return this->database + "/" + this->name;
  }

  std::string inline agencyCollectionId() const {
    return "Plan/Collections/" + this->database + "/" + this->id;
  }

  VPackBufferPtr
  createShardDbServerArray(
    ShardID const &shardId
  ) const;

  Collection() = delete;
};

struct MoveShardOperation {
  DatabaseID database;
  CollectionID collection;
  ShardID shard;
  ServerID from;
  ServerID to;
  bool isLeader;

  MoveShardOperation() = delete;

  VPackBufferPtr
  toVpackTodo(uint64_t jobId) const;
};

bool operator==(MoveShardOperation const &left, MoveShardOperation const &other);
std::ostream& operator<<(std::ostream& ostream, MoveShardOperation const& operation);

using RepairOperation = boost::variant<MoveShardOperation, AgencyWriteTransaction>;

class DistributeShardsLikeRepairer {
 public:
  ResultT<std::list<RepairOperation>> repairDistributeShardsLike(
    velocypack::Slice const& planCollections,
    velocypack::Slice const& supervisionHealth
  );

 private:
  std::vector< VPackBufferPtr > _vPackBuffers;

  std::map<ShardID, DBServers, VersionSort> static
  readShards(velocypack::Slice const& shards);

  DBServers static
  readDatabases(velocypack::Slice const& planDbServers);

  ResultT<std::map<CollectionID, struct Collection>> static
  readCollections(velocypack::Slice const& collectionsByDatabase);

  boost::optional<ServerID const> static
  findFreeServer(
    DBServers const& availableDbServers,
    DBServers const& shardDbServers
  );

  ResultT<std::vector<CollectionID>> static
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

  ResultT<std::list<RepairOperation>>
  fixLeader(
    DBServers const& availableDbServers,
    Collection& collection,
    Collection& proto,
    ShardID const& shardId,
    ShardID const& protoShardId
  );

  ResultT<std::list<RepairOperation>>
  fixShard(
    DBServers const& availableDbServers,
    Collection& collection,
    Collection& proto,
    ShardID const& shardId,
    ShardID const& protoShardId
  );

  ResultT<boost::optional<AgencyWriteTransaction>>
  createFixServerOrderTransaction(
    Collection& collection,
    Collection const& proto,
    ShardID const& shardId,
    ShardID const& protoShardId
  );

  AgencyWriteTransaction
  createRenameAttributeTransaction(
    Collection const& collection,
    velocypack::Slice const& value,
    std::string const& from,
    std::string const& to
  );

  ResultT<AgencyWriteTransaction>
  createRenameDistributeShardsLikeAttributeTransaction(
    Collection &collection
  );

  ResultT<AgencyWriteTransaction>
  createRestoreDistributeShardsLikeAttributeTransaction(
    Collection &collection
  );
};


}
}

#endif  // ARANGODB3_CLUSTERREPAIRS_H
