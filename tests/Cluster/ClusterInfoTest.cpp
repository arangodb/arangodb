////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/AgencyCache.h"
#include "Mocks/Servers.h"

using namespace arangodb;

struct ClusterInfoTest : public ::testing::Test {
  ClusterInfoTest() 
      : server() {}
  
  arangodb::tests::mocks::MockCoordinator server;
};

TEST_F(ClusterInfoTest, testServerExists) {
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();

  // no servers present
  ASSERT_FALSE(ci.serverExists(""));
  ASSERT_FALSE(ci.serverExists("foo"));
  ASSERT_FALSE(ci.serverExists("bar"));
  ASSERT_FALSE(ci.serverExists("PRMR-abcdef-1090595"));

  // populate some servers
  {
    std::unordered_map<ServerID, std::string> servers;
    servers.emplace("PRMR-012345-678", "testi");
    servers.emplace("PRMR-012345-123", "testmann");
    ci.setServers(std::move(servers));
    
    ASSERT_TRUE(ci.serverExists("PRMR-012345-678"));
    ASSERT_TRUE(ci.serverExists("PRMR-012345-123"));
    ASSERT_FALSE(ci.serverExists("PRMR-012345-1234"));
    ASSERT_FALSE(ci.serverExists("PRMR-12345-123"));

    ASSERT_FALSE(ci.serverExists("testi"));
    ASSERT_FALSE(ci.serverExists("testmann"));
    ASSERT_FALSE(ci.serverExists(""));
    ASSERT_FALSE(ci.serverExists("foo"));
    ASSERT_FALSE(ci.serverExists("bar"));
    ASSERT_FALSE(ci.serverExists("PRMR-abcdef-1090595"));
  }

  {
    // flush servers map once more
    ci.setServers({});
    
    ASSERT_FALSE(ci.serverExists("PRMR-012345-678"));
    ASSERT_FALSE(ci.serverExists("PRMR-012345-123"));
    ASSERT_FALSE(ci.serverExists("testi"));
    ASSERT_FALSE(ci.serverExists("testmann"));
    ASSERT_FALSE(ci.serverExists(""));
    ASSERT_FALSE(ci.serverExists("foo"));
    ASSERT_FALSE(ci.serverExists("bar"));
    ASSERT_FALSE(ci.serverExists("PRMR-abcdef-1090595"));
  }
}

TEST_F(ClusterInfoTest, testServerAliasExists) {
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();

  // no aliases present
  ASSERT_FALSE(ci.serverAliasExists(""));
  ASSERT_FALSE(ci.serverAliasExists("foo"));
  ASSERT_FALSE(ci.serverAliasExists("bar"));
  ASSERT_FALSE(ci.serverAliasExists("PRMR-abcdef-1090595"));

  // populate some aliases
  {
    std::unordered_map<ServerID, std::string> aliases;
    aliases.emplace("DBServer0001", "PRMR-012345-678");
    aliases.emplace("DBServer0002", "PRMR-9999-666");
    ci.setServerAliases(std::move(aliases));
    
    ASSERT_TRUE(ci.serverAliasExists("DBServer0001"));
    ASSERT_TRUE(ci.serverAliasExists("DBServer0002"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0003"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0000"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer00001"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-012345-678"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-9999-666"));
  }

  {
    // flush aliases map once more
    ci.setServerAliases({});
    
    ASSERT_FALSE(ci.serverAliasExists("DBServer0001"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0002"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0003"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0000"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer00001"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-012345-678"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-9999-666"));
  }
}

TEST_F(ClusterInfoTest, plan_will_provide_latest_id) {
  auto &cache {server.getFeature<ClusterFeature>().agencyCache()};
  auto [acb, index] {cache.read({AgencyCommHelper::path("Sync/LatestID")})};
  auto expectedLatestId {acb->slice().at(0).get("arango").get("Sync").get("LatestID").getInt()};
  auto& ci {server.getFeature<arangodb::ClusterFeature>().clusterInfo()};
  auto builder {std::make_shared<VPackBuilder>()};
  auto result {ci.agencyPlan(builder)};
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(builder->slice().at(0).get("arango").get("Sync").get("LatestID").getInt(), expectedLatestId);
}

