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

  // maybe extract more, like
  // "Lock": "..."
  // "DBServers": {...}
  // ?

  for (auto const &databaseIterator : ObjectIterator(collectionsByDatabase)) {
    std::string const databaseId = databaseIterator.key.copyString();

    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
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

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "    collection: " << collectionId << " / " << collectionName;

      if (distributeShardsLike) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "    distributeShardsLike: " << distributeShardsLike.get();
      }

      struct Collection collection1 {
        databaseId,
        collectionName,
        distributeShardsLike,
        repairingDistributeShardsLike,
        {}
      };

      auto const& pairRv = collections.emplace(std::make_pair(collectionId, collection1));

      //auto& collectionIt = pairRv.first;
      struct Collection& collection = pairRv.first->second;

      std::map<std::string, DBServers, VersionSort> &shards = collection.shardsByName;

      Slice const &shardsSlice = collectionSlice.get("shards");

      for (auto const &shardIterator : ObjectIterator(shardsSlice)) {
        std::string const shardId = shardIterator.key.copyString();
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "      shard: " << shardId;

        DBServers dbServers;

        for (auto const &dbServerIterator : ArrayIterator(shardIterator.value)) {
          std::string const dbServerId = dbServerIterator.copyString();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "        dbserver: " << dbServerId;

          dbServers.emplace_back(dbServerId);
        }

        shards.emplace(std::make_pair(shardId, dbServers));
      }

    }

  }

  return collections;
}


std::vector<CollectionId>
findCollectionsToFix(std::map<CollectionId, struct Collection> collections) {
  std::vector<CollectionId> collectionsToFix;
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] findCollectionsToFix()";

  for (auto const &it : collections) {
    CollectionId const& collectionId = it.first;
    struct Collection const& collection = it.second;

    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
    << "[tg]   checking collection " << collectionId;

    if (collection.repairingDistributeShardsLike) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "[tg]     repairingDistributeShardsLike exists, fixing";
      collectionsToFix.emplace_back(collectionId);
      continue;
    }
    if (! collection.distributeShardsLike) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "[tg]     distributeShardsLike doesn't exist, not fixing";
      continue;
    }

    struct Collection & proto = collections[collection.distributeShardsLike.get()];

    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
    << "[tg]   against proto collection " << collection.distributeShardsLike.get();

//    auto zippedShards = boost::make_zip_iterator(
//      boost::make_tuple(collection.shardsByName.begin(), proto.shardsByName.begin())
//    );
//    auto zippedShards = boost::zip_iterator(
//      collection.shardsByName.begin(),
//      collection.shardsByName.end(),
//      proto.shardsByName.begin(),
//      proto.shardsByName.end()
//    );

    if (collection.shardsByName.size() != proto.shardsByName.size()) {
      // TODO This should maybe not be a warning. Also, "proto collection" should be changed in something more
      // meaningful to the user.
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "Unequal number of shards in collection " << collection.database << "/" << collection.name
      << " and its proto collection " << proto.database << "/" << proto.name;

      continue;
    }

    for (auto const& it : boost::combine(collection.shardsByName, proto.shardsByName)) {
      // boost::tie(auto const& shardIt, auto const& protoShardIt) = it;
      auto const& shardIt = it.get<0>();
      auto const& protoShardIt = it.get<1>();

      DBServers const& dbServers = shardIt.second;
      DBServers const& protoDbServers = protoShardIt.second;

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "[tg]     comparing shards " << shardIt.first << " and " << protoShardIt.first;

      if (dbServers != protoDbServers) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "[tg]       fixing shard";
        collectionsToFix.emplace_back(collectionId);
        break;
      }
    }

//    for (auto const& shardIt : collection.shardsByName) {
//      std::string const& shardName = shardIt.first;
//      DBServers const& dbServers = shardIt.second;
//      DBServers const& protoDbServers = proto.shardsByName[shardName];
//
//      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
//      << "[tg]     checking shard " << shardName;
//
//      if (dbServers != protoDbServers) {
//        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
//        << "[tg]       fixing shard";
//        collectionsToFix.emplace_back(collectionId);
//        break;
//      }
//    }
  }

  return collectionsToFix;
}


AgencyWriteTransaction
ClusterRepairs::repairDistributeShardsLike(VPackSlice &&plan) {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] ClusterMethods::repairDistributeShardsLike()";

  std::map<CollectionId, struct Collection> collections = readCollections(plan);

  std::vector<CollectionId> collectionsToFix = findCollectionsToFix(collections);

  // TODO maybe it makes more sense to find the shards to fix instead of the collections?

  for (auto const& collectionIt : collectionsToFix) {
    for (auto const& shardIt : collections[collectionIt].shardsByName) {
      std::string const& shardName = shardIt.first;

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "[tg] fix shard " << shardName;
    }
  }

  // Phase 1:
  // Step 1.1: filter collections to fix
  // Step 1.1a: link proto collection to each collection to fix
  // Step 1.2: add collections with repairingDistributeShardsLike and corresponding proto collection
  // Step 1.3:
  // Phase 2:
  // fixShard()

  std::vector<AgencyOperation> agencyOperations = {};
  std::vector<AgencyPrecondition> agencyPreconditions = {};

  // TODO both arguments may either be a single Op/Precond or a vector. Use a single value if applicable.
  AgencyWriteTransaction trans(agencyOperations, agencyPreconditions);

  return trans;
}
