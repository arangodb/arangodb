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

#include <arangod/Agency/AgencyComm.h>
#include <velocypack/Compare.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-common.h>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include "ClusterInfo.h"
#include "ClusterRepairOperations.h"

namespace arangodb {
namespace velocypack {
class Slice;
template <typename T>
class Buffer;
}

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

inline std::ostream& operator<<(std::ostream& stream,
                                VPackBufferPtr const& vpack) {
  return stream << "std::shared_ptr<VPackBuffer> { "
                << velocypack::Slice(vpack->data()).toJson() << " "
                << "}";
}

template <typename T>
class ResultT : public arangodb::Result {
 public:
  ResultT static success(T val) { return ResultT(val, TRI_ERROR_NO_ERROR); }

  ResultT static error(int errorNumber) {
    return ResultT(boost::none, errorNumber);
  }

  ResultT static error(int errorNumber, std::string const& errorMessage) {
    return ResultT(boost::none, errorNumber, errorMessage);
  }

  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  ResultT(Result const& other) : Result(other) {
    // .ok() is not allowed here, as _val should be expected to be initialized
    // iff .ok() is true.
    TRI_ASSERT(other.fail());
  }

  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  ResultT(T&& val) : ResultT(std::forward<T>(val), TRI_ERROR_NO_ERROR) {}

  ResultT() = delete;

  ResultT& operator=(T const& val_) {
    _val = val_;
    return *this;
  }

  ResultT& operator=(T&& val_) {
    _val = std::move(val_);
    return *this;
  }

  // These are very convenient, but also make it very easy to accidentally use
  // the value of an error-result...
  //
  //  operator T() const { return get(); }
  //  operator T &() { return get(); }

  T* operator->() { return &get(); }
  T const* operator->() const { return &get(); }

  T& operator*() & { return get(); }
  T const& operator*() const& { return get(); }

  T&& operator*() && { return get(); }
  T const&& operator*() const&& { return get(); }

  explicit operator bool() const { return ok(); }

  T const& get() const { return _val.get(); }
  T& get() { return _val.get(); }

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
      return this->errorNumber() == other.errorNumber() &&
             this->errorMessage() == other.errorMessage();
    }

    return false;
  }

 protected:
  boost::optional<T> _val;

  ResultT(boost::optional<T>&& val_, int errorNumber)
      : Result(errorNumber), _val(val_) {}

  ResultT(boost::optional<T>&& val_, int errorNumber,
          std::string const& errorMessage)
      : Result(errorNumber, errorMessage), _val(val_) {}
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

  Collection() = delete;

  // constructor with named parameters
  Collection(
      tagged_argument<tag::database, DatabaseID> database_,
      tagged_argument<tag::collectionId, CollectionID> collectionId_,
      tagged_argument<tag::collectionName, std::string> collectionName_,
      tagged_argument<tag::replicationFactor, uint64_t> replicationFactor_,
      tagged_argument<tag::deleted, bool> deleted_,
      tagged_argument<tag::isSmart, bool> isSmart_,
      tagged_argument<tag::distributeShardsLike, boost::optional<CollectionID>>
          distributeShardsLike_,
      tagged_argument<tag::repairingDistributeShardsLike,
                      boost::optional<CollectionID>>
          repairingDistributeShardsLike_,
      tagged_argument<tag::shardsById,
                      std::map<ShardID, DBServers, VersionSort>>
          shardsById_);
};

class DistributeShardsLikeRepairer {
 public:
  ResultT<std::map<
      CollectionID,
      ResultT<std::list<
          RepairOperation>>>> static repairDistributeShardsLike(velocypack::
                                                                    Slice const&
                                                                        planCollections,
                                                                velocypack::
                                                                    Slice const&
                                                                        supervisionHealth);

 private:
  std::map<ShardID, DBServers, VersionSort> static readShards(
      velocypack::Slice const& shards);

  DBServers static readDatabases(velocypack::Slice const& planDbServers);

  ResultT<std::map<CollectionID, struct Collection>> static readCollections(
      velocypack::Slice const& collectionsByDatabase);

  boost::optional<ServerID const> static findFreeServer(
      DBServers const& availableDbServers, DBServers const& shardDbServers);

  std::vector<std::pair<CollectionID, Result>> static findCollectionsToFix(
      std::map<CollectionID, struct Collection> collections);

  DBServers static serverSetDifference(DBServers setA, DBServers setB);

  DBServers static serverSetSymmetricDifference(DBServers setA, DBServers setB);

  MoveShardOperation static createMoveShardOperation(
      struct Collection& collection, ShardID const& shardId,
      ServerID const& fromServerId, ServerID const& toServerId, bool isLeader);

  // "proto collection" always means the collection referred to in the
  // "distributeShardsLike" attribute of "collection"

  ResultT<std::list<RepairOperation>> static fixLeader(
      DBServers const& availableDbServers, struct Collection& collection,
      struct Collection const& proto, ShardID const& shardId,
      ShardID const& protoShardId);

  ResultT<std::list<RepairOperation>> static fixShard(
      DBServers const& availableDbServers, struct Collection& collection,
      struct Collection const& proto, ShardID const& shardId,
      ShardID const& protoShardId);

  ResultT<boost::optional<FixServerOrderOperation>> static createFixServerOrderOperation(
      struct Collection& collection, struct Collection const& proto,
      ShardID const& shardId, ShardID const& protoShardId);

  ResultT<BeginRepairsOperation> static createBeginRepairsOperation(
      struct Collection& collection, struct Collection const& proto);

  std::
      vector<cluster_repairs::ShardWithProtoAndDbServers> static createShardVector(
          std::map<ShardID, DBServers, VersionSort> const& shardsById,
          std::map<ShardID, DBServers, VersionSort> const& protoShardsById);

  ResultT<FinishRepairsOperation> static createFinishRepairsOperation(
      struct Collection& collection, struct Collection const& proto);

  ResultT<std::list<RepairOperation>> static fixAllShardsOfCollection(
      struct Collection& collection, struct Collection const& proto,
      DBServers const& availableDbServers);
};
}
}

#endif  // ARANGOD_CLUSTER_CLUSTER_REPAIR_DISTRIBUTE_SHARDS_LIKE_H
