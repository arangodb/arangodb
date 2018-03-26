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

#include "ClusterRepairDistributeShardsLike.h"

#include <boost/range/combine.hpp>

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


ResultT<std::map<CollectionID, struct cluster_repairs::Collection>>
DistributeShardsLikeRepairer::readCollections(
  const Slice &collectionsByDatabase
) {
  std::map<CollectionID, struct Collection> collections;

  for (auto const &databaseIterator : ObjectIterator(collectionsByDatabase)) {
    DatabaseID const databaseId = databaseIterator.key.copyString();

    Slice const &collectionsSlice = databaseIterator.value;

    for (auto const &collectionIterator : ObjectIterator(collectionsSlice)) {
      CollectionID const collectionId = collectionIterator.key.copyString();
      Slice const &collectionSlice = collectionIterator.value;

      // attributes
      std::string collectionName;
      uint64_t replicationFactor = 0;
      bool deleted = false;
      bool isSmart = false;
      boost::optional<CollectionID> distributeShardsLike;
      boost::optional<CollectionID> repairingDistributeShardsLike;
      Slice shardsSlice;
      std::map<std::string, Slice> residualAttributes;

      for (auto const& it : ObjectIterator(collectionSlice)) {
        std::string const& key = it.key.copyString();
        if (key == "name") {
          collectionName = it.value.copyString();
        }
        else if (key == "id") {
          std::string const id = it.value.copyString();
          TRI_ASSERT(id == collectionId);
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
        else if(key == "deleted") {
          deleted = it.value.getBool();
        }
        else if(key == "isSmart") {
          isSmart = it.value.getBool();
        }
      }

      std::map<ShardID, DBServers, VersionSort> shardsById
        = readShards(shardsSlice);

      struct Collection collection {
        .database = databaseId,
        .name = collectionName,
        .id = collectionId,
        .replicationFactor = replicationFactor,
        .deleted = deleted,
        .isSmart = isSmart,
        .distributeShardsLike = distributeShardsLike,
        .repairingDistributeShardsLike = repairingDistributeShardsLike,
        .shardsById = std::move(shardsById),
      };

      collections.emplace(std::make_pair(collectionId, std::move(collection)));
    }

  }

  return collections;
}


ResultT<std::vector<CollectionID>>
DistributeShardsLikeRepairer::findCollectionsToFix(
  std::map<CollectionID, struct Collection> collections
) {
  LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::findCollectionsToFix: started";

  std::vector<CollectionID> collectionsToFix;

  for (auto const &collectionIterator : collections) {
    CollectionID const& collectionId = collectionIterator.first;
    struct Collection const& collection = collectionIterator.second;

    LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
    << "findCollectionsToFix: checking collection " << collection.fullName();

    if (collection.deleted) {
      LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
      << "findCollectionsToFix: collection " << collection.fullName()
      << " has `deleted: true` and will be ignored.";
      continue;
    }

    if (collection.repairingDistributeShardsLike) {
      LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
      << "findCollectionsToFix: repairs on collection " << collection.fullName()
      << " were already started earlier, but are unfinished "
      << "(attribute repairingDistributeShardsLike exists). "
      << "Adding it to the list of collections to fix.";
      collectionsToFix.emplace_back(collectionId);
      continue;
    }
    if (! collection.distributeShardsLike) {
      LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
      << "findCollectionsToFix: distributeShardsLike doesn't exist, not fixing "
      << collection.fullName();
      continue;
    }

    struct Collection& proto = collections.at(collection.distributeShardsLike.get());

    LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
    << "findCollectionsToFix: comparing against distributeShardsLike collection "
    << proto.fullName();

    if (collection.shardsById.size() != proto.shardsById.size()) {
      if (collection.isSmart && collection.shardsById.empty()) {
        // This case is expected: smart edge collections have no shards.
        continue;
      }

      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::findCollectionsToFix: "
      << "Unequal number of shards in collection " << collection.fullName()
      << " and its distributeShardsLike collection " << proto.fullName();

      return Result(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS);
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

  // TODO maybe use a random server instead?
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
  MoveShardOperation moveShardOperation {
    .database = collection.database,
    .collectionId = collection.id,
    .collectionName = collection.name,
    .shard = shardId,
    .from = fromServerId,
    .to = toServerId,
    .isLeader = isLeader,
  };

  // "Move" the shard in `collection`
  for (auto &it : collection.shardsById.at(shardId)) {
    if (it == fromServerId) {
      it = toServerId;
    }
  }

  return moveShardOperation;
}


ResultT<std::list<RepairOperation>>
DistributeShardsLikeRepairer::fixLeader(
  DBServers const& availableDbServers,
  Collection& collection,
  Collection const& proto,
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

  if (protoLeader == shardLeader) {
    return repairOperations;
  }

  if (collection.replicationFactor == availableDbServers.size()) {
    // The replicationFactor should have been reduced before calling this method
    return Result(TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
  }

  if (std::find(shardDbServers.begin(), shardDbServers.end(), protoLeader) != shardDbServers.end()) {
    boost::optional<ServerID const> tmpServer = findFreeServer(availableDbServers, shardDbServers);

    if (! tmpServer) {
      // This happens if all db servers that don't contain the shard are unhealthy
      return Result(TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
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


ResultT<std::list<RepairOperation>>
DistributeShardsLikeRepairer::fixShard(
  DBServers const& availableDbServers,
  Collection& collection,
  Collection const& proto,
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

  if (collection.replicationFactor != proto.replicationFactor) {
    // TODO maybe write a test
    std::stringstream errorMessage;
    errorMessage
      << "replicationFactor is violated: "
      << "Collection " << collection.fullName()
      << " and its distributeShardsLike prototype " << proto.fullName()
      << " have replicationFactors of " << collection.replicationFactor << " and "
      << proto.replicationFactor << ", respectively.";
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::fixShard: "
    << errorMessage.str();

    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
      errorMessage.str());
  }

  auto fixLeaderRepairOperationsResult = fixLeader(availableDbServers, collection, proto, shardId, protoShardId);
  if (fixLeaderRepairOperationsResult.fail()) {
    return fixLeaderRepairOperationsResult;
  }

  std::list<RepairOperation> repairOperations
    = fixLeaderRepairOperationsResult.get();

  DBServers const& protoShardDbServers = proto.shardsById.at(protoShardId);
  DBServers& shardDbServers = collection.shardsById.at(shardId);

  DBServers serversOnlyOnProto
    = serverSetDifference(protoShardDbServers, shardDbServers);

  DBServers serversOnlyOnShard
    = serverSetDifference(shardDbServers, protoShardDbServers);

  if (serversOnlyOnProto.size() != serversOnlyOnShard.size()) {
    // TODO write a test
    std::stringstream errorMessage;
    errorMessage
      << "replicationFactor is violated: "
      << "Collection " << collection.fullName()
      << " and its distributeShardsLike prototype " << proto.fullName()
      << " have " << serversOnlyOnShard.size() << " and "
      << serversOnlyOnProto.size() << " different(mismatching) "
      << " DBServers, respectively.";
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::fixShard: "
    << errorMessage.str();

    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
      errorMessage.str());
  }

  for (auto const& zipIt : boost::combine(serversOnlyOnProto, serversOnlyOnShard)) {
    auto const& protoServerIt = zipIt.get<0>();
    auto const& shardServerIt = zipIt.get<1>();

    RepairOperation moveShardOperation =
      createMoveShardOperation(collection, shardId, shardServerIt, protoServerIt, false);
    repairOperations.emplace_back(moveShardOperation);
  }

  ResultT<boost::optional<FixServerOrderOperation>> maybeFixServerOrderOperationResult
    = createFixServerOrderOperation(collection, proto, shardId, protoShardId);

  if (maybeFixServerOrderOperationResult.fail()) {
    return maybeFixServerOrderOperationResult;
  }

  if(auto const& maybeFixServerOrderOperation = maybeFixServerOrderOperationResult.get()) {
    FixServerOrderOperation const& fixServerOrderOperation = maybeFixServerOrderOperation.get();
    repairOperations.emplace_back(fixServerOrderOperation);
  }

  return repairOperations;
}


ResultT<
  std::map< CollectionID,
    ResultT<std::list<RepairOperation>>
  >
>
DistributeShardsLikeRepairer::repairDistributeShardsLike(
  Slice const &planCollections,
  Slice const &supervisionHealth
) {
  LOG_TOPIC(INFO, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
  << "Starting to collect neccessary repairs";

  // Needed to build agency transactions
  TRI_ASSERT(AgencyCommManager::MANAGER != nullptr);

  auto collectionMapResult = readCollections(planCollections);
  if (collectionMapResult.fail()) {
    return collectionMapResult;
  }
  std::map<CollectionID, struct Collection> collectionMap = collectionMapResult.get();
  DBServers const availableDbServers = readDatabases(supervisionHealth);

  auto collectionsToFixResult = findCollectionsToFix(collectionMap);
  if (collectionsToFixResult.fail()) {
    return collectionsToFixResult;
  }
  std::vector<CollectionID> collectionsToFix = collectionsToFixResult.get();

  std::map<CollectionID, ResultT<std::list<RepairOperation>>> repairOperationsByCollection;

  for (auto const& collectionIdIterator : collectionsToFix) {
    struct Collection& collection = collectionMap.at(collectionIdIterator);
    LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::repairDistributeShardsLike: fixing collection "
    << collection.fullName();

    ShardID protoId;
    if (collection.distributeShardsLike) {
      protoId = collection.distributeShardsLike.get();
    }
    else if (collection.repairingDistributeShardsLike) {
      protoId = collection.repairingDistributeShardsLike.get();
    }
    else {
      // This should never happen
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
      << "(repairing)distributeShardsLike missing in "
      << collection.fullName();
      repairOperationsByCollection.emplace(
        collection.id,
        Result { TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES }
      );
      break;
    }

    struct Collection& proto = collectionMap.at(protoId);

    auto beginRepairsOperation
      = createBeginRepairsOperation(
        collection,
        proto
      );
    if (beginRepairsOperation.fail()) {
      repairOperationsByCollection.emplace(
        collection.id,
        std::move(beginRepairsOperation)
      );
      break;
    }
    std::list<RepairOperation> repairOperations;
    repairOperations.emplace_back(
      std::move(beginRepairsOperation.get())
    );

    ResultT<std::list<RepairOperation>> shardRepairOperationsResult
      = fixAllShardsOfCollection(collection, proto, availableDbServers);

    if (shardRepairOperationsResult.fail()) {
      repairOperationsByCollection.emplace(
        collection.id,
        std::move(shardRepairOperationsResult)
      );
      break;
    }

    repairOperations.splice(repairOperations.end(), shardRepairOperationsResult.get());

    auto finishRepairsOperation
      = createFinishRepairsOperation(
        collection,
        proto
      );
    if (finishRepairsOperation.fail()) {
      repairOperationsByCollection.emplace(
        collection.id,
        std::move(finishRepairsOperation)
      );
      break;
    }
    repairOperations.emplace_back(
      std::move(finishRepairsOperation.get())
    );

    repairOperationsByCollection.emplace(
      collection.id,
      std::move(repairOperations)
    );
  }

  return repairOperationsByCollection;
}

ResultT<boost::optional<FixServerOrderOperation>>
DistributeShardsLikeRepairer::createFixServerOrderOperation(
  Collection &collection,
  Collection const &proto,
  ShardID const &shardId,
  ShardID const &protoShardId
) {
  LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
  << "DistributeShardsLikeRepairer::createFixServerOrderOperation: "
  << "Fixing DBServer order on " << collection.fullName() << "/"  << shardId
  << " to match " << proto.fullName() << "/"  << protoShardId;

  DBServers& dbServers = collection.shardsById.at(shardId);
  DBServers const& protoDbServers = proto.shardsById.at(protoShardId);

  TRI_ASSERT(dbServers.size() == protoDbServers.size());
  if (dbServers.size() != protoDbServers.size()) {
    std::stringstream errorMessage;
    errorMessage
      << "replicationFactor violated: Collection "
      << collection.fullName() << " and its distributeShardsLike "
      << " prototype have mismatching numbers of DBServers; "
      << dbServers.size() << " (on shard " << shardId << ") "
      << "and " << protoDbServers.size() << " (on shard " << protoShardId << ")"
      << ", respectively.";
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::createFixServerOrderOperation: "
    << errorMessage.str();
    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED, errorMessage.str());
  }

  TRI_ASSERT(! dbServers.empty());
  if (dbServers.empty()) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS);
  }

  TRI_ASSERT(dbServers[0] == protoDbServers[0]);
  if (dbServers[0] != protoDbServers[0]) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS);
  }
  ServerID leader = protoDbServers[0];

  TRI_ASSERT(serverSetSymmetricDifference(dbServers, protoDbServers).empty());
  if (!serverSetSymmetricDifference(dbServers, protoDbServers).empty()) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS);
  }

  if (dbServers == protoDbServers) {
    LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::createFixServerOrderOperation: "
    << "Order is already equal, doing nothing";

    return { boost::none };
  }

  FixServerOrderOperation fixServerOrderOperation {
    .database = collection.database,
    .collectionId = collection.id,
    .collectionName = collection.name,
    .protoCollectionId = proto.id,
    .protoCollectionName = proto.name,
    .shard = shardId,
    .protoShard = protoShardId,
    .leader = leader,
    .followers = DBServers { dbServers.begin() + 1, dbServers.end() },
    .protoFollowers = DBServers { protoDbServers.begin() + 1, protoDbServers.end() },
  };

  // Change order for the rest of the repairs as well
  dbServers = protoDbServers;

  return { fixServerOrderOperation };
}


