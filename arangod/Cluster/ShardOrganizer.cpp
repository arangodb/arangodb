////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ShardOrganizer.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"

using namespace arangodb;
using namespace arangodb::basics;

ShardingResult ShardOrganizer::createShardMap(ShardingSettings /*const&*/ settings) { // grrr
  std::string distributeShardsLike = settings.distributeShardsLike();
  ShardingResult result;
  if (!distributeShardsLike.empty()) {
    TRI_voc_cid_t otherCid = 0;
    try {
      otherCid = _ci->getCid(settings.databaseName(), distributeShardsLike);
    } catch (Exception const& e) {
      if (e.code() != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        throw;
      }
    }
    std::string otherCidString = arangodb::basics::StringUtils::itoa(otherCid);

    result = createShardMap(settings.databaseName(), otherCidString);
    result.resultSettings = settings;
    result.resultSettings.distributeShardsLike(otherCidString);
    if (result.fail() && settings.createIndependentOnShardsLikeError()) {
      result = createShardMap(settings.numberOfShards(), settings.replicationFactor(),
        _ci->getCurrentDBServers(), settings.avoidServers(), settings.softReplicationFactor());
      result.resultSettings.distributeShardsLike("");
    }
  } else {
    result = createShardMap(settings.numberOfShards(), settings.replicationFactor(),
      _ci->getCurrentDBServers(), settings.avoidServers(), settings.softReplicationFactor());
    result.resultSettings = settings;
  }
  return result;
}

ShardingResult ShardOrganizer::createShardMap(std::string const& databaseName, std::string const& otherCidString) {
  ShardMapPtr shards;
  if (otherCidString != "0") {
    if (_ci->hasDistributeShardsLike(databaseName, otherCidString)) {
      return ShardingResult(TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE);
    }
    auto referenceShards = _ci->getShardServerList(databaseName, otherCidString);

    uint64_t const id = _ci->uniqid(referenceShards.size());
    ShardMap myShards;
    size_t i {0};
    for (auto const& it: referenceShards) {
      myShards.emplace("s" + StringUtils::itoa(id + (i++)), it.second);
    }
    shards = std::make_shared<ShardMap>(myShards);
  } else {
    return ShardingResult(TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE);
  }

  ShardingResult result;
  result.resultShards = shards;
  return result;
}

ShardingResult ShardOrganizer::createShardMap(uint64_t const& numberOfShards, uint64_t replicationFactor,
      std::vector<std::string> dbServers, std::vector<std::string> const& avoid, bool softReplicationFactor) {
  // cluster system replicationFactor is 1...allow startup with 1 DBServer.
  // an addFollower job will be spawned right away and ensure proper
  // resilience as soon as another DBServer is available :S
  // the default behaviour however is to bail out and inform the user
  // that the requested replicationFactor is not possible right now
  if (dbServers.size() < replicationFactor && !softReplicationFactor) {
    return ShardingResult(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
  }

  if (!avoid.empty()) {
    if (dbServers.size() - avoid.size() >= replicationFactor) {
      dbServers.erase(
        std::remove_if(
          dbServers.begin(), dbServers.end(), [&](const std::string&x) {
            return std::find(avoid.begin(), avoid.end(), x) != avoid.end();
          }), dbServers.end());
    }
  }

  std::random_shuffle(dbServers.begin(), dbServers.end());
  ShardingResult result;
  result.resultShards = std::make_shared<ShardOrganizer::ShardMap>(
    distributeShards(numberOfShards, replicationFactor, dbServers)
  );
  return result;
}

ShardOrganizer::ShardMap ShardOrganizer::distributeShards(
    uint64_t numberOfShards,
    uint64_t replicationFactor,
    std::vector<std::string> const& dbServers) {
  
  std::unordered_map<std::string, std::vector<std::string>> shards;

  // mop: distribute satellite collections on all servers
  if (replicationFactor == 0) {
    replicationFactor = dbServers.size();
  }

  // fetch a unique id for each shard to create
  uint64_t const id = _ci->uniqid(numberOfShards);
  
  size_t leaderIndex = 0;
  size_t followerIndex = 0;
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    // determine responsible server(s)
    std::vector<std::string> serverIds;
    for (uint64_t j = 0; j < replicationFactor; ++j) {
      if (j >= dbServers.size()) {
        LOG_TOPIC(WARN, Logger::CLUSTER)
          << "createCollectionCoordinator: replicationFactor is "
          << "too large for the number of DBservers";
        break;
      }
      std::string candidate;
      // mop: leader
      if (serverIds.size() == 0) {
        candidate = dbServers[leaderIndex++];
        if (leaderIndex >= dbServers.size()) {
          leaderIndex = 0;
        }
      } else {
        do {
          candidate = dbServers[followerIndex++];
          if (followerIndex >= dbServers.size()) {
            followerIndex = 0;
          }
        } while (candidate == serverIds[0]); // mop: ignore leader
      }
      serverIds.push_back(candidate);
    }

    // determine shard id
    std::string shardId = "s" + StringUtils::itoa(id + i);

    shards.emplace(shardId, serverIds);
  }

  return shards;
}