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
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/velocypack-common.h>

#include <boost/optional.hpp>
#include <boost/variant.hpp>

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace cluster_repairs {

using DBServers = std::vector<std::string>;
using CollectionId = std::string;

// TODO add tests
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

  std::string database;
  std::string name;
  std::string id;
  uint64_t replicationFactor;
  boost::optional<CollectionId const> distributeShardsLike;
  boost::optional<CollectionId const> repairingDistributeShardsLike;
  boost::optional<bool> repairingDistributeShardsLikeReplicationFactorReduced;
  std::map<std::string, DBServers, VersionSort> shardsByName;

  std::map<std::string, VPackSlice> residualAttributes;

  std::string inline agencyCollectionId() {
    return "Plan/Collections/" + this->database + "/" + this->id;
  }

  std::shared_ptr<VPackBuffer<uint8_t>>
  createShardDbServerArray(
    std::string const &shardId
  );

  // maybe more?
  // isSystem
  // numberOfShards
  // deleted
};


class DistributeShardsLikeRepairer {
 public:
  std::list<AgencyWriteTransaction> repairDistributeShardsLike(
    VPackSlice const& planCollections, VPackSlice const& planDbServers
  );

 private:
  std::vector< std::shared_ptr<VPackBuffer<uint8_t>> > _vPackBuffers;

  std::map<std::string, DBServers, VersionSort> static
  readShards(VPackSlice const& shards);

  DBServers static
  readDatabases(VPackSlice const& planDbServers);

  std::map<CollectionId, struct Collection> static
  readCollections(VPackSlice const& collectionsByDatabase);

  boost::optional<std::string const> static
  findFreeServer(
    DBServers const& availableDbServers,
    DBServers const& shardDbServers
  );

  std::vector<CollectionId> static
  findCollectionsToFix(std::map<CollectionId, struct Collection> collections);

  DBServers static serverSetDifference(
    DBServers availableDbServers,
    DBServers shardDbServers
  );

  AgencyWriteTransaction
  createMoveShardTransaction(
    Collection& collection,
    std::string const& shardId,
    std::string const& fromServerId,
    std::string const& toServerId
  );

  std::list<AgencyWriteTransaction>
  fixLeader(
    DBServers const& availableDbServers,
    Collection& collection,
    Collection& proto,
    std::string const& shardId,
    std::string const& protoShardId
  );

  std::list<AgencyWriteTransaction>
  fixShard(
    DBServers const& availableDbServers,
    Collection& collection,
    Collection& proto,
    std::string const& shardId,
    std::string const& protoShardId
  );

  boost::optional<AgencyWriteTransaction>
  createFixServerOrderTransaction(
    Collection& collection,
    Collection const& proto,
    std::string const& shardId,
    std::string const& protoShardId
  );
};


}
}

#endif  // ARANGODB3_CLUSTERREPAIRS_H
