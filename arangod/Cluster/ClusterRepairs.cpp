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

#include "ClusterRepairs.h"

#include "Logger/Logger.h"
#include "velocypack/Iterator.h"

#include <boost/optional.hpp>
#include <boost/range/combine.hpp>
#include <boost/variant.hpp>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::cluster_repairs;

bool VersionSort::operator()(std::string const &a, std::string const &b) const {
  std::vector<CharOrInt> va = splitVersion(a);
  std::vector<CharOrInt> vb = splitVersion(b);

  return std::lexicographical_compare(
    va.begin(), va.end(),
    vb.begin(), vb.end(),
    [](CharOrInt const &a, CharOrInt const &b) -> bool {
      if (a.which() != b.which()) {
        return a.which() < b.which();
      }
      return a < b;
    }
  );
}

std::vector<VersionSort::CharOrInt>
VersionSort::splitVersion(std::string const &str) {
  size_t from = std::string::npos;
  size_t to = std::string::npos;

  std::vector<CharOrInt> result;

  for (size_t pos = 0; pos < str.length(); pos++) {
    if (isdigit(str[pos])) {
      if (from == std::string::npos) {
        from = pos;
      }
      to = pos;
    } else if (to != std::string::npos) {
      result.emplace_back(std::stoul(str.substr(from, to)));
      from = to = std::string::npos;
    } else {
      result.emplace_back(str[pos]);
    }
  }

  if (to != std::string::npos) {
    result.emplace_back(std::stoul(str.substr(from, to)));
  }

  return result;
}


std::shared_ptr<VPackBuffer<uint8_t>>
cluster_repairs::Collection::createShardDbServerArray(
  ShardID const &shardId
) const {
  VPackBuilder builder;

  builder.add(Value(ValueType::Array));

  for (auto const& it : shardsById.at(shardId)) {
    builder.add(Value(it));
  }

  builder.close();

  return builder.steal();
}

std::map<ShardID, DBServers, VersionSort>
DistributeShardsLikeRepairer::readShards(Slice const& shards) {
  std::map<ShardID, DBServers, VersionSort> shardsById;

  for (auto const &shardIterator : ObjectIterator(shards)) {
    ShardID const shardId = shardIterator.key.copyString();

    DBServers dbServers;

    for (auto const &dbServerIterator : ArrayIterator(shardIterator.value)) {
      ServerID const dbServerId = dbServerIterator.copyString();

      dbServers.emplace_back(dbServerId);
    }

    shardsById.emplace(std::make_pair(shardId, std::move(dbServers)));
  }

  return shardsById;
}

DBServers
DistributeShardsLikeRepairer::readDatabases(
  const Slice &supervisionHealth
) {
  DBServers dbServers;

  for (auto const &it : ObjectIterator(supervisionHealth)) {
    ServerID const &serverId = it.key.copyString();
    if (serverId.substr(0, 5) == "PRMR-"
        && it.value.hasKey("Status")
        && it.value.get("Status").copyString() == "GOOD") {
      dbServers.emplace_back(it.key.copyString());
    }
  }

  return dbServers;
}


std::map<CollectionID, struct cluster_repairs::Collection>
DistributeShardsLikeRepairer::readCollections(
  const Slice &collectionsByDatabase
) {
  std::map<CollectionID, struct Collection> collections;

  // maybe extract more fields, like
  // "Lock": "..."
  // "DBServers": {...}
  // ?

  for (auto const &databaseIterator : ObjectIterator(collectionsByDatabase)) {
    DatabaseID const databaseId = databaseIterator.key.copyString();

    Slice const &collectionsSlice = databaseIterator.value;

    for (auto const &collectionIterator : ObjectIterator(collectionsSlice)) {
      CollectionID const collectionId = collectionIterator.key.copyString();
      Slice const &collectionSlice = collectionIterator.value;

      // attributes
      std::string collectionName;
      uint64_t replicationFactor;
      boost::optional<CollectionID const> distributeShardsLike;
      boost::optional<CollectionID const> repairingDistributeShardsLike;
      Slice shardsSlice;
      std::map<std::string, Slice> residualAttributes;

      for (auto const& it : ObjectIterator(collectionSlice)) {
        std::string const& key = it.key.copyString();
        if (key == "name") {
          collectionName = it.value.copyString();
        }
        else if (key == "id") {
          std::string const id = it.value.copyString();
          if (id != collectionId) {
            // TODO throw a meaningful exception
            THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
          }
        }
        else if (key == "replicationFactor") {
          replicationFactor = it.value.getUInt();
        }
        else if(key == "distributeShardsLike") {
          distributeShardsLike.emplace(it.value.copyString());
        }
        else if(key == "repairingDistributeShardsLike") {
          repairingDistributeShardsLike.emplace(it.value.copyString());
        }
        else if(key == "shards") {
          shardsSlice = it.value;
        }
        else {
          residualAttributes.emplace(std::make_pair(key, it.value));
        }
      }

      std::map<ShardID, DBServers, VersionSort> shardsById
        = readShards(shardsSlice);

      struct Collection collection {
        collectionSlice,
        databaseId,
        collectionName,
        collectionId,
        replicationFactor,
        distributeShardsLike,
        repairingDistributeShardsLike,
        boost::none,
        std::move(shardsById),
        std::move(residualAttributes)
      };

      collections.emplace(std::make_pair(collectionId, std::move(collection)));
    }

  }

  return collections;
}