ResultT<BeginRepairsOperation>
DistributeShardsLikeRepairer::createBeginRepairsOperation(
  Collection &collection,
  Collection const &proto
) {
  bool distributeShardsLikeExists
    = collection.distributeShardsLike.is_initialized();
  bool repairingDistributeShardsLikeExists
    = collection.repairingDistributeShardsLike.is_initialized();

  TRI_ASSERT(distributeShardsLikeExists != repairingDistributeShardsLikeExists);
  if (distributeShardsLikeExists == repairingDistributeShardsLikeExists) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES);
  }

  bool renameDistributeShardsLike = distributeShardsLikeExists;

  if (renameDistributeShardsLike) {
    collection.repairingDistributeShardsLike.swap(
      collection.distributeShardsLike
    );
  }

  size_t previousReplicationFactor = collection.replicationFactor;

  collection.replicationFactor = proto.replicationFactor;

  return BeginRepairsOperation {
    .database = collection.database,
    .collectionId = collection.id,
    .collectionName = collection.name,
    .protoCollectionId = proto.id,
    .protoCollectionName = proto.name,
    .collectionReplicationFactor = previousReplicationFactor,
    .protoReplicationFactor = proto.replicationFactor,
    .renameDistributeShardsLike = renameDistributeShardsLike,
  };
}

