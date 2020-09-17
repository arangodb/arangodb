////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include <Basics/StringUtils.h>
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <boost/range/combine.hpp>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::cluster_repairs;

std::map<ShardID, DBServers, VersionSort> DistributeShardsLikeRepairer::readShards(Slice const& shards) {
  std::map<ShardID, DBServers, VersionSort> shardsById;

  for (auto const& shardIterator : ObjectIterator(shards)) {
    ShardID const shardId = shardIterator.key.copyString();

    DBServers dbServers;

    for (auto const& dbServerIterator : ArrayIterator(shardIterator.value)) {
      ServerID const dbServerId = dbServerIterator.copyString();
      dbServers.emplace_back(std::move(dbServerId));
    }

    shardsById.try_emplace(std::move(shardId), std::move(dbServers));
  }

  return shardsById;
}

DBServers DistributeShardsLikeRepairer::readDatabases(const Slice& supervisionHealth) {
  DBServers dbServers;

  for (auto const& it : ObjectIterator(supervisionHealth)) {
    ServerID const& serverId = it.key.copyString();
    if (serverId.substr(0, 5) == "PRMR-" && it.value.hasKey("Status") &&
        it.value.get("Status").copyString() == "GOOD") {
      dbServers.emplace_back(it.key.copyString());
    }
  }

  return dbServers;
}

ResultT<std::map<CollectionID, struct cluster_repairs::Collection>>
DistributeShardsLikeRepairer::readCollections(const Slice& collectionsByDatabase) {
  std::map<CollectionID, struct Collection> collections;

  for (auto const& databaseIterator : ObjectIterator(collectionsByDatabase)) {
    DatabaseID const databaseId = databaseIterator.key.copyString();

    Slice const& collectionsSlice = databaseIterator.value;

    for (auto const& collectionIterator : ObjectIterator(collectionsSlice)) {
      CollectionID const collectionId = collectionIterator.key.copyString();
      Slice const& collectionSlice = collectionIterator.value;

      // attributes
      std::string collectionName;
      uint64_t replicationFactor = 0;
      bool deleted = false;
      bool isSmart = false;
      std::optional<CollectionID> distributeShardsLike;
      std::optional<CollectionID> repairingDistributeShardsLike;
      Slice shardsSlice;
      std::map<std::string, Slice> residualAttributes;

      for (auto const& it : ObjectIterator(collectionSlice)) {
        std::string const& key = it.key.copyString();
        if (key == StaticStrings::DataSourceName) {
          collectionName = it.value.copyString();
        } else if (key == StaticStrings::DataSourceId) {
          std::string const id = it.value.copyString();
          TRI_ASSERT(id == collectionId);
        } else if (key == "replicationFactor" && it.value.isInteger()) {
          // replicationFactor may be "satellite" instead of an int.
          // This can be ignored here.
          replicationFactor = it.value.getUInt();
        } else if (key == "distributeShardsLike") {
          distributeShardsLike.emplace(it.value.copyString());
        } else if (key == "repairingDistributeShardsLike") {
          repairingDistributeShardsLike.emplace(it.value.copyString());
        } else if (key == "shards") {
          shardsSlice = it.value;
        } else if (key == StaticStrings::DataSourceDeleted) {
          deleted = it.value.getBool();
        } else if (key == StaticStrings::IsSmart) {
          isSmart = it.value.getBool();
        }
      }

      std::map<ShardID, DBServers, VersionSort> shardsById = readShards(shardsSlice);

      struct Collection collection {
        _database = databaseId, _collectionId = collectionId,
        _collectionName = collectionName,
        _replicationFactor = replicationFactor, _deleted = deleted,
        _isSmart = isSmart, _distributeShardsLike = distributeShardsLike,
        _repairingDistributeShardsLike = repairingDistributeShardsLike,
        _shardsById = shardsById
      };

      collections.try_emplace(std::move(collectionId), std::move(collection));
    }
  }

  return collections;
}

