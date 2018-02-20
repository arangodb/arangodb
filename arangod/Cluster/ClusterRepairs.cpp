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
#include "ClusterInfo.h"
#include "velocypack/Iterator.h"

#include <boost/optional.hpp>
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
  boost::optional<std::string> distributeShardsLike;
  boost::optional<std::string> repairingDistributeShardsLike;
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
      boost::optional<std::string> distributeShardsLike, repairingDistributeShardsLike;
      if (collectionSlice.hasKey("distributeShardsLike")) {
        distributeShardsLike = collectionSlice.get("distributeShardsLike").copyString();
      }
      if (collectionSlice.hasKey("repairingDistributeShardsLike")) {
        repairingDistributeShardsLike = collectionSlice.get("repairingDistributeShardsLike").copyString();
      }

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "    collection: " << collectionId << " / " << collectionName;

      if (distributeShardsLike) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "    distributeShardsLike: " << distributeShardsLike.get();
      }

      struct Collection collection = {
        databaseId,
        collectionName,
        distributeShardsLike,
        repairingDistributeShardsLike,
        {}
      };

      collections.emplace(std::make_pair(collectionId, collection));

      std::map<std::string, DBServers, VersionSort> &shards = collection.shardsByName;

      Slice const &shardsSlice = collectionSlice.get("shards");

      for (auto const &shardIterator : ObjectIterator(shardsSlice)) {
        std::string const shardId = shardIterator.key.copyString();
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "      shard: " << shardId;

        DBServers dbServers;
        shards.emplace(std::make_pair(shardId, dbServers));

        for (auto const &dbServerIterator : ArrayIterator(shardIterator.value)) {
          std::string const dbServerId = dbServerIterator.copyString();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "        dbserver: " << dbServerId;

          dbServers.emplace_back(dbServerId);
        }

      }

    }

  }

  return collections;
}

AgencyWriteTransaction
ClusterRepairs::repairDistributeShardsLike(VPackSlice &&plan) {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
  << "[tg] ClusterMethods::repairDistributeShardsLike()";

  std::map<CollectionId, struct Collection> collections = readCollections(plan);

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