ResultT<FinishRepairsOperation>
DistributeShardsLikeRepairer::createFinishRepairsOperation(
  Collection &collection,
  Collection const &proto
) {
  TRI_ASSERT(collection.repairingDistributeShardsLike && ! collection.distributeShardsLike);
  if (! collection.repairingDistributeShardsLike || collection.distributeShardsLike) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES);
  }

  if (collection.replicationFactor != proto.replicationFactor) {
    // this should never happen.
    std::stringstream errorMessage;
    errorMessage
      << "replicationFactor is violated: "
      << "Collection " << collection.fullName()
      << " and its distributeShardsLike prototype " << proto.fullName()
      << " have replicationFactors of " << collection.replicationFactor << " and "
      << proto.replicationFactor << ", respectively.";
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "DistributeShardsLikeRepairer::createFinishRepairsOperation: "
    << errorMessage.str();

    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
      errorMessage.str());
  }

  collection.distributeShardsLike.swap(
    collection.repairingDistributeShardsLike
  );

  return FinishRepairsOperation {
    .database = collection.database,
    .collectionId = collection.id,
    .collectionName = collection.name,
    .collectionShards = collection.shardsById,
    .protoCollectionId = proto.id,
    .protoCollectionName = proto.name,
    .protoCollectionShards = proto.shardsById,
    .replicationFactor = proto.replicationFactor,
  };
}

ResultT<std::list<RepairOperation>>
DistributeShardsLikeRepairer::fixAllShardsOfCollection(
  Collection &collection,
  Collection const &proto,
  DBServers const &availableDbServers
) {
  std::list<RepairOperation> shardRepairOperations;

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
      auto newRepairOperationsResult = fixShard(availableDbServers, collection, proto, shardId, protoShardId);
      if (newRepairOperationsResult.fail()) {
        return newRepairOperationsResult;
      }
      std::list<RepairOperation> newRepairOperations
        = newRepairOperationsResult.get();
      shardRepairOperations.splice(shardRepairOperations.end(), newRepairOperations);
    }
    else {
      LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
      << "shard " << collection.fullName() << "/" << shardId
      << " doesn't need fixing";
    }
  }

  return shardRepairOperations;
}
