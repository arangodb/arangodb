////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/Utils/PlanCollectionEntry.h"
#include "VocBase/Properties/CreateCollectionBody.h"

#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {

namespace {

ServerID generateServerName(uint64_t shardIndex, uint64_t replicationIndex,
                            uint64_t shuffle) {
  std::string start = (replicationIndex == 0)
                          ? "LEADER"
                          : "FOLLOWER_" + std::to_string(replicationIndex);
  return start + "_s_" + std::to_string(shardIndex) + "_gen_" +
         std::to_string(shuffle);
}

// This test Shard distribution will encode Leader/Follower, and amount of
// shuffles in server names For easier debugging
struct TestShardDistribution : public IShardDistributionFactory {
  TestShardDistribution(uint64_t numberOfShards, uint64_t replicationFactor) {
    _shardToServerMapping.reserve(numberOfShards);
    for (uint64_t s = 0; s < numberOfShards; ++s) {
      std::vector<ServerID> servers;
      for (uint64_t r = 0; r < replicationFactor; ++r) {
        servers.emplace_back(generateServerName(s, r, 0));
      }
      _shardToServerMapping.emplace_back(
          ResponsibleServerList{std::move(servers)});
    }
  }

  auto planShardsOnServers(std::vector<ServerID>,
                           std::unordered_set<ServerID>& serversPlanned)
      -> Result override {
    // We increase the index of the shuffle generation.
    ++_shuffleGeneration;
    for (uint64_t s = 0; s < _shardToServerMapping.size(); ++s) {
      ResponsibleServerList& servers = _shardToServerMapping[s];
      for (uint64_t r = 0; r < servers.servers.size(); ++r) {
        // We fake all servers here, and do not use the handed in list
        servers.servers[r] = generateServerName(s, r, _shuffleGeneration);
        serversPlanned.emplace(servers.servers[r]);
      }
      _shardToServerMapping.emplace_back(std::move(servers));
    }
    return {};
  }

 private:
  uint64_t _shuffleGeneration{0};
};
}  // namespace

class PlanCollectionEntryTest : public ::testing::Test {
 protected:
  static std::vector<ShardID> generateShardNames(uint64_t numberOfShards,
                                                 uint64_t idOffset = 0) {
    std::vector<ShardID> result;
    result.reserve(numberOfShards);
    for (uint64_t i = 0; i < numberOfShards; ++i) {
      result.emplace_back("s" + std::to_string(numberOfShards + idOffset));
    }
    return result;
  }

  static CreateCollectionBody prepareMinimalCollection(
      uint64_t nrShards, uint64_t replicationFactor) {
    CreateCollectionBody col{};
    col.name = "test";
    col.id = DataSourceId(123);
    col.numberOfShards = nrShards;
    col.replicationFactor = replicationFactor;
    return col;
  }
};

TEST_F(PlanCollectionEntryTest, default_values) {
  auto col = prepareMinimalCollection(1, 1);
  auto numberOfShards = col.numberOfShards.value();
  auto distProto = std::make_shared<TestShardDistribution>(
      numberOfShards, col.replicationFactor.value());
  auto shards = generateShardNames(numberOfShards);
  ShardDistribution dist{shards, distProto};
  AgencyIsBuildingFlags buildingFlags;
  buildingFlags.rebootId = RebootId(42);
  buildingFlags.coordinatorName = "CRDN_123";
  PlanCollectionEntry entry{col, std::move(dist), std::move(buildingFlags)};
  auto builder = entry.toVPackDeprecated();
  LOG_DEVEL << builder.toJson();
}

}  // namespace arangodb::tests
