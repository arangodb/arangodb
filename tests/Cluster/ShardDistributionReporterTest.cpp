////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cluster::ShardDistributionReporter
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "fakeit.hpp"

#include "Cluster/ClusterComm.h"
#include "Cluster/ShardDistributionReporter.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace fakeit;
using namespace arangodb::cluster;

SCENARIO("The shard distribution can be reported", "[cluster][shards]") {
  GIVEN("An instance of the Reporter") {
    Mock<ClusterComm> commMock;
    ClusterComm& cc = commMock.get();

    Mock<ClusterInfo> infoMock;
    ClusterInfo& ci = infoMock.get();

    Mock<LogicalCollection> colMock;
    LogicalCollection& col = colMock.get();

    ShardDistributionReporter testee(
        std::shared_ptr<ClusterComm>(&cc, [](ClusterComm*) {}), &ci);

    WHEN("Asked with a database name") {
      std::string dbname = "UnitTestDB";
      std::string colName = "UnitTestCollection";

      std::string s1 = "s1234";
      std::string s2 = "s2345";
      std::string s3 = "s3456";

      std::string dbserver1 = "PRMR_123_456";
      std::string dbserver2 = "PRMR_456_123";
      std::string dbserver3 = "PRMR_987_654";

      std::string dbserver1short = "DBServer1";
      std::string dbserver2short = "DBServer2";
      std::string dbserver3short = "DBServer3";

      // Simlated situation:
      // s1 is in-sync: DBServer1 <- DBServer2, DBServer3

      // s2 is off-sync: DBServer2 <- DBServer1, DBServer3

      // s3 is in- and off-sync: DBServer3 <- DBServer2 (sync), DBServer1

      // Fake the shard map
      auto shards = std::make_shared<ShardMap>();
      shards->emplace(s1,
                      std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
      shards->emplace(s2,
                      std::vector<ServerID>{dbserver2, dbserver1, dbserver3});
      shards->emplace(s3,
                      std::vector<ServerID>{dbserver3, dbserver1, dbserver2});

      // Fake the aliases
      auto aliases = std::unordered_map<ServerID, std::string>{
          {dbserver1, dbserver1short},
          {dbserver2, dbserver2short},
          {dbserver3, dbserver3short}};

      // Fake the collections
      std::vector<std::shared_ptr<LogicalCollection>> allCollections;
      allCollections.emplace_back(
          std::shared_ptr<LogicalCollection>(&col, [](LogicalCollection*) {}));

      // Now we fake the calls
      When(Method(infoMock, getCollections))
          .AlwaysDo([&](DatabaseID const& dbId) {
            REQUIRE(dbId == dbname);
            return allCollections;
          });
      When(Method(infoMock, getServerAliases)).AlwaysReturn(aliases);

      When(Method(colMock, name)).AlwaysReturn(colName);
      When(
          ConstOverloadedMethod(colMock, shardIds, std::shared_ptr<ShardMap>()))
          .AlwaysReturn(shards);

      VPackBuilder resultBuilder;
      testee.getDistributionForDatabase(dbname, resultBuilder);

      VPackSlice result = resultBuilder.slice();

      THEN("It should return an object") { REQUIRE(result.isObject()); }

      THEN("It should return one entry for every collection") {
        VPackSlice col = result.get(colName);
        REQUIRE(col.isObject());
      }

      WHEN("Checking one of those collections") {
        result = result.get(colName);
        REQUIRE(result.isObject());

        WHEN("validating the plan") {
          VPackSlice plan = result.get("Plan");
          REQUIRE(plan.isObject());

          // One entry per shard
          REQUIRE(plan.length() == shards->size());

          WHEN("Testing the in-sync shard") {
            VPackSlice shard = plan.get(s1);

            REQUIRE(shard.isObject());

            THEN("It should have the correct leader shortname") {
              VPackSlice leader = shard.get("leader");

              REQUIRE(leader.isString());
              REQUIRE(leader.copyString() == dbserver1short);
            }

            THEN("It should have the correct followers shortnames") {
              VPackSlice followers = shard.get("followers");
              REQUIRE(followers.isArray());
              REQUIRE(followers.length() == 2);

              VPackSlice firstFollower = followers.at(0);
              REQUIRE(firstFollower.isString());

              VPackSlice secondFollower = followers.at(1);
              REQUIRE(secondFollower.isString());

              // We do not guarentee any ordering here
              if (StringRef(firstFollower) == dbserver2short) {
                REQUIRE(secondFollower.copyString() == dbserver3short);
              } else {
                REQUIRE(firstFollower.copyString() == dbserver3short);
                REQUIRE(secondFollower.copyString() == dbserver2short);
              }
            }

            THEN("It should not display progress") {
              VPackSlice progress = shard.get("progress");
              REQUIRE(progress.isNone());
            }
          }

          WHEN("Testing the off-sync shard") {
            VPackSlice shard = plan.get(s2);

            REQUIRE(shard.isObject());

            THEN("It should have the correct leader shortname") {
              VPackSlice leader = shard.get("leader");

              REQUIRE(leader.isString());
              REQUIRE(leader.copyString() == dbserver2short);
            }

            THEN("It should have the correct followers shortnames") {
              VPackSlice followers = shard.get("followers");
              REQUIRE(followers.isArray());
              REQUIRE(followers.length() == 2);

              VPackSlice firstFollower = followers.at(0);
              REQUIRE(firstFollower.isString());

              VPackSlice secondFollower = followers.at(1);
              REQUIRE(secondFollower.isString());

              // We do not guarentee any ordering here
              if (StringRef(firstFollower) == dbserver1short) {
                REQUIRE(secondFollower.copyString() == dbserver3short);
              } else {
                REQUIRE(firstFollower.copyString() == dbserver3short);
                REQUIRE(secondFollower.copyString() == dbserver1short);
              }
            }

            THEN("It should display the progress") {
              VPackSlice progress = shard.get("progress");
              REQUIRE(progress.isObject());

              VPackSlice total = progress.get("total");
              REQUIRE(total.isNumber());

              VPackSlice current = progress.get("current");
              REQUIRE(current.isNumber());

              // TODO
              REQUIRE(false);
            }
          }

          WHEN("Testing the partial in-sync shard") {
            VPackSlice shard = plan.get(s3);

            REQUIRE(shard.isObject());

            THEN("It should have the correct leader shortname") {
              VPackSlice leader = shard.get("leader");

              REQUIRE(leader.isString());
              REQUIRE(leader.copyString() == dbserver3short);
            }

            THEN("It should have the correct followers shortnames") {
              VPackSlice followers = shard.get("followers");
              REQUIRE(followers.isArray());
              REQUIRE(followers.length() == 2);

              VPackSlice firstFollower = followers.at(0);
              REQUIRE(firstFollower.isString());

              VPackSlice secondFollower = followers.at(1);
              REQUIRE(secondFollower.isString());

              // We do not guarentee any ordering here
              if (StringRef(firstFollower) == dbserver1short) {
                REQUIRE(secondFollower.copyString() == dbserver2short);
              } else {
                REQUIRE(firstFollower.copyString() == dbserver2short);
                REQUIRE(secondFollower.copyString() == dbserver1short);
              }
            }

            THEN("It should display the progress") {
              VPackSlice progress = shard.get("progress");
              REQUIRE(progress.isObject());

              VPackSlice total = progress.get("total");
              REQUIRE(total.isNumber());

              VPackSlice current = progress.get("current");
              REQUIRE(current.isNumber());

              // TODO
              REQUIRE(false);
            }
          }
        }

        WHEN("validating current") {
          VPackSlice current = result.get("Current");
          REQUIRE(current.isObject());

          // One entry per shard
          REQUIRE(current.length() == shards->size());
        }
      }
    }
  }
}
