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

#include "ClusterInfo.h"
#include "Logger/Logger.h"
#include "velocypack/Iterator.h"

#include <boost/optional.hpp>
#include <boost/range/combine.hpp>
#include <boost/variant.hpp>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;

class VersionSort {
  using CharOrInt = boost::variant<char, uint64_t>;

 public:

  bool operator()(std::string const &a, std::string const &b) {
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

 private:

  std::vector<CharOrInt> static splitVersion(std::string const &str) {
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
};

using DBServers = std::vector<std::string>;
using CollectionId = std::string;

struct Collection {
  // corresponding slice
  // TODO remove this, it should not be needed
  Slice const slice;

  std::string database;
  std::string name;
  uint64_t replicationFactor;
  boost::optional<CollectionId const> distributeShardsLike;
  boost::optional<CollectionId const> repairingDistributeShardsLike;
  boost::optional<bool> repairingDistributeShardsLikeReplicationFactorReduced;
  std::map<std::string, DBServers, VersionSort> shardsByName;

  std::map<std::string, Slice> residualAttributes;

  std::string agencyCollectionId() {
    return "Plan/Collections/" + this->database + "/" + this->name;
  }

  VPackBuilder createShardDbServerArrayVpack(std::string const& shardId) {
    // TODO preserve the memory behind builder.buffer()!
    VPackBuilder builder;
    builder.add(Value(ValueType::Array));

    for (auto const& it : shardsByName[shardId]) {
      builder.add(Value(it));
    }

    builder.close();

    return builder;
  }

  // maybe more?
  // isSystem
  // numberOfShards
  // deleted
};

std::map<std::string, DBServers, VersionSort>
readShards(Slice const& shards) {
  std::map<std::string, DBServers, VersionSort> shardsByName;

  for (auto const &shardIterator : ObjectIterator(shards)) {
    std::string const shardId = shardIterator.key.copyString();

    DBServers dbServers;

    for (auto const &dbServerIterator : ArrayIterator(shardIterator.value)) {
      std::string const dbServerId = dbServerIterator.copyString();

      dbServers.emplace_back(dbServerId);
    }

    shardsByName.emplace(std::make_pair(shardId, std::move(dbServers)));
  }

  return shardsByName;
}

DBServers
readDatabases(const Slice &planDbServers) {
  DBServers dbServers;

  // TODO use .[0].arango.Supervision.Health instead of .[0].arango.Plan.DBServers
  // TODO return only healthy DBServers, i.e. key =~ ^PRMR- and value.Status == "GOOD"

  for (auto const &it : ObjectIterator(planDbServers)) {
     dbServers.emplace_back(it.key.copyString());
  }

  return dbServers;
}


std::map<CollectionId, struct Collection>
readCollections(const Slice &collectionsByDatabase) {
  std::map<CollectionId, struct Collection> collections;

  // maybe extract more fields, like
  // "Lock": "..."
  // "DBServers": {...}
  // ?

  for (auto const &databaseIterator : ObjectIterator(collectionsByDatabase)) {
    std::string const databaseId = databaseIterator.key.copyString();

    Slice const &collectionsSlice = databaseIterator.value;

    for (auto const &collectionIterator : ObjectIterator(collectionsSlice)) {
      std::string const collectionId = collectionIterator.key.copyString();
      Slice const &collectionSlice = collectionIterator.value;

      // attributes
      std::string collectionName;
      uint64_t replicationFactor;
      boost::optional<std::string const> distributeShardsLike, repairingDistributeShardsLike;
      Slice shardsSlice;
      std::map<std::string, Slice> residualAttributes;

      for (auto const& it : ObjectIterator(collectionSlice)) {
        std::string const& key = it.key.copyString();
        if (key == "name") {
          collectionName = it.value.copyString();
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

      std::map<std::string, DBServers, VersionSort> shardsByName
        = readShards(shardsSlice);

      struct Collection collection {
        collectionSlice,
        databaseId,
        collectionName,
        replicationFactor,
        distributeShardsLike,
        repairingDistributeShardsLike,
        boost::none,
        std::move(shardsByName),
        std::move(residualAttributes)
      };

      collections.emplace(std::make_pair(collectionId, std::move(collection)));
    }

  }

  return collections;
}


std::vector<CollectionId>
findCollectionsToFix(std::map<CollectionId, struct Collection> collections) {
  std::vector<CollectionId> collectionsToFix;
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
  << "[tg] findCollectionsToFix()";

  for (auto const &collectionIterator : collections) {
    CollectionId const& collectionId = collectionIterator.first;
    struct Collection const& collection = collectionIterator.second;

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
    << "[tg]   checking collection " << collectionId;

    if (collection.repairingDistributeShardsLike) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "[tg]     repairingDistributeShardsLike exists, fixing";
      collectionsToFix.emplace_back(collectionId);
      continue;
    }
    if (! collection.distributeShardsLike) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "[tg]     distributeShardsLike doesn't exist, not fixing";
      continue;
    }

    struct Collection & proto = collections[collection.distributeShardsLike.get()];

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
    << "[tg]   against proto collection " << collection.distributeShardsLike.get();

    if (collection.shardsByName.size() != proto.shardsByName.size()) {
      // TODO This should maybe not be a warning.
      // TODO Is there anything we can do in this case? Why does this happen anyway?

      // answer: This should only happen if collection has "isSmart": true. In
      // that case, the number of shards should be 0.
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "Unequal number of shards in collection " << collection.database << "/" << collection.name
      << " and its distributeShardsLike collection " << proto.database << "/" << proto.name;

      continue;
    }

    for (auto const& zippedShardsIt : boost::combine(collection.shardsByName, proto.shardsByName)) {
      auto const& shardIt = zippedShardsIt.get<0>();
      auto const& protoShardIt = zippedShardsIt.get<1>();

      DBServers const& dbServers = shardIt.second;
      DBServers const& protoDbServers = protoShardIt.second;

      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "[tg]     comparing shards " << shardIt.first << " and " << protoShardIt.first;

      if (dbServers != protoDbServers) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "[tg]       fixing collection";
        collectionsToFix.emplace_back(collectionId);
        break;
      }
    }
  }

  return collectionsToFix;
}

boost::optional<std::string const>
findFreeServer(
  DBServers availableDbServers,
  DBServers shardDbServers) {
  std::sort(availableDbServers.begin(), availableDbServers.end());
  std::sort(shardDbServers.begin(), shardDbServers.end());

  DBServers freeServer;

  std::set_difference(
    availableDbServers.begin(), availableDbServers.end(),
    shardDbServers.begin(), shardDbServers.end(),
    std::back_inserter(freeServer)
  );

  if (freeServer.size() > 0) {
    return freeServer[0];
  }

  return boost::none;
}

AgencyWriteTransaction
createMoveShardTransaction(
  Collection& collection,
  std::string const& shardId,
  std::string const& from,
  std::string const& to
) {
  std::string const agencyShardId
    = collection.agencyCollectionId() + "/shards/" + shardId;

  // TODO fix builder/buffer memory management
  VPackBuilder builder = collection.createShardDbServerArrayVpack(shardId);
  VPackSlice oldDbServerSlice = builder.slice();
  std::shared_ptr<VPackBuffer<uint8_t>> oldDbServerBuffer = builder.steal();

  AgencyPrecondition agencyPrecondition {
    agencyShardId,
    AgencyPrecondition::Type::VALUE,
    oldDbServerSlice
  };

  for (auto& it : collection.shardsByName[shardId]) {
    if (it == from) {
      it = to;
    }
  }

  builder = collection.createShardDbServerArrayVpack(shardId);
  VPackSlice newDbServerSlice = builder.slice();
  std::shared_ptr<VPackBuffer<uint8_t>> newDbServerBuffer = builder.steal();

  AgencyOperation agencyOperation {
    agencyShardId,
    AgencyValueOperationType::SET,
    newDbServerSlice
  };

  return AgencyWriteTransaction { agencyOperation, agencyPrecondition };
}


std::list<AgencyWriteTransaction> fixLeader(
  DBServers const& availableDbServers,
  Collection& collection,
  Collection& proto,
  std::string const& shardId,
  std::string const& protoShardId
) {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] fixLeader("
  << "\"" << collection.database << "/" << collection.name << "\","
  << "\"" << proto.database << "/" << proto.name << "\","
  << "\"" << shardId << "/" << protoShardId << "\","
  << ")";

  DBServers const& protoShardDbServers = proto.shardsByName[protoShardId];
  DBServers& shardDbServers = collection.shardsByName[shardId];

  std::string const& protoLeader = protoShardDbServers.front();
  std::string const& shardLeader = shardDbServers.front();

  std::list<AgencyWriteTransaction> transactions;

//[PSEUDO-TODO] didReduceRepl := false

  if (protoLeader == shardLeader) {
    return {};
  }

  if (collection.replicationFactor == availableDbServers.size()) {
    // TODO maybe check if col.replicationFactor is at least 2 or 3?
    collection.replicationFactor -= 1;
    collection.repairingDistributeShardsLikeReplicationFactorReduced = true;
    // TODO create transaction. write this code after writing a corresponding test!
  }
  
  if (std::find(shardDbServers.begin(), shardDbServers.end(), protoLeader) != shardDbServers.end()) {
    boost::optional<std::string const> tmpServer = findFreeServer(availableDbServers, shardDbServers);

    if (! tmpServer) {
      // TODO write a test and throw a meaningful exception
      THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
    }

    AgencyWriteTransaction trx = createMoveShardTransaction(collection, shardId, protoLeader, tmpServer.get());

    transactions.push_back(trx);
  }

  AgencyWriteTransaction trx = createMoveShardTransaction(collection, shardId, shardLeader, protoLeader);

  transactions.push_back(trx);

  if (collection.repairingDistributeShardsLikeReplicationFactorReduced
    and collection.repairingDistributeShardsLikeReplicationFactorReduced.get()) {
    collection.replicationFactor += 1;
    collection.repairingDistributeShardsLikeReplicationFactorReduced = boost::none;
    // TODO write test (see above). Add a transaction. Don't forget to add
    // replicationFactor to the preconditions.
//[PSEUDO-TODO]  if (didReduceRepl) {
//[PSEUDO-TODO]    col.replicationFactor += 1;
//[PSEUDO-TODO]    waitForSync();
//[PSEUDO-TODO]  }
  }

  return transactions;
}


std::list<AgencyWriteTransaction> fixShard(
  DBServers const& availableDbServers,
  Collection& collection,
  Collection& proto,
  std::string const& shardId,
  std::string const& protoShardId
) {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] fixShard("
    << "\"" << collection.database << "/" << collection.name << "\","
    << "\"" << proto.database << "/" << proto.name << "\","
    << "\"" << shardId << "/" << protoShardId << "\","
    << ")";

  fixLeader(availableDbServers, collection, proto, shardId, protoShardId);

  std::vector<AgencyPrecondition> agencyPreconditions = {
    AgencyPrecondition(
      collection.agencyCollectionId(),
      AgencyPrecondition::Type::VALUE,
      collection.slice
    )
  };

  std::vector<AgencyOperation> agencyOperations = {};

  // TODO both arguments may either be a single Op/Precond or a vector. Use a single value if applicable.
  AgencyWriteTransaction trx(agencyOperations, agencyPreconditions);

  return {trx};
}


std::list<AgencyWriteTransaction>
ClusterRepairs::repairDistributeShardsLike(
  Slice const& planCollections,
  Slice const& planDbServers
) {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
  << "[tg] ClusterRepairs::repairDistributeShardsLike()";

  // Needed to build agency transactions
  TRI_ASSERT(AgencyCommManager::MANAGER != nullptr);

  std::map<CollectionId, struct Collection> collectionMap = readCollections(planCollections);
  DBServers const availableDbServers = readDatabases(planDbServers);

  std::vector<CollectionId> collectionsToFix = findCollectionsToFix(collectionMap);

  std::list<AgencyWriteTransaction> transactions;

  for (auto const& collectionIdIterator : collectionsToFix) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
    << "[tg] fix collection " << collectionIdIterator;
    struct Collection& collection = collectionMap[collectionIdIterator];

    // TODO rename distributeShardsLike to repairingDistributeShardsLike in
    // collection
    std::string protoId;
    if (collection.distributeShardsLike) {
      protoId = collection.distributeShardsLike.get();
    }
    else if (collection.repairingDistributeShardsLike) {
      protoId = collection.repairingDistributeShardsLike.get();
    }

    struct Collection& proto = collectionMap[protoId];

    for (auto const& zippedShardsIterator : boost::combine(collection.shardsByName, proto.shardsByName)) {
      auto const &shardIterator = zippedShardsIterator.get<0>();
      auto const &protoShardIterator = zippedShardsIterator.get<1>();

      std::string const& shardId = shardIterator.first;
      std::string const& protoShardId = protoShardIterator.first;

      DBServers const &dbServers = shardIterator.second;
      DBServers const &protoDbServers = protoShardIterator.second;

      if (dbServers != protoDbServers) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "[tg] shard " << shardId << " needs fixing";
        // TODO Do we need to check that dbServers and protoDbServers are not empty?
        // TODO Do we need to check that dbServers and protoDbServers are of equal size?
        std::list<AgencyWriteTransaction> newTransactions
          = fixShard(availableDbServers, collection, proto, shardId, protoShardId);
        transactions.splice(transactions.end(), newTransactions);
      }
      else {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "[tg] shard " << shardId << " doesn't need fixing";
      }
    }
  }

  // Phase 1:
  // Step 1.1: filter collections to fix
  // Step 1.1a: link proto collection to each collection to fix
  // Step 1.2: add collections with repairingDistributeShardsLike and corresponding proto collection
  // Step 1.3:
  // Phase 2:
  // fixShard()


  return transactions;
}