std::vector<CollectionID>
DistributeShardsLikeRepairer::findCollectionsToFix(
  std::map<CollectionID, struct Collection> collections
) {
  LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::findCollectionsToFix: started";

  std::vector<CollectionID> collectionsToFix;

  // TODO Are there additional criteria when a collection shouldn't be fixed?
  // e.g. deleted: true, some status, isSystem, ...?

  for (auto const &collectionIterator : collections) {
    CollectionID const& collectionId = collectionIterator.first;
    struct Collection const& collection = collectionIterator.second;

    LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
    << "findCollectionsToFix: checking collection " << collection.fullName();

    if (collection.repairingDistributeShardsLike) {
      // TODO Reduce loglevel and rewrite message as soon as this is tested
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "findCollectionsToFix: repairingDistributeShardsLike exists, adding "
      << collection.fullName();
      collectionsToFix.emplace_back(collectionId);
      continue;
    }
    if (! collection.distributeShardsLike) {
      LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
      << "findCollectionsToFix: distributeShardsLike doesn't exist, not fixing "
      << collection.fullName();
      continue;
    }

    struct Collection& proto = collections[collection.distributeShardsLike.get()];

    LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
    << "findCollectionsToFix: comparing against distributeShardsLike collection "
    << proto.fullName();

    if (collection.shardsById.size() != proto.shardsById.size()) {
      // TODO This should maybe not be a warning.
      // TODO Is there anything we can do in this case? Why does this happen anyway?

      // answer: This should only happen if collection has "isSmart": true. In
      // that case, the number of shards should be 0.
      LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
      << "Unequal number of shards in collection " << collection.fullName()
      << " and its distributeShardsLike collection " << proto.fullName();

      continue;
    }

    for (auto const& zippedShardsIt : boost::combine(collection.shardsById, proto.shardsById)) {
      auto const& shardIt = zippedShardsIt.get<0>();
      auto const& protoShardIt = zippedShardsIt.get<1>();

      DBServers const& dbServers = shardIt.second;
      DBServers const& protoDbServers = protoShardIt.second;

      LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
      << "findCollectionsToFix: comparing shards " << shardIt.first << " and " << protoShardIt.first;

      if (dbServers != protoDbServers) {
        LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
        << "findCollectionsToFix: collection "
        << collection.fullName() << " needs fixing because (at least) shard "
        << shardIt.first << " differs from "
        << protoShardIt.first << " in " << proto.fullName();
        collectionsToFix.emplace_back(collectionId);
        break;
      }
    }
  }

  return collectionsToFix;
}

boost::optional<ServerID const>
DistributeShardsLikeRepairer::findFreeServer(
  DBServers const& availableDbServers,
  DBServers const& shardDbServers
) {
  DBServers freeServer = serverSetDifference(availableDbServers, shardDbServers);

  // TODO use random server
  if (!freeServer.empty()) {
    return freeServer[0];
  }

  return boost::none;
}

DBServers DistributeShardsLikeRepairer::serverSetDifference(
  DBServers setA,
  DBServers setB
) {
  sort(setA.begin(), setA.end());
  sort(setB.begin(), setB.end());

  DBServers difference;

  set_difference(
    setA.begin(), setA.end(),
    setB.begin(), setB.end(),
    back_inserter(difference)
  );

  return difference;
}


DBServers
DistributeShardsLikeRepairer::serverSetSymmetricDifference(
  DBServers setA,
  DBServers setB
) {
  sort(setA.begin(), setA.end());
  sort(setB.begin(), setB.end());

  DBServers symmetricDifference;

  std::set_symmetric_difference(
    setA.begin(), setA.end(),
    setB.begin(), setB.end(),
    back_inserter(symmetricDifference)
  );

  return symmetricDifference;
}


