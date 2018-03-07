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

template<typename T, std::size_t N>
std::ostream& operator<<(std::ostream& stream, std::array<T, N> array) {
  stream << "std::array<" << typeid(T).name() << "> { ";
  std::for_each(
    array.begin(),
    array.end(),
    [&stream](T const &val) {
      stream << val << ", ";
    }
  );

  stream << "}";

  return stream;
}

inline std::ostream& operator<<(std::ostream& stream, VPackBufferPtr vpack) {
  return stream << "std::shared_ptr<VPackBuffer> { "
                << velocypack::Slice(vpack->data()).toJson() << " "
                << "}";
}

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

  ResultT map(ResultT<T> (*fun)(T const& val)) const {
    if (ok()) {
      return ResultT<T>::success(fun(get()));
    }

    return *this;
  }

  bool operator==(ResultT<T> const& other) const {
    if (this->ok() && other.ok()) {
      return this->get() == other.get();
    }
    if (this->fail() && other.fail()) {
      return this->errorNumber() == other.errorNumber()
             && this->errorMessage() == other.errorMessage();
    }

    return false;
  }

 protected:
  boost::optional<T> _val;

  ResultT(boost::optional<T> val_, int errorNumber)
    : Result(errorNumber), _val(val_) {}

  ResultT(boost::optional<T> val_, int errorNumber, std::__cxx11::string const &errorMessage)
    : Result(errorNumber, errorMessage), _val(val_) {}
};


template<typename T>
std::ostream& operator<<(std::ostream& stream, ResultT<T> const& result) {
  return stream
    << "ResultT<" << typeid(T).name() << "> "
    << ": Result { "
    << "errorNumber = " << result.errorNumber() << ", "
    << "errorMessage = \"" << result.errorMessage() << "\" "
    << "} { "
    << "val = " << result.get() << " "
    << "}";
}


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
};

struct FinishRepairsOperation {
  DatabaseID database;
  CollectionID collectionId;
  std::string collectionName;
  CollectionID protoCollectionId;
  std::string protoCollectionName;
  size_t replicationFactor;

  FinishRepairsOperation() = delete;
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

  VPackBufferPtr
  toVpackTodo(uint64_t jobId) const;
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
};


bool operator==(BeginRepairsOperation const &left, BeginRepairsOperation const &right);
bool operator==(FinishRepairsOperation const &left, FinishRepairsOperation const &right);
bool operator==(MoveShardOperation const &left, MoveShardOperation const &right);
bool operator==(FixServerOrderOperation const &left, FixServerOrderOperation const &right);

std::ostream& operator<<(std::ostream& ostream, BeginRepairsOperation const& operation);
std::ostream& operator<<(std::ostream& ostream, FinishRepairsOperation const& operation);
std::ostream& operator<<(std::ostream& ostream, MoveShardOperation const& operation);
std::ostream& operator<<(std::ostream& ostream, FixServerOrderOperation const& operation);

using RepairOperation = boost::variant<
  BeginRepairsOperation const,
  FinishRepairsOperation const,
  MoveShardOperation const,
  FixServerOrderOperation const
>;

std::ostream& operator<<(std::ostream& ostream, RepairOperation const& operation);

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

  ResultT<boost::optional<FixServerOrderOperation>>
  createFixServerOrderOperation(
    Collection &collection,
    Collection const &proto,
    ShardID const &shardId,
    ShardID const &protoShardId
  );

  ResultT<BeginRepairsOperation>
  createBeginRepairsOperation(
    Collection &collection,
    Collection const &proto
  );

  ResultT<FinishRepairsOperation>
  createFinishRepairsOperation(
    Collection &collection,
    Collection const &proto
  );
};


class RepairOperationToTransactionVisitor
  : public boost::static_visitor<
    std::pair<AgencyWriteTransaction, boost::optional<uint64_t>>
  > {
  using ReturnValueT = std::pair<AgencyWriteTransaction, boost::optional<uint64_t>>;

 public:
  std::vector<VPackBufferPtr> vpackBufferArray;

  ReturnValueT
  operator()(BeginRepairsOperation const& op);

  ReturnValueT
  operator()(FinishRepairsOperation const& op);

  ReturnValueT
  operator()(MoveShardOperation const& op);

  ReturnValueT
  operator()(FixServerOrderOperation const& op);


 private:
  std::string agencyCollectionId(
    DatabaseID database,
    CollectionID collection
  ) const;


  VPackBufferPtr
  createShardDbServerArray(
    ServerID const& leader,
    DBServers const& followers
  ) const;

};

}
}

#endif  // ARANGODB3_CLUSTERREPAIRS_H
