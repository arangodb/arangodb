////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ShardOrganizer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "fakeit.hpp"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ShardOrganizer.h"

#include <string>
#include <vector>

using namespace arangodb;
using namespace fakeit;

TEST_CASE("creating shardmaps", "[cluster][shardorganizer]") {
  Mock<ClusterInfo> mockClusterInfo;
  When(Method(mockClusterInfo, uniqid)).AlwaysReturn(1);
  

SECTION("without having any server creating a shardMap should bail out") {
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(0);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({});
  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);

  REQUIRE(result.errorNumber() == TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
}

SECTION("without having enough servers creating a shardMap should bail out") {
  When(Method(mockClusterInfo, getCid)).AlwaysDo([&](std::string const& database, std::string const& distributeShardsLike) -> TRI_voc_cid_t {
    return 0;
  });
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysDo([&]() -> std::vector<std::string> {
    return {};
  });
  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);

  REQUIRE(result.errorNumber() == TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
}

SECTION("creating a shardMap for a system collection should not bail out even if there are not enough servers") {
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(0);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({});


  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.softReplicationFactor(true);
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);

  INFO(result.errorNumber());
  REQUIRE(result.ok());
}

SECTION("creating a collection with distribute shards like should create the collection with the same servers") {
  ShardMap reference;
  reference.emplace("testi", std::vector<std::string>({"wursti", "warzi"}));
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(1);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({"test", "test2", "test3"});
  When(Method(mockClusterInfo, hasDistributeShardsLike)).AlwaysReturn(false);
  When(Method(mockClusterInfo, getShardMap)).AlwaysReturn(std::make_shared<ShardMap>(reference));

  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.distributeShardsLike("test");
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);

  INFO(result.errorNumber());
  REQUIRE(result.ok());
}

SECTION("creating a collection with distribute shards like should create the collection with the same servers even when there are not enough servers") {
  ShardMap reference;
  reference.emplace("testi", std::vector<std::string>({"wursti", "warzi"}));
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(1);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({});
  When(Method(mockClusterInfo, hasDistributeShardsLike)).AlwaysReturn(false);
  When(Method(mockClusterInfo, getShardMap)).AlwaysReturn(std::make_shared<ShardMap>(reference));

  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.distributeShardsLike("test");
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);
  
  INFO(result.errorNumber());
  REQUIRE(result.ok());
  REQUIRE(result.resultShards->size() == reference.size());
}

SECTION("creating a collection with distribute shards like should bail out if the master isn't found") {
  ShardMap reference;
  reference.emplace("testi", std::vector<std::string>({"wursti", "warzi"}));
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(0);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({});
  When(Method(mockClusterInfo, hasDistributeShardsLike)).AlwaysReturn(false);
  When(Method(mockClusterInfo, getShardMap)).AlwaysReturn(std::make_shared<ShardMap>(reference));

  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.distributeShardsLike("test");
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);
  
  INFO(result.errorNumber());
  REQUIRE(result.fail());
  REQUIRE(result.errorNumber() == TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE);
}

SECTION("creating a collection with distribute shards like should create it like normal if the master isn't found and ignoreErrors is enabled") {
  ShardMap reference;
  reference.emplace("testi", std::vector<std::string>({"wursti", "warzi"}));
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(0);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({"testi","hasti", "knacksi"});
  When(Method(mockClusterInfo, hasDistributeShardsLike)).AlwaysReturn(false);
  When(Method(mockClusterInfo, getShardMap)).AlwaysReturn(std::make_shared<ShardMap>(reference));

  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.distributeShardsLike("test");
  settings.createIndependentOnShardsLikeError(true);
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);
  
  INFO(result.errorNumber());
  REQUIRE(result.ok());
  REQUIRE(result.resultShards->size() == settings.numberOfShards());
  REQUIRE(result.resultShards->begin()->second.size() == settings.replicationFactor());
}

SECTION("avoiding servers should be a soft option and if the replication factor is bigger then it should be ignored") {
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(0);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({"testi", "hasti"});
  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.avoidServers({"testi"});
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);

  INFO(result.errorNumber());
  REQUIRE(result.ok());
}

SECTION("trying to create a shardMap like another should bail out if the master also has 'distributeShardsLike'") {
  ShardMap reference;
  reference.emplace("testi", std::vector<std::string>({"wursti", "warzi"}));
  When(Method(mockClusterInfo, getCid)).AlwaysReturn(1);
  When(Method(mockClusterInfo, getCurrentDBServers)).AlwaysReturn({});
  When(Method(mockClusterInfo, hasDistributeShardsLike)).AlwaysReturn(true);
  When(Method(mockClusterInfo, getShardMap)).AlwaysReturn(std::make_shared<ShardMap>(reference));

  auto& clusterInfo = mockClusterInfo.get();

  ShardingSettings settings;
  settings.replicationFactor(2);
  settings.numberOfShards(4);
  settings.distributeShardsLike("test");
  
  ShardOrganizer organizer(&clusterInfo);
  auto result = organizer.createShardMap(settings);
  
  INFO(result.errorNumber());
  REQUIRE(result.fail());
  REQUIRE(result.errorNumber() == TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE);
}

  //ClusterInfo::cleanup();
}