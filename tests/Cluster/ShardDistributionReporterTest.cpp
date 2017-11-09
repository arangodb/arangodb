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
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <queue>

using namespace arangodb;
using namespace fakeit;
using namespace arangodb::cluster;
using namespace arangodb::httpclient;

static std::shared_ptr<VPackBuilder> buildCountBody(uint64_t count) {
  auto res = std::make_shared<VPackBuilder>();
  res->openObject();
  res->add("count", VPackValue(count));
  res->close();
  return res;
}

SCENARIO("The shard distribution can be reported", "[cluster][shards]") {
  GIVEN("An instance of the Reporter") {
    Mock<ClusterComm> commMock;
    ClusterComm& cc = commMock.get();

    Mock<ClusterInfo> infoMock;
    ClusterInfo& ci = infoMock.get();

    Mock<CollectionInfoCurrent> infoCurrentMock;
    CollectionInfoCurrent& cicInst = infoCurrentMock.get();

    std::shared_ptr<CollectionInfoCurrent> cic(&cicInst,
                                               [](CollectionInfoCurrent*) {});

    Mock<LogicalCollection> colMock;
    LogicalCollection& col = colMock.get();


    // Moking HttpResults
    Mock<SimpleHttpResult> db1s2CountMock;
    SimpleHttpResult& httpdb1s2Count = db1s2CountMock.get();

    Mock<SimpleHttpResult> db1s3CountMock;
    SimpleHttpResult& httpdb1s3Count = db1s3CountMock.get();

    Mock<SimpleHttpResult> db2s2CountMock;
    SimpleHttpResult& httpdb2s2Count = db2s2CountMock.get();

    Mock<SimpleHttpResult> db3s2CountMock;
    SimpleHttpResult& httpdb3s2Count = db3s2CountMock.get();

    Mock<SimpleHttpResult> db3s3CountMock;
    SimpleHttpResult& httpdb3s3Count = db3s3CountMock.get();

    ShardDistributionReporter testee(
        std::shared_ptr<ClusterComm>(&cc, [](ClusterComm*) {}), &ci);

    WHEN("Asked with a database name") {
      std::string dbname = "UnitTestDB";
      std::string colName = "UnitTestCollection";
      std::string cidString = "1337";

      std::string s1 = "s1234";
      std::string s2 = "s2345";
      std::string s3 = "s3456";

      std::string dbserver1 = "PRMR_123_456";
      std::string dbserver2 = "PRMR_456_123";
      std::string dbserver3 = "PRMR_987_654";

      std::string dbserver1short = "DBServer1";
      std::string dbserver2short = "DBServer2";
      std::string dbserver3short = "DBServer3";
      uint64_t shard2LeaderCount = 1337;
      uint64_t shard2LowFollowerCount = 456;
      uint64_t shard2HighFollowerCount = 1111;

      uint64_t shard3LeaderCount = 4651;
      uint64_t shard3FollowerCount = 912;

      bool gotFirstRequest = false;
      CoordTransactionID cordTrxId = 0;
      uint64_t requestsInFlight = 0;

      std::queue<ClusterCommResult> responses;
      ClusterCommResult leaderS2Response;
      OperationID leaderS2Id;
      bool leaderS2Delivered = true;

      ClusterCommResult leaderS3Response;
      OperationID leaderS3Id;
      bool leaderS3Delivered = true;

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

      std::vector<ServerID> s1Current{dbserver1, dbserver2, dbserver3};
      std::vector<ServerID> s2Current{dbserver2};
      std::vector<ServerID> s3Current{dbserver3, dbserver2};

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
      When(Method(infoMock, getCollectionCurrent))
          .AlwaysDo([&](DatabaseID const& dbId, CollectionID const& cId) {
            REQUIRE(dbId == dbname);
            REQUIRE(cId == cidString);
            return cic;
          });

      When(Method(colMock, name)).AlwaysReturn(colName);
      When(
          ConstOverloadedMethod(colMock, shardIds, std::shared_ptr<ShardMap>()))
          .AlwaysReturn(shards);
      When(Method(colMock, cid_as_string)).AlwaysReturn(cidString);

      When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
        if (sid == s1) {
          return s1Current;
        }
        if (sid == s2) {
          return s2Current;
        }
        if (sid == s3) {
          return s3Current;
        }
        // Illegal!
        REQUIRE(false);
        return s1Current;
      });

      // Mocking HTTP Response
      When(ConstOverloadedMethod(db1s2CountMock, getBodyVelocyPack, std::shared_ptr<VPackBuilder> ())).AlwaysDo([&] () {
          return buildCountBody(shard2LowFollowerCount);
      });

      When(ConstOverloadedMethod(db1s3CountMock, getBodyVelocyPack, std::shared_ptr<VPackBuilder> ())).AlwaysDo([&] () {
          return buildCountBody(shard3FollowerCount);
      });

      When(ConstOverloadedMethod(db2s2CountMock, getBodyVelocyPack, std::shared_ptr<VPackBuilder> ())).AlwaysDo([&] () {
          return buildCountBody(shard2LeaderCount);
      });

      When(ConstOverloadedMethod(db3s2CountMock, getBodyVelocyPack, std::shared_ptr<VPackBuilder> ())).AlwaysDo([&] () {
          return buildCountBody(shard2HighFollowerCount);
      });

      When(ConstOverloadedMethod(db3s3CountMock, getBodyVelocyPack, std::shared_ptr<VPackBuilder> ())).AlwaysDo([&] () {
          return buildCountBody(shard3LeaderCount);
      });

      // Mocking the ClusterComm for count calls
      When(Method(commMock, asyncRequest))
          .AlwaysDo([&](
              ClientTransactionID const&,
              CoordTransactionID const coordTransactionID,
              std::string const& destination, rest::RequestType reqtype,
              std::string const& path, std::shared_ptr<std::string const> body,
              std::unique_ptr<std::unordered_map<std::string, std::string>>&
                  headerFields,
              std::shared_ptr<ClusterCommCallback> callback,
              ClusterCommTimeout timeout, bool singleRequest,
              ClusterCommTimeout initTimeout) -> OperationID {
            REQUIRE(initTimeout == -1.0);  // Default
            REQUIRE(!singleRequest);       // we want to use keep-alive
            REQUIRE(callback == nullptr);  // We actively wait
            REQUIRE(reqtype == rest::RequestType::GET);  // count is only get!
            REQUIRE(headerFields->empty());              // Nono headers

            // This feature has at most 2s to do its job
            // otherwise default values will be returned
            REQUIRE(timeout <= 2.0);

            if (!gotFirstRequest) {
              gotFirstRequest = true;
              cordTrxId = coordTransactionID;
            } else {
              // We always use the same id
              REQUIRE(cordTrxId == coordTransactionID);
            }

            OperationID opId = TRI_NewTickServer();

            ClusterCommResult response;
            response.coordTransactionID = cordTrxId;
            response.operationID = opId;
            response.answer_code = rest::ResponseCode::OK;
            response.status = CL_COMM_RECEIVED;

            // '/_api/collection/' + shard.shard + '/count'
            if (destination == "server:" + dbserver1) {
              // off-sync follows s2,s3
              if (path == "/_api/collection/" + s2 + "/count") {
                response.result = std::shared_ptr<SimpleHttpResult>(&httpdb1s2Count, [](SimpleHttpResult*) {});
              } else {
                REQUIRE(path == "/_api/collection/" + s3 + "/count");
                response.result = std::shared_ptr<SimpleHttpResult>(&httpdb1s3Count, [](SimpleHttpResult*) {});
              } 
            } else if (destination == "server:" + dbserver2) {
              // Leads s2
              REQUIRE(path == "/_api/collection/" + s2 + "/count");
              response.result = std::shared_ptr<SimpleHttpResult>(&httpdb2s2Count, [](SimpleHttpResult*) {});
              leaderS2Response = response;
              leaderS2Id = opId;
              leaderS2Delivered = false;
              return opId;
            } else if (destination == "server:" + dbserver3) {
              // Leads s3
              // off-sync follows s2
              if (path == "/_api/collection/" + s2 + "/count") {
                response.result = std::shared_ptr<SimpleHttpResult>(&httpdb3s2Count, [](SimpleHttpResult*) {});
              } else {
                REQUIRE(path == "/_api/collection/" + s3 + "/count");
                response.result = std::shared_ptr<SimpleHttpResult>(&httpdb3s3Count, [](SimpleHttpResult*) {});
                leaderS3Response = response;
                leaderS3Id = opId;
                leaderS3Delivered = false;
                return opId;
              }
            } else {
              // Unknown Server!!
              REQUIRE(false);
            }

            responses.push(response);
            return opId;
          });

      When(Method(commMock, wait))
          .AlwaysDo([&](ClientTransactionID const&,
                        CoordTransactionID const coordTransactionID,
                        OperationID const operationID, ShardID const& shardID,
                        ClusterCommTimeout timeout) {
            REQUIRE(coordTransactionID == cordTrxId);
            REQUIRE(shardID == "");     // Superfluous
            REQUIRE(timeout ==
                    0.0);  // Default, the requests has timeout already

            if (operationID == leaderS2Id && !leaderS2Delivered) {
              REQUIRE(leaderS2Id != 0);
              REQUIRE(!leaderS2Delivered);
              leaderS2Delivered = true;
              return leaderS2Response;
            }

            if (operationID == leaderS3Id && !leaderS3Delivered) {
              REQUIRE(leaderS3Id != 0);
              REQUIRE(!leaderS3Delivered);
              leaderS3Delivered = true;
              return leaderS3Response;
            }

            REQUIRE(operationID == 0);  // We do not wait for a specific one

            REQUIRE(!responses.empty());
            ClusterCommResult last = responses.front();
            responses.pop();
            return last;
          });

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
            REQUIRE(total.getNumber<uint64_t>() == shard2LeaderCount);

            VPackSlice current = progress.get("current");
            REQUIRE(current.isNumber());
            REQUIRE(current.getNumber<uint64_t>() == shard2LowFollowerCount);
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
            REQUIRE(total.getNumber<uint64_t>() == shard3LeaderCount);

            VPackSlice current = progress.get("current");
            REQUIRE(current.isNumber());
            REQUIRE(current.getNumber<uint64_t>() == shard3FollowerCount);
          }
        }
      }

      WHEN("validating current") {
        VPackSlice current = result.get("Current");
        REQUIRE(current.isObject());

        // One entry per shard
        REQUIRE(current.length() == shards->size());

        WHEN("Testing the in-sync shard") {
          VPackSlice shard = current.get(s1);

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
        }

        WHEN("Testing the off-sync shard") {
          VPackSlice shard = current.get(s2);

          REQUIRE(shard.isObject());

          THEN("It should have the correct leader shortname") {
            VPackSlice leader = shard.get("leader");

            REQUIRE(leader.isString());
            REQUIRE(leader.copyString() == dbserver2short);
          }

          THEN("It should not have any followers") {
            VPackSlice followers = shard.get("followers");
            REQUIRE(followers.isArray());
            REQUIRE(followers.length() == 0);
          }
        }

        WHEN("Testing the partial in-sync shard") {
          VPackSlice shard = current.get(s3);

          REQUIRE(shard.isObject());

          THEN("It should have the correct leader shortname") {
            VPackSlice leader = shard.get("leader");

            REQUIRE(leader.isString());
            REQUIRE(leader.copyString() == dbserver3short);
          }

          THEN("It should have the correct followers shortnames") {
            VPackSlice followers = shard.get("followers");
            REQUIRE(followers.isArray());
            REQUIRE(followers.length() == 1);

            VPackSlice firstFollower = followers.at(0);
            REQUIRE(firstFollower.isString());
            REQUIRE(firstFollower.copyString() == dbserver2short);
          }
        }
      }
    }
  }
}
}

SCENARIO("Validating count reporting on distribute shards", "[cluster][shards]") {
}


// TEST TO ADD
// We need to verify the following count reports:
// if (min(followCount) < leaderCount) => current == min(followCount)
// if (min(followCount) >= leaderCount) => current == max(followCount)
