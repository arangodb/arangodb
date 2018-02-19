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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;


AgencyWriteTransaction
ClusterRepairs::repairDistributeShardsLike(VPackSlice &&plan) {
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
    << "[tg] ClusterMethods::repairDistributeShardsLike()";

  Slice const& collectionsByDatabase = plan.get("Collections");

  for (auto const& databaseIterator : ObjectIterator(collectionsByDatabase)) {
    std::string const databaseId = databaseIterator.key.copyString();

    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
    << "  db: " << databaseId;

    Slice const& collections = databaseIterator.value;

    for (auto const& collectionIterator : ObjectIterator(collections)) {
      std::string const collectionId = collectionIterator.key.copyString();
      Slice const& collection = collectionIterator.value;
      std::string const collectionName = collection.get("name").copyString();
      boost::optional<std::string> distributeShardsLike;
      if (collection.hasKey("distributeShardsLike")) {
        distributeShardsLike = collection.get("distributeShardsLike").copyString();
      }

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "    collection: " << collectionId << " / " << collectionName;

      if (distributeShardsLike) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "    distributeShardsLike: " << distributeShardsLike.get();
      }

      Slice const &shards = collection.get("shards");

      for (auto const& shardIterator : ObjectIterator(shards)) {
        std::string const shardId = shardIterator.key.copyString();
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "      shard: " << shardId;

        for (auto const& dbServerIterator : ArrayIterator(shardIterator.value)) {
          std::string const dbServerId = dbServerIterator.copyString();
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "        dbserver: " << dbServerId;
        }

      }

    }

  }

//  for(auto const& db : ci->databases(true)) {
//    for(auto const& collection : ci->getCollections(db)) {
//      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
//        << db << "/" << collection;
//    }
//  }

  std::vector<AgencyOperation> agencyOperations = {};
  std::vector<AgencyPrecondition> agencyPreconditions = {};

  // TODO both arguments may either be a single Op/Precond or a vector. Use a single value if applicable.
  AgencyWriteTransaction trans(agencyOperations, agencyPreconditions);

  return trans;
}