MoveShardOperation
DistributeShardsLikeRepairer::createMoveShardOperation(
  Collection& collection,
  ShardID const& shardId,
  ServerID const& fromServerId,
  ServerID const& toServerId,
  bool isLeader
) {
  MoveShardOperation moveShardOperation = {
    collection.database,
    collection.id,
    shardId,
    fromServerId,
    toServerId,
    isLeader
  };

  // "Move" the shard in `collection`
  for (auto &it : collection.shardsById.at(shardId)) {
    if (it == fromServerId) {
      it = toServerId;
    }
  }

  return moveShardOperation;
}


std::list<RepairOperation>
DistributeShardsLikeRepairer::fixLeader(
  DBServers const& availableDbServers,
  Collection& collection,
  Collection& proto,
  ShardID const& shardId,
  ShardID const& protoShardId
) {
  LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::fixLeader("
  << "\"" << collection.database << "/" << collection.name << "\","
  << "\"" << proto.database << "/" << proto.name << "\","
  << "\"" << shardId << "/" << protoShardId << "\","
  << ")";

  DBServers const& protoShardDbServers = proto.shardsById.at(protoShardId);
  DBServers& shardDbServers = collection.shardsById.at(shardId);

  ServerID const& protoLeader = protoShardDbServers.front();
  ServerID const& shardLeader = shardDbServers.front();

  std::list<RepairOperation> repairOperations;

//[PSEUDO-TODO] didReduceRepl := false

  if (protoLeader == shardLeader) {
    return {};
  }

  if (collection.replicationFactor == availableDbServers.size()) {
    // The replicationFactor should have been reduced before calling this method
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_ENOUGH_HEALTHY);
  }
  
  if (std::find(shardDbServers.begin(), shardDbServers.end(), protoLeader) != shardDbServers.end()) {
    boost::optional<ServerID const> tmpServer = findFreeServer(availableDbServers, shardDbServers);

    if (! tmpServer) {
      // This happens if all db servers that don't contain the shard are unhealthy
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_ENOUGH_HEALTHY);
    }

    RepairOperation moveShardOperation = createMoveShardOperation(
      collection,
      shardId,
      protoLeader,
      tmpServer.get(),
      false
    );

    repairOperations.emplace_back(moveShardOperation);
  }

  RepairOperation moveShardOperation = createMoveShardOperation(
    collection,
    shardId,
    shardLeader,
    protoLeader,
    true
  );

  repairOperations.emplace_back(moveShardOperation);

  return repairOperations;
}


