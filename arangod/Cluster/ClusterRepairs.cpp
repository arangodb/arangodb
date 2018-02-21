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

  std::string database;
  std::string name;
  boost::optional<CollectionId const> distributeShardsLike;
  boost::optional<CollectionId const> repairingDistributeShardsLike;
  std::map<std::string, DBServers, VersionSort> shardsByName;

  // maybe more?
  // isSystem
  // numberOfShards
  // replicationFactor
  // deleted
};


std::map<CollectionId, struct Collection>
readCollections(const VPackSlice &plan) {
  std::map<CollectionId, struct Collection> collections;

  Slice const &collectionsByDatabase = plan.get("Collections");

  // maybe extract more fields, like
  // "Lock": "..."
  // "DBServers": {...}
  // ?

  for (auto const &databaseIterator : ObjectIterator(collectionsByDatabase)) {
    std::string const databaseId = databaseIterator.key.copyString();

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
    << "  db: " << databaseId;

    Slice const &collectionsSlice = databaseIterator.value;

    for (auto const &collectionIterator : ObjectIterator(collectionsSlice)) {
      std::string const collectionId = collectionIterator.key.copyString();
      Slice const &collectionSlice = collectionIterator.value;
      std::string const collectionName = collectionSlice.get("name").copyString();
      boost::optional<std::string const> distributeShardsLike, repairingDistributeShardsLike;
      if (collectionSlice.hasKey("distributeShardsLike")) {
        distributeShardsLike.emplace(
          collectionSlice.get("distributeShardsLike").copyString()
        );
      }
      if (collectionSlice.hasKey("repairingDistributeShardsLike")) {
        repairingDistributeShardsLike.emplace(
          collectionSlice.get("repairingDistributeShardsLike").copyString()
        );
      }

      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "    collection: " << collectionId << " / " << collectionName;

      if (distributeShardsLike) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "    distributeShardsLike: " << distributeShardsLike.get();
      }

      std::map<std::string, DBServers, VersionSort> shardsByName;

      Slice const &shardsSlice = collectionSlice.get("shards");

      for (auto const &shardIterator : ObjectIterator(shardsSlice)) {
        std::string const shardId = shardIterator.key.copyString();
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "      shard: " << shardId;

        DBServers dbServers;

        for (auto const &dbServerIterator : ArrayIterator(shardIterator.value)) {
          std::string const dbServerId = dbServerIterator.copyString();
          LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "        dbserver: " << dbServerId;

          dbServers.emplace_back(dbServerId);
        }

        shardsByName.emplace(std::make_pair(shardId, std::move(dbServers)));
      }

      struct Collection collection {
        databaseId,
        collectionName,
        distributeShardsLike,
        repairingDistributeShardsLike,
        std::move(shardsByName)
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


AgencyWriteTransaction fixShard(
  Collection const& collection,
  Collection const& proto,
  std::string const& shardId,
  std::string const& protoShardId
) {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] fixShard("
    << "\"" << collection.database << "/" << collection.name << "\","
    << "\"" << proto.database << "/" << proto.name << "\","
    << "\"" << shardId << "/" << protoShardId << "\","
    << ")";


  std::vector<AgencyOperation> agencyOperations = {};
  std::vector<AgencyPrecondition> agencyPreconditions = {};

  // TODO both arguments may either be a single Op/Precond or a vector. Use a single value if applicable.
  AgencyWriteTransaction trx(agencyOperations, agencyPreconditions);

  return trx;
}


std::vector<AgencyWriteTransaction>
ClusterRepairs::repairDistributeShardsLike(VPackSlice &&plan) {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
  << "[tg] ClusterRepairs::repairDistributeShardsLike()";

  std::map<CollectionId, struct Collection> collectionMap = readCollections(plan);

  std::vector<CollectionId> collectionsToFix = findCollectionsToFix(collectionMap);

  std::vector<AgencyWriteTransaction> transactions;

  for (auto const& collectionIdIterator : collectionsToFix) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
    << "[tg] fix collection " << collectionIdIterator;
    struct Collection const& collection = collectionMap[collectionIdIterator];

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
        AgencyWriteTransaction trx = fixShard(collection, proto, shardId, protoShardId);
        transactions.emplace_back(trx);
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