std::vector<std::pair<CollectionID, Result>> DistributeShardsLikeRepairer::findCollectionsToFix(
    std::map<CollectionID, struct cluster_repairs::Collection> collections) {
  LOG_TOPIC("88d36", TRACE, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::findCollectionsToFix: started";

  std::vector<std::pair<CollectionID, Result>> collectionsToFix;

  for (auto const& collectionIterator : collections) {
    CollectionID const& collectionId = collectionIterator.first;
    struct Collection const& collection = collectionIterator.second;

    LOG_TOPIC("2d887", TRACE, arangodb::Logger::CLUSTER)
        << "findCollectionsToFix: checking collection " << collection.fullName();

    if (collection.deleted) {
      LOG_TOPIC("e900c", DEBUG, arangodb::Logger::CLUSTER)
          << "findCollectionsToFix: collection " << collection.fullName()
          << " has `deleted: true` and will be ignored.";
      continue;
    }

    if (collection.repairingDistributeShardsLike) {
      LOG_TOPIC("0fc23", DEBUG, arangodb::Logger::CLUSTER)
          << "findCollectionsToFix: repairs on collection " << collection.fullName()
          << " were already started earlier, but are unfinished "
          << "(attribute repairingDistributeShardsLike exists). "
          << "Adding it to the list of collections to fix.";
      collectionsToFix.emplace_back(std::make_pair(collectionId, Result()));
      continue;
    }
    if (!collection.distributeShardsLike) {
      LOG_TOPIC("c9d9f", TRACE, arangodb::Logger::CLUSTER)
          << "findCollectionsToFix: distributeShardsLike doesn't exist, not "
             "fixing "
          << collection.fullName();
      continue;
    }

    struct Collection& proto = collections.at(collection.distributeShardsLike.value());

    LOG_TOPIC("994ab", TRACE, arangodb::Logger::CLUSTER)
        << "findCollectionsToFix: comparing against distributeShardsLike "
           "collection "
        << proto.fullName();

    if (collection.shardsById.size() != proto.shardsById.size()) {
      if (collection.isSmart && collection.shardsById.empty()) {
        // This case is expected: smart edge collections have no shards.
        continue;
      }

      LOG_TOPIC("20bef", ERR, arangodb::Logger::CLUSTER)
          << "DistributeShardsLikeRepairer::findCollectionsToFix: "
          << "Unequal number of shards in collection " << collection.fullName()
          << " and its distributeShardsLike collection " << proto.fullName();

      collectionsToFix.emplace_back(
          std::make_pair(collectionId, Result(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS)));
      continue;
    }

    for (auto const& zippedShardsIt :
         boost::combine(collection.shardsById, proto.shardsById)) {
      auto const& shardIt = zippedShardsIt.get<0>();
      auto const& protoShardIt = zippedShardsIt.get<1>();

      DBServers const& dbServers = shardIt.second;
      DBServers const& protoDbServers = protoShardIt.second;

      LOG_TOPIC("0de03", TRACE, arangodb::Logger::CLUSTER)
          << "findCollectionsToFix: comparing shards " << shardIt.first
          << " and " << protoShardIt.first;

      if (dbServers != protoDbServers) {
        LOG_TOPIC("d142d", DEBUG, arangodb::Logger::CLUSTER)
            << "findCollectionsToFix: collection " << collection.fullName()
            << " needs fixing because (at least) shard " << shardIt.first
            << " differs from " << protoShardIt.first << " in " << proto.fullName();
        collectionsToFix.emplace_back(std::make_pair(collectionId, Result()));
        break;
      }
    }
  }

  return collectionsToFix;
}

std::optional<ServerID const> DistributeShardsLikeRepairer::findFreeServer(
    DBServers const& availableDbServers, DBServers const& shardDbServers) {
  DBServers freeServer = serverSetDifference(availableDbServers, shardDbServers);

  if (!freeServer.empty()) {
    return freeServer[0];
  }

  return std::nullopt;
}

DBServers DistributeShardsLikeRepairer::serverSetDifference(DBServers setA, DBServers setB) {
  sort(setA.begin(), setA.end());
  sort(setB.begin(), setB.end());

  DBServers difference;

  set_difference(setA.begin(), setA.end(), setB.begin(), setB.end(),
                 back_inserter(difference));

  return difference;
}

DBServers DistributeShardsLikeRepairer::serverSetSymmetricDifference(DBServers setA,
                                                                     DBServers setB) {
  sort(setA.begin(), setA.end());
  sort(setB.begin(), setB.end());

  DBServers symmetricDifference;

  std::set_symmetric_difference(setA.begin(), setA.end(), setB.begin(),
                                setB.end(), back_inserter(symmetricDifference));

  return symmetricDifference;
}

MoveShardOperation DistributeShardsLikeRepairer::createMoveShardOperation(
    struct cluster_repairs::Collection& collection, ShardID const& shardId,
    ServerID const& fromServerId, ServerID const& toServerId, bool isLeader) {
  MoveShardOperation moveShardOperation{_database = collection.database,
                                        _collectionId = collection.id,
                                        _collectionName = collection.name,
                                        _shard = shardId,
                                        _from = fromServerId,
                                        _to = toServerId,
                                        _isLeader = isLeader};

  {  // "Move" the shard in `collection`
    DBServers& dbServers = collection.shardsById.at(shardId);
    DBServers dbServersAfterMove;

    // If moving the leader, the new server will be the first in the list.
    if (isLeader) {
      dbServersAfterMove.push_back(toServerId);
    }

    // Copy all but the 'from' server. Relative order stays unchanged.
    std::copy_if(dbServers.begin(), dbServers.end(), std::back_inserter(dbServersAfterMove),
                 [&fromServerId](ServerID const& it) {
                   return it != fromServerId;
                 });

    // If moving a follower, the new server will be the last in the list.
    if (!isLeader) {
      dbServersAfterMove.push_back(toServerId);
    }

    dbServers.swap(dbServersAfterMove);
  }

  return moveShardOperation;
}

ResultT<std::list<RepairOperation>> DistributeShardsLikeRepairer::fixLeader(
    DBServers const& availableDbServers, struct cluster_repairs::Collection& collection,
    struct cluster_repairs::Collection const& proto, ShardID const& shardId,
    ShardID const& protoShardId) {
  LOG_TOPIC("61262", DEBUG, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::fixLeader("
      << "\"" << collection.database << "/" << collection.name << "\","
      << "\"" << proto.database << "/" << proto.name << "\","
      << "\"" << shardId << "/" << protoShardId << "\","
      << ")";

  DBServers const& protoShardDbServers = proto.shardsById.at(protoShardId);
  DBServers& shardDbServers = collection.shardsById.at(shardId);

  if (protoShardDbServers.empty() || shardDbServers.empty()) {
    return Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS);
  }

  ServerID const protoLeader = protoShardDbServers.front();
  ServerID const shardLeader = shardDbServers.front();

  std::list<RepairOperation> repairOperations;

  if (protoLeader == shardLeader) {
    return repairOperations;
  }

  if (collection.replicationFactor == availableDbServers.size()) {
    // The replicationFactor should have been reduced before calling this method
    return Result(TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
  }

  if (std::find(shardDbServers.begin(), shardDbServers.end(), protoLeader) !=
      shardDbServers.end()) {
    std::optional<ServerID const> tmpServer =
        findFreeServer(availableDbServers, shardDbServers);

    if (!tmpServer) {
      // This happens if all db servers that don't contain the shard are
      // unhealthy
      return Result(TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
    }

    RepairOperation moveShardOperation =
        createMoveShardOperation(collection, shardId, protoLeader, tmpServer.value(), false);

    repairOperations.emplace_back(moveShardOperation);
  }

  RepairOperation moveShardOperation =
      createMoveShardOperation(collection, shardId, shardLeader, protoLeader, true);

  repairOperations.emplace_back(moveShardOperation);

  return repairOperations;
}

ResultT<std::list<RepairOperation>> DistributeShardsLikeRepairer::fixShard(
    DBServers const& availableDbServers, struct cluster_repairs::Collection& collection,
    struct cluster_repairs::Collection const& proto, ShardID const& shardId,
    ShardID const& protoShardId) {
  LOG_TOPIC("d585c", DEBUG, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::fixShard: "
      << "Called for shard " << shardId << " with prototype " << protoShardId;

  if (collection.replicationFactor != proto.replicationFactor) {
    std::stringstream errorMessage;
    errorMessage << "replicationFactor is violated: "
                 << "Collection " << collection.fullName()
                 << " and its distributeShardsLike prototype " << proto.fullName()
                 << " have replicationFactors of " << collection.replicationFactor
                 << " and " << proto.replicationFactor << ", respectively.";
    LOG_TOPIC("34b04", ERR, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::fixShard: " << errorMessage.str();

    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
                  errorMessage.str());
  }

  auto fixLeaderRepairOperationsResult =
      fixLeader(availableDbServers, collection, proto, shardId, protoShardId);
  if (fixLeaderRepairOperationsResult.fail()) {
    return fixLeaderRepairOperationsResult;
  }

  std::list<RepairOperation> repairOperations = fixLeaderRepairOperationsResult.get();

  DBServers const& protoShardDbServers = proto.shardsById.at(protoShardId);
  DBServers& shardDbServers = collection.shardsById.at(shardId);

  DBServers serversOnlyOnProto = serverSetDifference(protoShardDbServers, shardDbServers);

  DBServers serversOnlyOnShard = serverSetDifference(shardDbServers, protoShardDbServers);

  if (serversOnlyOnProto.size() != serversOnlyOnShard.size()) {
    std::stringstream errorMessage;
    errorMessage << "replicationFactor is violated: "
                 << "Collection " << collection.fullName()
                 << " and its distributeShardsLike prototype " << proto.fullName()
                 << " have " << serversOnlyOnShard.size() << " and "
                 << serversOnlyOnProto.size() << " different (mismatching) "
                 << "DBServers, respectively.";
    LOG_TOPIC("cfc3f", ERR, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::fixShard: " << errorMessage.str();

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

  ResultT<std::optional<FixServerOrderOperation>> maybeFixServerOrderOperationResult =
      createFixServerOrderOperation(collection, proto, shardId, protoShardId);

  if (maybeFixServerOrderOperationResult.fail()) {
    return std::move(maybeFixServerOrderOperationResult).result();
  }

  if (auto const& maybeFixServerOrderOperation =
          maybeFixServerOrderOperationResult.get()) {
    FixServerOrderOperation const& fixServerOrderOperation =
        maybeFixServerOrderOperation.value();
    repairOperations.emplace_back(fixServerOrderOperation);
  }

  return repairOperations;
}

ResultT<std::map<CollectionID, ResultT<std::list<RepairOperation>>>> DistributeShardsLikeRepairer::repairDistributeShardsLike(
    Slice const& planCollections, Slice const& supervisionHealth) {
  LOG_TOPIC("8f26d", INFO, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
      << "Starting to collect necessary repairs";

  auto collectionMapResult = readCollections(planCollections);
  if (collectionMapResult.fail()) {
    return std::move(collectionMapResult).result();
  }
  std::map<CollectionID, struct Collection> collectionMap = collectionMapResult.get();
  DBServers const availableDbServers = readDatabases(supervisionHealth);

  std::vector<std::pair<CollectionID, Result>> collectionsToFix =
      findCollectionsToFix(collectionMap);

  std::map<CollectionID, ResultT<std::list<RepairOperation>>> repairOperationsByCollection;

  for (auto const& collectionIterator : collectionsToFix) {
    CollectionID const& collectionId = collectionIterator.first;
    Result const& result = collectionIterator.second;
    if (result.fail()) {
      repairOperationsByCollection.try_emplace(collectionId, result);
      continue;
    }
    struct Collection& collection = collectionMap.at(collectionId);
    LOG_TOPIC("f82b1", TRACE, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::repairDistributeShardsLike: fixing "
           "collection "
        << collection.fullName();

    ShardID protoId;

    bool failOnPurpose = false;
    TRI_IF_FAILURE(
        "DistributeShardsLikeRepairer::repairDistributeShardsLike/"
        "TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES") {
      if (StringUtils::isSuffix(collection.name,
                                "---fail_inconsistent_attributes_in_"
                                "repairDistributeShardsLike")) {
        failOnPurpose = true;
      }
    }
    if (collection.distributeShardsLike && !failOnPurpose) {
      protoId = collection.distributeShardsLike.value();
    } else if (collection.repairingDistributeShardsLike && !failOnPurpose) {
      protoId = collection.repairingDistributeShardsLike.value();
    } else {
      // This should never happen
      LOG_TOPIC("2b82f", ERR, arangodb::Logger::CLUSTER)
          << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
          << "(repairing)distributeShardsLike missing in " << collection.fullName();
      repairOperationsByCollection.try_emplace(collection.id,
                                           Result{TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES});
      continue;
    }

    struct Collection& proto = collectionMap.at(protoId);

    auto beginRepairsOperation = createBeginRepairsOperation(collection, proto);
    if (beginRepairsOperation.fail()) {
      repairOperationsByCollection.try_emplace(collection.id,
                                           std::move(beginRepairsOperation).result());
      continue;
    }
    std::list<RepairOperation> repairOperations;
    repairOperations.emplace_back(std::move(beginRepairsOperation.get()));

    ResultT<std::list<RepairOperation>> shardRepairOperationsResult =
        fixAllShardsOfCollection(collection, proto, availableDbServers);

    if (shardRepairOperationsResult.fail()) {
      repairOperationsByCollection.try_emplace(
          collection.id, std::move(shardRepairOperationsResult).result());
      continue;
    }

    repairOperations.splice(repairOperations.end(), shardRepairOperationsResult.get());

    auto finishRepairsOperation = createFinishRepairsOperation(collection, proto);
    if (finishRepairsOperation.fail()) {
      repairOperationsByCollection.try_emplace(collection.id,
                                           std::move(finishRepairsOperation).result());
      continue;
    }
    repairOperations.emplace_back(std::move(finishRepairsOperation.get()));

    repairOperationsByCollection.try_emplace(collection.id, std::move(repairOperations));
  }

  return repairOperationsByCollection;
}

ResultT<std::optional<FixServerOrderOperation>> DistributeShardsLikeRepairer::createFixServerOrderOperation(
    struct cluster_repairs::Collection& collection,
    struct cluster_repairs::Collection const& proto, ShardID const& shardId,
    ShardID const& protoShardId) {
  LOG_TOPIC("4b432", DEBUG, arangodb::Logger::CLUSTER)
      << "DistributeShardsLikeRepairer::createFixServerOrderOperation: "
      << "Fixing DBServer order on " << collection.fullName() << "/" << shardId
      << " to match " << proto.fullName() << "/" << protoShardId;

  DBServers& dbServers = collection.shardsById.at(shardId);
  DBServers const& protoDbServers = proto.shardsById.at(protoShardId);

  TRI_ASSERT(dbServers.size() == protoDbServers.size());
  if (dbServers.size() != protoDbServers.size()) {
    std::stringstream errorMessage;
    errorMessage << "replicationFactor violated: Collection "
                 << collection.fullName() << " and its distributeShardsLike "
                 << " prototype have mismatching numbers of DBServers; "
                 << dbServers.size() << " (on shard " << shardId << ") "
                 << "and " << protoDbServers.size() << " (on shard " << protoShardId << ")"
                 << ", respectively.";
    LOG_TOPIC("daf62", ERR, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::createFixServerOrderOperation: "
        << errorMessage.str();
    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
                  errorMessage.str());
  }

  TRI_ASSERT(!dbServers.empty());
  if (dbServers.empty()) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS);
  }

  bool leadersMatch = dbServers[0] == protoDbServers[0];
  TRI_ASSERT(leadersMatch);
  TRI_IF_FAILURE(
      "DistributeShardsLikeRepairer::createFixServerOrderOperation/"
      "TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS") {
    if (StringUtils::isSuffix(collection.name, "---fail_mismatching_leaders")) {
      leadersMatch = false;
    }
  }
  if (!leadersMatch) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS);
  }
  ServerID const leader = protoDbServers[0];

  bool followersMatch = serverSetSymmetricDifference(dbServers, protoDbServers).empty();
  TRI_ASSERT(followersMatch);
  TRI_IF_FAILURE(
      "DistributeShardsLikeRepairer::createFixServerOrderOperation/"
      "TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS") {
    if (StringUtils::isSuffix(collection.name,
                              "---fail_mismatching_followers")) {
      followersMatch = false;
    }
  }
  if (!followersMatch) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS);
  }

  if (dbServers == protoDbServers) {
    LOG_TOPIC("9e454", DEBUG, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::createFixServerOrderOperation: "
        << "Order is already equal, doing nothing";

    return {std::nullopt};
  }

  FixServerOrderOperation fixServerOrderOperation{
      _database = collection.database,
      _collectionId = collection.id,
      _collectionName = collection.name,
      _protoCollectionId = proto.id,
      _protoCollectionName = proto.name,
      _shard = shardId,
      _protoShard = protoShardId,
      _leader = leader,
      _followers = DBServers{dbServers.begin() + 1, dbServers.end()},
      _protoFollowers = DBServers{protoDbServers.begin() + 1, protoDbServers.end()}};

  // Change order for the rest of the repairs as well
  dbServers = protoDbServers;

  return {fixServerOrderOperation};
}

ResultT<BeginRepairsOperation> DistributeShardsLikeRepairer::createBeginRepairsOperation(
    struct cluster_repairs::Collection& collection,
    struct cluster_repairs::Collection const& proto) {
  bool distributeShardsLikeExists = collection.distributeShardsLike.has_value();
  bool repairingDistributeShardsLikeExists =
      collection.repairingDistributeShardsLike.has_value();

  bool exactlyOneDslAttrIsSet = distributeShardsLikeExists != repairingDistributeShardsLikeExists;
  TRI_ASSERT(exactlyOneDslAttrIsSet);
  TRI_IF_FAILURE(
      "DistributeShardsLikeRepairer::createBeginRepairsOperation/"
      "TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES") {
    if (StringUtils::isSuffix(
            collection.name,
            "---fail_inconsistent_attributes_in_createBeginRepairsOperation")) {
      exactlyOneDslAttrIsSet = false;
    }
  }
  if (!exactlyOneDslAttrIsSet) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES);
  }

  bool renameDistributeShardsLike = distributeShardsLikeExists;

  if (renameDistributeShardsLike) {
    collection.repairingDistributeShardsLike.swap(collection.distributeShardsLike);
  }

  uint64_t previousReplicationFactor = collection.replicationFactor;

  collection.replicationFactor = proto.replicationFactor;

  return BeginRepairsOperation(_database = collection.database,
                               _collectionId = collection.id,
                               _collectionName = collection.name,
                               _protoCollectionId = proto.id,
                               _protoCollectionName = proto.name,
                               _collectionReplicationFactor = previousReplicationFactor,
                               _protoReplicationFactor = proto.replicationFactor,
                               _renameDistributeShardsLike = renameDistributeShardsLike);
}

ResultT<FinishRepairsOperation> DistributeShardsLikeRepairer::createFinishRepairsOperation(
    struct cluster_repairs::Collection& collection,
    struct cluster_repairs::Collection const& proto) {
  bool onlyRepairingDslIsSet = collection.repairingDistributeShardsLike &&
                               !collection.distributeShardsLike;
  TRI_ASSERT(onlyRepairingDslIsSet);
  TRI_IF_FAILURE(
      "DistributeShardsLikeRepairer::createFinishRepairsOperation/"
      "TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES") {
    if (StringUtils::isSuffix(collection.name,
                              "---fail_inconsistent_attributes_in_"
                              "createFinishRepairsOperation")) {
      onlyRepairingDslIsSet = false;
    }
  }
  if (!onlyRepairingDslIsSet) {
    // this should never happen.
    return Result(TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES);
  }

  if (collection.replicationFactor != proto.replicationFactor) {
    // this should never happen.
    std::stringstream errorMessage;
    errorMessage << "replicationFactor is violated: "
                 << "Collection " << collection.fullName()
                 << " and its distributeShardsLike prototype " << proto.fullName()
                 << " have replicationFactors of " << collection.replicationFactor
                 << " and " << proto.replicationFactor << ", respectively.";
    LOG_TOPIC("b583a", ERR, arangodb::Logger::CLUSTER)
        << "DistributeShardsLikeRepairer::createFinishRepairsOperation: "
        << errorMessage.str();

    return Result(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED,
                  errorMessage.str());
  }

  collection.distributeShardsLike.swap(collection.repairingDistributeShardsLike);

  return FinishRepairsOperation{_database = collection.database,
                                _collectionId = collection.id,
                                _collectionName = collection.name,
                                _protoCollectionId = proto.id,
                                _protoCollectionName = proto.name,
                                _shards = createShardVector(collection.shardsById,
                                                            proto.shardsById),
                                _replicationFactor = proto.replicationFactor};
}

std::vector<cluster_repairs::ShardWithProtoAndDbServers> DistributeShardsLikeRepairer::createShardVector(
    std::map<ShardID, DBServers, VersionSort> const& shardsById,
    std::map<ShardID, DBServers, VersionSort> const& protoShardsById) {
  std::vector<ShardWithProtoAndDbServers> shards;

  for (auto const& it : boost::combine(shardsById, protoShardsById)) {
    auto const& shardIt = it.get<0>();
    auto const& protoShardIt = it.get<1>();
    ShardID const& shard = shardIt.first;
    ShardID const& protoShard = protoShardIt.first;

    // DBServers must be the same at this point!
    TRI_ASSERT(shardIt.second == protoShardIt.second);

    DBServers const& dbServers = protoShardIt.second;

    shards.emplace_back(std::make_tuple(shard, protoShard, dbServers));
  }

  return shards;
}

ResultT<std::list<RepairOperation>> DistributeShardsLikeRepairer::fixAllShardsOfCollection(
    struct cluster_repairs::Collection& collection,
    struct cluster_repairs::Collection const& proto, DBServers const& availableDbServers) {
  std::list<RepairOperation> shardRepairOperations;

  for (auto const& zippedShardsIterator :
       boost::combine(collection.shardsById, proto.shardsById)) {
    auto const& shardIterator = zippedShardsIterator.get<0>();
    auto const& protoShardIterator = zippedShardsIterator.get<1>();

    ShardID const& shardId = shardIterator.first;
    ShardID const& protoShardId = protoShardIterator.first;

    DBServers const& dbServers = shardIterator.second;
    DBServers const& protoDbServers = protoShardIterator.second;

    if (dbServers != protoDbServers) {
      LOG_TOPIC("2584a", INFO, arangodb::Logger::CLUSTER)
          << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
          << "Shard " << shardId << " of collection " << collection.fullName()
          << " does not match shard " << protoShardId << " of collection "
          << proto.fullName() << " as it should. Collecting repairs.";

      bool protoDbServersEmpty = protoDbServers.empty();
      TRI_IF_FAILURE(
          "DistributeShardsLikeRepairer::repairDistributeShardsLike/"
          "TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS") {
        if (StringUtils::isSuffix(collection.name,
                                  "---fail_no_proto_dbservers")) {
          protoDbServersEmpty = true;
        }
      }
      if (protoDbServersEmpty) {
        LOG_TOPIC("5942c", ERR, arangodb::Logger::CLUSTER)
            << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
            << "prototype shard " << proto.fullName() << "/" << protoShardId << " of shard "
            << collection.fullName() << "/" << shardId << " has no DBServers!";
        return Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS);
      }
      bool dbServersEmpty = dbServers.empty();
      TRI_IF_FAILURE(
          "DistributeShardsLikeRepairer::repairDistributeShardsLike/"
          "TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS") {
        if (StringUtils::isSuffix(collection.name, "---fail_no_dbservers")) {
          dbServersEmpty = true;
        }
      }
      if (dbServersEmpty) {
        LOG_TOPIC("ce865", ERR, arangodb::Logger::CLUSTER)
            << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
            << " shard " << collection.fullName() << "/" << shardId
            << " has no DBServers!";
        return Result(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS);
      }

      auto newRepairOperationsResult =
          fixShard(availableDbServers, collection, proto, shardId, protoShardId);
      if (newRepairOperationsResult.fail()) {
        return newRepairOperationsResult;
      }
      std::list<RepairOperation> newRepairOperations = newRepairOperationsResult.get();
      shardRepairOperations.splice(shardRepairOperations.end(), newRepairOperations);
    } else {
      LOG_TOPIC("e5827", TRACE, arangodb::Logger::CLUSTER)
          << "DistributeShardsLikeRepairer::repairDistributeShardsLike: "
          << "shard " << collection.fullName() << "/" << shardId << " doesn't need fixing";
    }
  }

  return shardRepairOperations;
}

cluster_repairs::Collection::Collection(
    tagged_argument<tag::database, DatabaseID> database_,
    tagged_argument<tag::collectionId, CollectionID> collectionId_,
    tagged_argument<tag::collectionName, std::string> collectionName_,
    tagged_argument<tag::replicationFactor, uint64_t> replicationFactor_,
    tagged_argument<tag::deleted, bool> deleted_, tagged_argument<tag::isSmart, bool> isSmart_,
    tagged_argument<tag::distributeShardsLike, std::optional<CollectionID>> distributeShardsLike_,
    tagged_argument<tag::repairingDistributeShardsLike, std::optional<CollectionID>> repairingDistributeShardsLike_,
    tagged_argument<tag::shardsById, std::map<ShardID, DBServers, VersionSort>> shardsById_)
    : database(database_.value),
      name(collectionName_.value),
      id(collectionId_.value),
      replicationFactor(replicationFactor_.value),
      deleted(deleted_.value),
      isSmart(isSmart_.value),
      distributeShardsLike(distributeShardsLike_.value),
      repairingDistributeShardsLike(repairingDistributeShardsLike_.value),
      shardsById(shardsById_.value) {}