std::list<RepairOperation>
DistributeShardsLikeRepairer::fixShard(
  DBServers const& availableDbServers,
  Collection& collection,
  Collection& proto,
  ShardID const& shardId,
  ShardID const& protoShardId
) {
  LOG_TOPIC(INFO, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::fixShard: "
  << "Fixing DBServers"
  << " on shard " << shardId
  << " of collection " << collection.fullName()
  << " to match shard " << protoShardId
  << " of collection " << proto.fullName();

  std::list<RepairOperation> repairOperations;
  repairOperations = fixLeader(availableDbServers, collection, proto, shardId, protoShardId);

  DBServers const& protoShardDbServers = proto.shardsById.at(protoShardId);
  DBServers& shardDbServers = collection.shardsById.at(shardId);

  DBServers serversOnlyOnProto
    = serverSetDifference(protoShardDbServers, shardDbServers);

  DBServers serversOnlyOnShard
    = serverSetDifference(shardDbServers, protoShardDbServers);

  if (serversOnlyOnProto.size() != serversOnlyOnShard.size()) {
// [PSEUDO]  If (onProto.length != onFollower.length) { fail(); } // Here the replicationFactor is violated. Will not fix
    // TODO write a test and throw a meaningful exception
    THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
  }

  for (auto const& zipIt : boost::combine(serversOnlyOnProto, serversOnlyOnShard)) {
    auto const& protoServerIt = zipIt.get<0>();
    auto const& shardServerIt = zipIt.get<1>();

    RepairOperation moveShardOperation =
      createMoveShardOperation(collection, shardId, shardServerIt, protoServerIt, false);
    repairOperations.emplace_back(moveShardOperation);

// [PSEUDO-TODO]    if (onProto.length >= col.replicationFactor - 2 && i == 0) {
// [PSEUDO-TODO]      // Security that we at least keep one insync follower
// [PSEUDO-TODO]      waitForSync();
// [PSEUDO-TODO]    }
  }

  if(auto trx = createFixServerOrderTransaction(collection, proto, shardId, protoShardId)) {
    repairOperations.emplace_back(trx.get());
  }

  return repairOperations;
}


std::list<RepairOperation>
DistributeShardsLikeRepairer::repairDistributeShardsLike(
  Slice const& planCollections,
  Slice const& supervisionHealth
) {
  LOG_TOPIC(INFO, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
  << "Starting to collect neccessary repairs";

  // Needed to build agency transactions
  TRI_ASSERT(AgencyCommManager::MANAGER != nullptr);

  std::map<CollectionID, struct Collection> collectionMap = readCollections(planCollections);
  DBServers const availableDbServers = readDatabases(supervisionHealth);

  std::vector<CollectionID> collectionsToFix = findCollectionsToFix(collectionMap);

  std::list<RepairOperation> repairOperations;

  for (auto const& collectionIdIterator : collectionsToFix) {
    struct Collection& collection = collectionMap[collectionIdIterator];
    LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::repairDistributeShardsLike: fixing collection "
    << collection.fullName();

    // TODO rename distributeShardsLike to repairingDistributeShardsLike in
    // collection
    ShardID protoId;
    if (collection.distributeShardsLike) {
      protoId = collection.distributeShardsLike.get();
    }
    else if (collection.repairingDistributeShardsLike) {
      protoId = collection.repairingDistributeShardsLike.get();
    }

    struct Collection& proto = collectionMap[protoId];

    for (auto const& zippedShardsIterator : boost::combine(collection.shardsById, proto.shardsById)) {
      auto const &shardIterator = zippedShardsIterator.get<0>();
      auto const &protoShardIterator = zippedShardsIterator.get<1>();

      ShardID const& shardId = shardIterator.first;
      ShardID const& protoShardId = protoShardIterator.first;

      DBServers const &dbServers = shardIterator.second;
      DBServers const &protoDbServers = protoShardIterator.second;

      if (dbServers != protoDbServers) {
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
        << "fixing shard " << collection.fullName() << "/" << shardId;
        // TODO Do we need to check that dbServers and protoDbServers are not empty?
        // TODO Do we need to check that dbServers and protoDbServers are of equal size?
        std::list<RepairOperation> newRepairOperations
          = fixShard(availableDbServers, collection, proto, shardId, protoShardId);
        repairOperations.splice(repairOperations.end(), newRepairOperations);
      }
      else {
        LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
        << "shard " << collection.fullName() << "/" << shardId
        << " doesn't need fixing";
      }
    }

    // TODO rename repairingDistributeShardsLike to distributeShardsLike in
    // TODO collection. Use preconditions to guarantee that everything was fixed.
  }

  return repairOperations;
}

boost::optional<AgencyWriteTransaction>
DistributeShardsLikeRepairer::createFixServerOrderTransaction(
  Collection& collection,
  Collection const& proto,
  ShardID const& shardId,
  ShardID const& protoShardId
) {
  std::string const agencyShardId
    = collection.agencyCollectionId() + "/shards/" + shardId;

  LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::createFixServerOrderTransaction: "
  << "Fix DBServer order on " << collection.fullName() << "/"  << shardId
  << " to match " << proto.fullName() << "/"  << protoShardId;

  DBServers& dbServers = collection.shardsById.at(shardId);
  DBServers const& protoDbServers = proto.shardsById.at(protoShardId);

  // TODO This test may not be necessary, as the symmetric set difference
  // TODO should catch this case. Keep it or remove it?
  if (dbServers.size() != protoDbServers.size()) {
    // TODO this should never happen. throw a meaningful exception.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
  }

  if (dbServers.size() == 0) {
    // TODO this should never happen. return an empty transaction or throw a meaningful exception?
    THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
  }

  if (dbServers[0] != protoDbServers[0]) {
    // TODO this should never happen. throw a meaningful exception.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
  }

  if (!serverSetSymmetricDifference(dbServers, protoDbServers).empty()) {
    // TODO this should never happen. throw a meaningful exception.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
  }

  if (dbServers == protoDbServers) {
    LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::createFixServerOrderTransaction: "
    << "Order is already equal, doing nothing";

    return boost::none;
  }

  std::shared_ptr<VPackBuffer<uint8_t>>
    vpack = collection.createShardDbServerArray(shardId);
  VPackSlice oldDbServerSlice = Slice(vpack->data());
  _vPackBuffers.emplace_back(std::move(vpack));

  AgencyPrecondition agencyPrecondition {
    agencyShardId,
    AgencyPrecondition::Type::VALUE,
    oldDbServerSlice
  };

  dbServers = protoDbServers;

  vpack = collection.createShardDbServerArray(shardId);
  VPackSlice newDbServerSlice = Slice(vpack->data());
  _vPackBuffers.emplace_back(std::move(vpack));

  AgencyOperation agencyOperation {
    agencyShardId,
    AgencyValueOperationType::SET,
    newDbServerSlice
  };

  return AgencyWriteTransaction { agencyOperation, agencyPrecondition };
}
