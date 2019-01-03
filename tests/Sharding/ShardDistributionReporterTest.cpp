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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterComm.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardDistributionReporter.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "tests/IResearch/StorageEngineMock.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <thread>
#include <queue>

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::httpclient;

static void VerifyAttributes(VPackSlice result, std::string const& colName,
                          std::string const& sName) {
  REQUIRE(result.isObject());

  VPackSlice col = result.get(colName);
  REQUIRE(col.isObject());

  VPackSlice plan = col.get("Plan");
  REQUIRE(plan.isObject());

  VPackSlice shard = plan.get(sName);
  REQUIRE(shard.isObject());
}

static void VerifyNumbers(VPackSlice result, std::string const& colName,
                          std::string const& sName, uint64_t testTotal,
                          uint64_t testCurrent) {
  REQUIRE(result.isObject());

  VPackSlice col = result.get(colName);
  REQUIRE(col.isObject());

  VPackSlice plan = col.get("Plan");
  REQUIRE(plan.isObject());

  VPackSlice shard = plan.get(sName);
  REQUIRE(shard.isObject());

  VPackSlice progress = shard.get("progress");
  REQUIRE(progress.isObject());

  VPackSlice total = progress.get("total");
  REQUIRE(total.isNumber());
  REQUIRE(total.getNumber<uint64_t>() == testTotal);

  VPackSlice current = progress.get("current");
  REQUIRE(current.isNumber());
  REQUIRE(current.getNumber<uint64_t>() == testCurrent);
}

static std::shared_ptr<VPackBuilder> buildCountBody(uint64_t count) {
  auto res = std::make_shared<VPackBuilder>();
  res->openObject();
  res->add("count", VPackValue(count));
  res->close();
  return res;
}

SCENARIO("The shard distribution can be reported", "[cluster][shards]") {
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  StorageEngineMock engine(server);
  arangodb::EngineSelectorFeature::ENGINE = &engine;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for TRI_vocbase_t::dropCollection(...)
  features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t instantiation
    
  for (auto& f: features) {
    arangodb::application_features::ApplicationServer::server->addFeature(f.first);
  } 

  for (auto& f: features) {
    f.first->prepare();
  }

  fakeit::Mock<ClusterComm> commMock;
  ClusterComm& cc = commMock.get();

  fakeit::Mock<ClusterInfo> infoMock;
  ClusterInfo& ci = infoMock.get();

  fakeit::Mock<CollectionInfoCurrent> infoCurrentMock;
  CollectionInfoCurrent& cicInst = infoCurrentMock.get();

  std::shared_ptr<CollectionInfoCurrent> cic(&cicInst,
                                             [](CollectionInfoCurrent*) {});

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
  
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto json = arangodb::velocypack::Parser::fromJson("{ \"cid\" : \"1337\", \"name\": \"UnitTestCollection\" }");
  arangodb::LogicalCollection col(vocbase, json->slice(), true);

  // Fake the aliases
  auto aliases =
      std::unordered_map<ServerID, std::string>{{dbserver1, dbserver1short},
                                                {dbserver2, dbserver2short},
                                                {dbserver3, dbserver3short}};

  // Fake the shard map
  auto shards = std::make_shared<ShardMap>();
  ShardMap currentShards;
  
  col.setShardMap(shards);

  // Fake the collections
  std::vector<std::shared_ptr<LogicalCollection>> allCollections;
  
  // Now we fake the calls
  fakeit::When(Method(infoMock, getCollections)).AlwaysDo([&](DatabaseID const& dbId) {
    REQUIRE(dbId == dbname);
    return allCollections;
  });
  // Now we fake the single collection call
  fakeit::When(Method(infoMock, getCollection)).AlwaysDo([&](DatabaseID const& dbId, CollectionID const& colId) {
    REQUIRE(dbId == dbname);
    // REQUIRE(colId == colName);
    REQUIRE(allCollections.size() > 0);

    for (auto const& c : allCollections) {
      if (c->name() == colId) {
        return c;
      }
    }

    REQUIRE(false);
    return std::shared_ptr<LogicalCollection>(nullptr);
  });
  fakeit::When(Method(infoMock, getServerAliases)).AlwaysReturn(aliases);
  fakeit::When(Method(infoMock, getCollectionCurrent).Using(dbname, cidString))
      .AlwaysDo([&](DatabaseID const& dbId, CollectionID const& cId) {
        REQUIRE(dbId == dbname);
        REQUIRE(cId == cidString);
        return cic;
      });

  ShardDistributionReporter testee(
      std::shared_ptr<ClusterComm>(&cc, [](ClusterComm*) {}), &ci);

  GIVEN("A healthy instance") {

    GIVEN("A single collection of three shards, and 3 replicas") {
      // Simlated situation:
      // s1 is in-sync: DBServer1 <- DBServer2, DBServer3

      // s2 is off-sync: DBServer2 <- DBServer1, DBServer3

      // s3 is in- and off-sync: DBServer3 <- DBServer2 (sync), DBServer1

      uint64_t shard2LeaderCount = 1337;
      uint64_t shard2LowFollowerCount = 456;
      uint64_t shard2HighFollowerCount = 1111;

      uint64_t shard3LeaderCount = 4651;
      uint64_t shard3FollowerCount = 912;

      // Moking HttpResults
      fakeit::Mock<SimpleHttpResult> db1s2CountMock;
      SimpleHttpResult& httpdb1s2Count = db1s2CountMock.get();

      fakeit::Mock<SimpleHttpResult> db1s3CountMock;
      SimpleHttpResult& httpdb1s3Count = db1s3CountMock.get();

      fakeit::Mock<SimpleHttpResult> db2s2CountMock;
      SimpleHttpResult& httpdb2s2Count = db2s2CountMock.get();

      fakeit::Mock<SimpleHttpResult> db3s2CountMock;
      SimpleHttpResult& httpdb3s2Count = db3s2CountMock.get();

      fakeit::Mock<SimpleHttpResult> db3s3CountMock;
      SimpleHttpResult& httpdb3s3Count = db3s3CountMock.get();

      bool gotFirstRequest = false;
      CoordTransactionID cordTrxId = 0;

      std::queue<ClusterCommResult> responses;
      ClusterCommResult leaderS2Response;
      OperationID leaderS2Id;
      bool leaderS2Delivered = true;

      ClusterCommResult leaderS3Response;
      OperationID leaderS3Id;
      bool leaderS3Delivered = true;

      shards->emplace(s1,
                      std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
      shards->emplace(s2,
                      std::vector<ServerID>{dbserver2, dbserver1, dbserver3});
      shards->emplace(s3,
                      std::vector<ServerID>{dbserver3, dbserver1, dbserver2});
  
      col.setShardMap(shards);

      currentShards.emplace(
          s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
      currentShards.emplace(s2, std::vector<ServerID>{dbserver2});
      currentShards.emplace(s3, std::vector<ServerID>{dbserver3, dbserver2});

      allCollections.emplace_back(
          std::shared_ptr<LogicalCollection>(&col, [](LogicalCollection*) {}));

      fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
        REQUIRE(((sid == s1) || (sid == s2) || (sid == s3)));
        return currentShards[sid];
      });

      // Mocking HTTP Response
      fakeit::When(ConstOverloadedMethod(db1s2CountMock, getBodyVelocyPack,
                                 std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(shard2LowFollowerCount); });

      fakeit::When(ConstOverloadedMethod(db1s3CountMock, getBodyVelocyPack,
                                 std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(shard3FollowerCount); });

      fakeit::When(ConstOverloadedMethod(db2s2CountMock, getBodyVelocyPack,
                                 std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(shard2LeaderCount); });

      fakeit::When(ConstOverloadedMethod(db3s2CountMock, getBodyVelocyPack,
                                 std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(shard2HighFollowerCount); });

      fakeit::When(ConstOverloadedMethod(db3s3CountMock, getBodyVelocyPack,
                                 std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(shard3LeaderCount); });

      // Mocking the ClusterComm for count calls
      fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                  std::string const& destination, rest::RequestType reqtype,
                  std::string const& path,
                  std::shared_ptr<std::string const> body,
                  std::unordered_map<std::string, std::string> const& headerFields,
                  std::shared_ptr<ClusterCommCallback> callback,
                  ClusterCommTimeout timeout, bool singleRequest,
                  ClusterCommTimeout initTimeout) -> OperationID {
                REQUIRE(initTimeout == -1.0);  // Default
                REQUIRE(!singleRequest);       // we want to use keep-alive
                REQUIRE(callback == nullptr);  // We actively wait
                REQUIRE(reqtype ==
                        rest::RequestType::GET);  // count is only get!
                REQUIRE(headerFields.empty());   // Nono headers

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

                // '/_db/UnitTestDB/_api/collection/' + shard.shard + '/count'
                if (destination == "server:" + dbserver1) {
                  // off-sync follows s2,s3
                  if (path == "/_db/UnitTestDB/_api/collection/" + s2 + "/count") {
                    response.result = std::shared_ptr<SimpleHttpResult>(
                        &httpdb1s2Count, [](SimpleHttpResult*) {});
                  } else {
                    REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s3 + "/count");
                    response.result = std::shared_ptr<SimpleHttpResult>(
                        &httpdb1s3Count, [](SimpleHttpResult*) {});
                  }
                } else if (destination == "server:" + dbserver2) {
                  // Leads s2
                  REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s2 + "/count");
                  response.result = std::shared_ptr<SimpleHttpResult>(
                      &httpdb2s2Count, [](SimpleHttpResult*) {});
                  leaderS2Response = response;
                  leaderS2Id = opId;
                  leaderS2Delivered = false;
                  return opId;
                } else if (destination == "server:" + dbserver3) {
                  // Leads s3
                  // off-sync follows s2
                  if (path == "/_db/UnitTestDB/_api/collection/" + s2 + "/count") {
                    response.result = std::shared_ptr<SimpleHttpResult>(
                        &httpdb3s2Count, [](SimpleHttpResult*) {});
                  } else {
                    REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s3 + "/count");
                    response.result = std::shared_ptr<SimpleHttpResult>(
                        &httpdb3s3Count, [](SimpleHttpResult*) {});
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

      fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                        OperationID const operationID, ShardID const& shardID,
                        ClusterCommTimeout timeout) {
            REQUIRE(coordTransactionID == cordTrxId);
            REQUIRE(shardID == "");  // Superfluous
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
      WHEN("testing distribution for database") {
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

              THEN("It should not display the progress") {
                VPackSlice progress = shard.get("progress");
                REQUIRE(progress.isNone());
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

              THEN("It should not display the progress") {
                VPackSlice progress = shard.get("progress");
                REQUIRE(progress.isNone());
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

      WHEN("testing distribution for collection database") {
        testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
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

              THEN("It should not display the progress") {
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

  WHEN("testing distribution for database") {
    GIVEN("A single collection of three shards, and 3 replicas") {
      shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
  
      col.setShardMap(shards);

      currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

      allCollections.emplace_back(
          std::shared_ptr<LogicalCollection>(&col, [](LogicalCollection*) {}));

      fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
          REQUIRE(sid == s1);
          return currentShards[sid];
          });

      // Moking HttpResults
      fakeit::Mock<SimpleHttpResult> leaderCountMock;
      SimpleHttpResult& lCount = leaderCountMock.get();

      fakeit::Mock<SimpleHttpResult> followerOneCountMock;
      SimpleHttpResult& f1Count = followerOneCountMock.get();

      fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
      SimpleHttpResult& f2Count = followerTwoCountMock.get();

      uint64_t leaderCount = 1337;
      uint64_t smallerFollowerCount = 456;
      uint64_t largerFollowerCount = 1111;

      // Mocking HTTP Response
      fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
            std::shared_ptr<VPackBuilder>()))
        .AlwaysDo([&]() { return buildCountBody(leaderCount); });

      fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
            std::shared_ptr<VPackBuilder>()))
        .AlwaysDo([&]() { return buildCountBody(largerFollowerCount); });

      fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
            std::shared_ptr<VPackBuilder>()))
        .AlwaysDo([&]() { return buildCountBody(smallerFollowerCount); });

      ClusterCommResult leaderRes;
      ClusterCommResult follower1Res;
      ClusterCommResult follower2Res;

      bool returnedFirstFollower = false;

      // Mocking the ClusterComm for count calls
      fakeit::When(Method(commMock, asyncRequest))
        .AlwaysDo(
            [&](CoordTransactionID const coordTransactionID,
              std::string const& destination, rest::RequestType reqtype,
              std::string const& path,
              std::shared_ptr<std::string const> body,
              std::unordered_map<std::string, std::string> const& headerFields,
              std::shared_ptr<ClusterCommCallback> callback,
              ClusterCommTimeout timeout, bool singleRequest,
              ClusterCommTimeout initTimeout) -> OperationID {
            REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

            OperationID opId = TRI_NewTickServer();

            ClusterCommResult response;
            response.coordTransactionID = coordTransactionID;
            response.operationID = opId;
            response.answer_code = rest::ResponseCode::OK;
            response.status = CL_COMM_RECEIVED;

            if (destination == "server:" + dbserver1) {
              response.result = std::shared_ptr<SimpleHttpResult>(
                  &lCount,  [](SimpleHttpResult*) {});
              leaderRes = response;
            } else if (destination == "server:" + dbserver2) {
              response.result = std::shared_ptr<SimpleHttpResult>(
                  &f1Count,  [](SimpleHttpResult*) {});
              follower1Res = response;
            } else if (destination == "server:" + dbserver3) {
              response.result = std::shared_ptr<SimpleHttpResult>(
                  &f2Count,  [](SimpleHttpResult*) {});
              follower2Res = response;
            } else {
              REQUIRE(false);
            }
            return opId;
            });

      fakeit::When(Method(commMock, wait))
        .AlwaysDo([&](CoordTransactionID const coordTransactionID,
              OperationID const operationID, ShardID const& shardID,
              ClusterCommTimeout timeout) {
            if (operationID != 0) {
            return leaderRes;
            }
            if (returnedFirstFollower) {
            return follower2Res;
            } else {
            returnedFirstFollower = true;
            return follower1Res;
            }
            });
    }
  }

  WHEN("testing collection distribution for database") {
    GIVEN("A single collection of three shards, and 3 replicas") {
      shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
  
      col.setShardMap(shards);

      currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

      allCollections.emplace_back(
          std::shared_ptr<LogicalCollection>(&col, [](LogicalCollection*) {}));

      fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
          REQUIRE(sid == s1);
          return currentShards[sid];
          });

      // Moking HttpResults
      fakeit::Mock<SimpleHttpResult> leaderCountMock;
      SimpleHttpResult& lCount = leaderCountMock.get();

      fakeit::Mock<SimpleHttpResult> followerOneCountMock;
      SimpleHttpResult& f1Count = followerOneCountMock.get();

      fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
      SimpleHttpResult& f2Count = followerTwoCountMock.get();

      uint64_t leaderCount = 1337;
      uint64_t smallerFollowerCount = 456;
      uint64_t largerFollowerCount = 1111;

      // Mocking HTTP Response
      fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
            std::shared_ptr<VPackBuilder>()))
        .AlwaysDo([&]() { return buildCountBody(leaderCount); });

      fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
            std::shared_ptr<VPackBuilder>()))
        .AlwaysDo([&]() { return buildCountBody(largerFollowerCount); });

      fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
            std::shared_ptr<VPackBuilder>()))
        .AlwaysDo([&]() { return buildCountBody(smallerFollowerCount); });

      ClusterCommResult leaderRes;
      ClusterCommResult follower1Res;
      ClusterCommResult follower2Res;

      bool returnedFirstFollower = false;

      // Mocking the ClusterComm for count calls
      fakeit::When(Method(commMock, asyncRequest))
        .AlwaysDo(
            [&](CoordTransactionID const coordTransactionID,
              std::string const& destination, rest::RequestType reqtype,
              std::string const& path,
              std::shared_ptr<std::string const> body,
              std::unordered_map<std::string, std::string> const& headerFields,
              std::shared_ptr<ClusterCommCallback> callback,
              ClusterCommTimeout timeout, bool singleRequest,
              ClusterCommTimeout initTimeout) -> OperationID {
            REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

            OperationID opId = TRI_NewTickServer();

            ClusterCommResult response;
            response.coordTransactionID = coordTransactionID;
            response.operationID = opId;
            response.answer_code = rest::ResponseCode::OK;
            response.status = CL_COMM_RECEIVED;

            if (destination == "server:" + dbserver1) {
              response.result = std::shared_ptr<SimpleHttpResult>(
                  &lCount,  [](SimpleHttpResult*) {});
              leaderRes = response;
            } else if (destination == "server:" + dbserver2) {
              response.result = std::shared_ptr<SimpleHttpResult>(
                  &f1Count,  [](SimpleHttpResult*) {});
              follower1Res = response;
            } else if (destination == "server:" + dbserver3) {
              response.result = std::shared_ptr<SimpleHttpResult>(
                  &f2Count,  [](SimpleHttpResult*) {});
              follower2Res = response;
            } else {
              REQUIRE(false);
            }
            return opId;
            });

      fakeit::When(Method(commMock, wait))
        .AlwaysDo([&](CoordTransactionID const coordTransactionID,
              OperationID const operationID, ShardID const& shardID,
              ClusterCommTimeout timeout) {
            if (operationID != 0) {
            return leaderRes;
            }
            if (returnedFirstFollower) {
            return follower2Res;
            } else {
            returnedFirstFollower = true;
            return follower1Res;
            }
            });

      WHEN("testing collection distribution for database") {
        WHEN("Both followers have a smaller amount of documents") {
          smallerFollowerCount = 456;
          largerFollowerCount = 1111;


          THEN("The minimum should be reported") {
            VPackBuilder resultBuilder;
            testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

            VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, smallerFollowerCount);
          }
        }

        WHEN("Both followers have a larger amount of documents") {
          smallerFollowerCount = 1987;
          largerFollowerCount = 2345;

          THEN("The maximum should be reported") {
            VPackBuilder resultBuilder;
            testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

            VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, largerFollowerCount);
          }
        }

        WHEN("one follower has more and one has less documents") {
          smallerFollowerCount = 456;
          largerFollowerCount = 2345;

          THEN("The lesser should be reported") {
            VPackBuilder resultBuilder;
            testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

            VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, smallerFollowerCount);
          }
        }
      }
    }
  }

  WHEN("testing distribution for database") {
    GIVEN("An unhealthy cluster") {
      shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
  
      col.setShardMap(shards);

      currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

      allCollections.emplace_back(
          std::shared_ptr<LogicalCollection>(&col, [](LogicalCollection*) {}));

      fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
          REQUIRE(currentShards.find(sid) != currentShards.end());
          return currentShards[sid];
          });

      WHEN("The leader does not respond") {

        ClusterCommResult leaderRes;

        CoordTransactionID coordId = 0;
        // Mocking the ClusterComm for count calls
        fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                std::string const& destination, rest::RequestType reqtype,
                std::string const& path,
                std::shared_ptr<std::string const> body,
                std::unordered_map<std::string, std::string> const& headerFields,
                std::shared_ptr<ClusterCommCallback> callback,
                ClusterCommTimeout timeout, bool singleRequest,
                ClusterCommTimeout initTimeout) -> OperationID {
              REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

              OperationID opId = TRI_NewTickServer();
              coordId = coordTransactionID;

              ClusterCommResult response;
              response.coordTransactionID = coordTransactionID;
              response.operationID = opId;
              response.answer_code = rest::ResponseCode::OK;
              response.status = CL_COMM_RECEIVED;

              if (destination == "server:" + dbserver1) {
                response.status = CL_COMM_TIMEOUT;
              } else if (destination == "server:" + dbserver2) {
              } else if (destination == "server:" + dbserver3) {
              } else {
                REQUIRE(false);
              }
              return opId;
              });

        fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID,
                ClusterCommTimeout timeout) {
              if (operationID != 0) {
              return leaderRes;
              }
              // If we get here we tried to wait for followers and we cannot use the answer...
              REQUIRE(false);
              return leaderRes;
              });

        fakeit::When(Method(commMock, drop))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID
                ) {
              // We need to abort this trx
              REQUIRE(coordTransactionID == coordId);
              // For all operations and shards
              REQUIRE(operationID == 0);
              REQUIRE(shardID == "");
              });

        THEN("It should use the defaults total: 1 current: 0") {
          VPackBuilder resultBuilder;
          testee.getDistributionForDatabase(dbname, resultBuilder);
          VerifyAttributes(resultBuilder.slice(), colName, s1);
        }

        THEN("It needs to call drop") {
          VPackBuilder resultBuilder;
          testee.getDistributionForDatabase(dbname, resultBuilder);
          fakeit::Verify(Method(commMock, drop)).Exactly(1);
        }

      }

      WHEN("One follower does not respond") {
        uint64_t leaderCount = 1337;
        uint64_t smallerFollowerCount = 456;
        uint64_t largerFollowerCount = 1111;

        // Moking HttpResults
        fakeit::Mock<SimpleHttpResult> leaderCountMock;
        SimpleHttpResult& lCount = leaderCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerOneCountMock;
        SimpleHttpResult& f1Count = followerOneCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
        SimpleHttpResult& f2Count = followerTwoCountMock.get();

        // Mocking HTTP Response
        fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(leaderCount); });

        fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(largerFollowerCount); });

        fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { REQUIRE(false); return buildCountBody(smallerFollowerCount); });



        ClusterCommResult leaderRes;
        ClusterCommResult follower1Res;
        ClusterCommResult follower2Res;
        bool returnedFirstFollower = false;

        CoordTransactionID coordId = 0;
        // Mocking the ClusterComm for count calls
        fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                std::string const& destination, rest::RequestType reqtype,
                std::string const& path,
                std::shared_ptr<std::string const> body,
                std::unordered_map<std::string, std::string> const& headerFields,
                std::shared_ptr<ClusterCommCallback> callback,
                ClusterCommTimeout timeout, bool singleRequest,
                ClusterCommTimeout initTimeout) -> OperationID {
              REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

              OperationID opId = TRI_NewTickServer();
              coordId = coordTransactionID;

              ClusterCommResult response;
              response.coordTransactionID = coordTransactionID;
              response.operationID = opId;
              response.answer_code = rest::ResponseCode::OK;
              response.status = CL_COMM_RECEIVED;

              if (destination == "server:" + dbserver1) {
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &lCount,  [](SimpleHttpResult*) {});
                leaderRes = response;
              } else if (destination == "server:" + dbserver2) {
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f1Count,  [](SimpleHttpResult*) {});
                follower1Res = response;
              } else if (destination == "server:" + dbserver3) {
                response.status = CL_COMM_TIMEOUT;
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f2Count,  [](SimpleHttpResult*) {});
                follower2Res = response;
              } else {
                REQUIRE(false);
              }

              return opId;
              });
        fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID,
                ClusterCommTimeout timeout) {
              if (operationID != 0) {
              return leaderRes;
              }
              if (returnedFirstFollower) {
              return follower2Res;
              } else {
              returnedFirstFollower = true;
              return follower1Res;
              }
              });

        fakeit::When(Method(commMock, drop))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID
                ) {
              // We need to abort this trx
              REQUIRE(coordTransactionID == coordId);
              // For all operations and shards
              REQUIRE(operationID == 0);
              REQUIRE(shardID == "");
              });

        THEN("It should use the leader and the other one") {
          VPackBuilder resultBuilder;
          testee.getDistributionForDatabase(dbname, resultBuilder);
          VerifyAttributes(resultBuilder.slice(), colName, s1);
        }

        THEN("It should not call drop") {
          VPackBuilder resultBuilder;
          testee.getDistributionForDatabase(dbname, resultBuilder);
          fakeit::Verify(Method(commMock, drop)).Exactly(0);
        }

      }

      WHEN("No follower does respond") {
        uint64_t leaderCount = 1337;
        uint64_t smallerFollowerCount = 456;
        uint64_t largerFollowerCount = 1111;

        // Moking HttpResults
        fakeit::Mock<SimpleHttpResult> leaderCountMock;
        SimpleHttpResult& lCount = leaderCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerOneCountMock;
        SimpleHttpResult& f1Count = followerOneCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
        SimpleHttpResult& f2Count = followerTwoCountMock.get();

        // Mocking HTTP Response
        fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(leaderCount); });

        fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { REQUIRE(false); return buildCountBody(largerFollowerCount); });

        fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { REQUIRE(false); return buildCountBody(smallerFollowerCount); });

        ClusterCommResult leaderRes;
        ClusterCommResult follower1Res;
        ClusterCommResult follower2Res;
        bool returnedFirstFollower = false;

        CoordTransactionID coordId = 0;
        // Mocking the ClusterComm for count calls
        fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                std::string const& destination, rest::RequestType reqtype,
                std::string const& path,
                std::shared_ptr<std::string const> body,
                std::unordered_map<std::string, std::string> const& headerFields,
                std::shared_ptr<ClusterCommCallback> callback,
                ClusterCommTimeout timeout, bool singleRequest,
                ClusterCommTimeout initTimeout) -> OperationID {
              REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

              OperationID opId = TRI_NewTickServer();
              coordId = coordTransactionID;

              ClusterCommResult response;
              response.coordTransactionID = coordTransactionID;
              response.operationID = opId;
              response.answer_code = rest::ResponseCode::OK;
              response.status = CL_COMM_RECEIVED;

              if (destination == "server:" + dbserver1) {
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &lCount,  [](SimpleHttpResult*) {});
                leaderRes = response;
              } else if (destination == "server:" + dbserver2) {
                response.status = CL_COMM_TIMEOUT;
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f1Count,  [](SimpleHttpResult*) {});
                follower1Res = response;
              } else if (destination == "server:" + dbserver3) {
                response.status = CL_COMM_TIMEOUT;
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f2Count,  [](SimpleHttpResult*) {});
                follower2Res = response;
              } else {
                REQUIRE(false);
              }

              return opId;
              });
        fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID,
                ClusterCommTimeout timeout) {
              if (operationID != 0) {
              return leaderRes;
              }
              if (returnedFirstFollower) {
              return follower2Res;
              } else {
              returnedFirstFollower = true;
              return follower1Res;
              }
              });

        fakeit::When(Method(commMock, drop))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID
                ) {
              // We need to abort this trx
              REQUIRE(coordTransactionID == coordId);
              // For all operations and shards
              REQUIRE(operationID == 0);
              REQUIRE(shardID == "");
              });

        THEN("It should use the leader") {
          VPackBuilder resultBuilder;
          testee.getDistributionForDatabase(dbname, resultBuilder);
          VerifyAttributes(resultBuilder.slice(), colName, s1);
        }

        THEN("It should not call drop") {
          VPackBuilder resultBuilder;
          testee.getDistributionForDatabase(dbname, resultBuilder);
          fakeit::Verify(Method(commMock, drop)).Exactly(0);
        }

      }
    }
  }

  WHEN("testing collection distribution for database") {
    GIVEN("An unhealthy cluster") {
      shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
  
      col.setShardMap(shards);

      currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

      allCollections.emplace_back(
          std::shared_ptr<LogicalCollection>(&col, [](LogicalCollection*) {}));

      fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
          REQUIRE(currentShards.find(sid) != currentShards.end());
          return currentShards[sid];
          });

      WHEN("The leader does not respond") {

        ClusterCommResult leaderRes;

        CoordTransactionID coordId = 0;
        // Mocking the ClusterComm for count calls
        fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                std::string const& destination, rest::RequestType reqtype,
                std::string const& path,
                std::shared_ptr<std::string const> body,
                std::unordered_map<std::string, std::string> const& headerFields,
                std::shared_ptr<ClusterCommCallback> callback,
                ClusterCommTimeout timeout, bool singleRequest,
                ClusterCommTimeout initTimeout) -> OperationID {
              REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

              OperationID opId = TRI_NewTickServer();
              coordId = coordTransactionID;

              ClusterCommResult response;
              response.coordTransactionID = coordTransactionID;
              response.operationID = opId;
              response.answer_code = rest::ResponseCode::OK;
              response.status = CL_COMM_RECEIVED;

              if (destination == "server:" + dbserver1) {
                response.status = CL_COMM_TIMEOUT;
              } else if (destination == "server:" + dbserver2) {
              } else if (destination == "server:" + dbserver3) {
              } else {
                REQUIRE(false);
              }
              return opId;
              });

        fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID,
                ClusterCommTimeout timeout) {
              if (operationID != 0) {
              return leaderRes;
              }
              // If we get here we tried to wait for followers and we cannot use the answer...
              REQUIRE(false);
              return leaderRes;
              });

        fakeit::When(Method(commMock, drop))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID
                ) {
              // We need to abort this trx
              REQUIRE(coordTransactionID == coordId);
              // For all operations and shards
              REQUIRE(operationID == 0);
              REQUIRE(shardID == "");
              });

        THEN("It should use the defaults total: 1 current: 0") {
          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
          VerifyNumbers(resultBuilder.slice(), colName, s1, 1, 0);
        }

        THEN("It needs to call drop") {
          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
          fakeit::Verify(Method(commMock, drop)).Exactly(1);
        }

      }

      WHEN("One follower does not respond") {
        uint64_t leaderCount = 1337;
        uint64_t smallerFollowerCount = 456;
        uint64_t largerFollowerCount = 1111;

        // Moking HttpResults
        fakeit::Mock<SimpleHttpResult> leaderCountMock;
        SimpleHttpResult& lCount = leaderCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerOneCountMock;
        SimpleHttpResult& f1Count = followerOneCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
        SimpleHttpResult& f2Count = followerTwoCountMock.get();

        // Mocking HTTP Response
        fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(leaderCount); });

        fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(largerFollowerCount); });

        fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { REQUIRE(false); return buildCountBody(smallerFollowerCount); });



        ClusterCommResult leaderRes;
        ClusterCommResult follower1Res;
        ClusterCommResult follower2Res;
        bool returnedFirstFollower = false;

        CoordTransactionID coordId = 0;
        // Mocking the ClusterComm for count calls
        fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                std::string const& destination, rest::RequestType reqtype,
                std::string const& path,
                std::shared_ptr<std::string const> body,
                std::unordered_map<std::string, std::string> const& headerFields,
                std::shared_ptr<ClusterCommCallback> callback,
                ClusterCommTimeout timeout, bool singleRequest,
                ClusterCommTimeout initTimeout) -> OperationID {
              REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

              OperationID opId = TRI_NewTickServer();
              coordId = coordTransactionID;

              ClusterCommResult response;
              response.coordTransactionID = coordTransactionID;
              response.operationID = opId;
              response.answer_code = rest::ResponseCode::OK;
              response.status = CL_COMM_RECEIVED;

              if (destination == "server:" + dbserver1) {
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &lCount,  [](SimpleHttpResult*) {});
                leaderRes = response;
              } else if (destination == "server:" + dbserver2) {
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f1Count,  [](SimpleHttpResult*) {});
                follower1Res = response;
              } else if (destination == "server:" + dbserver3) {
                response.status = CL_COMM_TIMEOUT;
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f2Count,  [](SimpleHttpResult*) {});
                follower2Res = response;
              } else {
                REQUIRE(false);
              }

              return opId;
              });
        fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID,
                ClusterCommTimeout timeout) {
              if (operationID != 0) {
              return leaderRes;
              }
              if (returnedFirstFollower) {
              return follower2Res;
              } else {
              returnedFirstFollower = true;
              return follower1Res;
              }
              });

        fakeit::When(Method(commMock, drop))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID
                ) {
              // We need to abort this trx
              REQUIRE(coordTransactionID == coordId);
              // For all operations and shards
              REQUIRE(operationID == 0);
              REQUIRE(shardID == "");
              });

        THEN("It should use the leader and the other one") {
          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
          VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, largerFollowerCount);
        }

        THEN("It should not call drop") {
          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
          fakeit::Verify(Method(commMock, drop)).Exactly(0);
        }

      }

      WHEN("No follower does respond") {
        uint64_t leaderCount = 1337;
        uint64_t smallerFollowerCount = 456;
        uint64_t largerFollowerCount = 1111;

        // Moking HttpResults
        fakeit::Mock<SimpleHttpResult> leaderCountMock;
        SimpleHttpResult& lCount = leaderCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerOneCountMock;
        SimpleHttpResult& f1Count = followerOneCountMock.get();

        fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
        SimpleHttpResult& f2Count = followerTwoCountMock.get();

        // Mocking HTTP Response
        fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { return buildCountBody(leaderCount); });

        fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { REQUIRE(false); return buildCountBody(largerFollowerCount); });

        fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
              std::shared_ptr<VPackBuilder>()))
          .AlwaysDo([&]() { REQUIRE(false); return buildCountBody(smallerFollowerCount); });

        ClusterCommResult leaderRes;
        ClusterCommResult follower1Res;
        ClusterCommResult follower2Res;
        bool returnedFirstFollower = false;

        CoordTransactionID coordId = 0;
        // Mocking the ClusterComm for count calls
        fakeit::When(Method(commMock, asyncRequest))
          .AlwaysDo(
              [&](CoordTransactionID const coordTransactionID,
                std::string const& destination, rest::RequestType reqtype,
                std::string const& path,
                std::shared_ptr<std::string const> body,
                std::unordered_map<std::string, std::string> const& headerFields,
                std::shared_ptr<ClusterCommCallback> callback,
                ClusterCommTimeout timeout, bool singleRequest,
                ClusterCommTimeout initTimeout) -> OperationID {
              REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

              OperationID opId = TRI_NewTickServer();
              coordId = coordTransactionID;

              ClusterCommResult response;
              response.coordTransactionID = coordTransactionID;
              response.operationID = opId;
              response.answer_code = rest::ResponseCode::OK;
              response.status = CL_COMM_RECEIVED;

              if (destination == "server:" + dbserver1) {
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &lCount,  [](SimpleHttpResult*) {});
                leaderRes = response;
              } else if (destination == "server:" + dbserver2) {
                response.status = CL_COMM_TIMEOUT;
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f1Count,  [](SimpleHttpResult*) {});
                follower1Res = response;
              } else if (destination == "server:" + dbserver3) {
                response.status = CL_COMM_TIMEOUT;
                response.result = std::shared_ptr<SimpleHttpResult>(
                    &f2Count,  [](SimpleHttpResult*) {});
                follower2Res = response;
              } else {
                REQUIRE(false);
              }

              return opId;
              });
        fakeit::When(Method(commMock, wait))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID,
                ClusterCommTimeout timeout) {
              if (operationID != 0) {
              return leaderRes;
              }
              if (returnedFirstFollower) {
              return follower2Res;
              } else {
              returnedFirstFollower = true;
              return follower1Res;
              }
              });

        fakeit::When(Method(commMock, drop))
          .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                OperationID const operationID, ShardID const& shardID
                ) {
              // We need to abort this trx
              REQUIRE(coordTransactionID == coordId);
              // For all operations and shards
              REQUIRE(operationID == 0);
              REQUIRE(shardID == "");
              });

        THEN("It should use the leader and the default current of 0") {
          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
          VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, 0);
        }

        THEN("It should not call drop") {
          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
          fakeit::Verify(Method(commMock, drop)).Exactly(0);
        }

      }

      // this should be obsolete because from now on only one collection can be asked
      // for its progressing state

      /*GIVEN("A second collection") {
        fakeit::Mock<LogicalCollection> secColMock;
        LogicalCollection& secCol = secColMock.get();

        std::string secColName = "UnitTestOtherCollection";
        std::string secCidString = "4561";


        fakeit::Mock<CollectionInfoCurrent> infoOtherCurrentMock;
        CollectionInfoCurrent& otherCicInst = infoOtherCurrentMock.get();

        std::shared_ptr<CollectionInfoCurrent> otherCic(
            &otherCicInst, [](CollectionInfoCurrent*) {});

        fakeit::When(Method(infoMock, getCollectionCurrent).Using(dbname, secCidString))
          .AlwaysDo([&](DatabaseID const& dbId, CollectionID const& cId) {
              REQUIRE(dbId == dbname);
              REQUIRE(cId == secCidString);
              return otherCic;
              });

        allCollections.emplace_back(
            std::shared_ptr<LogicalCollection>(&secCol, [](LogicalCollection*) {}));

        // Fake the shard map
        auto otherShards = std::make_shared<ShardMap>();
        ShardMap otherCurrentShards;

        otherShards->emplace(s2, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

        otherCurrentShards.emplace(s2, std::vector<ServerID>{dbserver1});

        fakeit::When(Method(secColMock, name)).AlwaysReturn(secColName);
        fakeit::When(
            ConstOverloadedMethod(secColMock, shardIds, std::shared_ptr<ShardMap>()))
          .AlwaysReturn(otherShards);
        fakeit::When(Method(secColMock, cid_as_string)).AlwaysReturn(secCidString);

        fakeit::When(Method(infoOtherCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
            REQUIRE(otherCurrentShards.find(sid) != otherCurrentShards.end());
            return otherCurrentShards[sid];
            });

        THEN("It should not ask the second collection if the first waits > 2.0s") {
          // Moking HttpResults
          fakeit::Mock<SimpleHttpResult> leaderCountMock;
          SimpleHttpResult& lCount = leaderCountMock.get();

          fakeit::Mock<SimpleHttpResult> followerOneCountMock;
          SimpleHttpResult& f1Count = followerOneCountMock.get();

          fakeit::Mock<SimpleHttpResult> followerTwoCountMock;
          SimpleHttpResult& f2Count = followerTwoCountMock.get();

          uint64_t leaderCount = 1337;
          uint64_t smallerFollowerCount = 456;
          uint64_t largerFollowerCount = 1111;

          // Mocking HTTP Response
          fakeit::When(ConstOverloadedMethod(leaderCountMock, getBodyVelocyPack,
                std::shared_ptr<VPackBuilder>()))
            .AlwaysDo([&]() { return buildCountBody(leaderCount); });

          fakeit::When(ConstOverloadedMethod(followerOneCountMock, getBodyVelocyPack,
                std::shared_ptr<VPackBuilder>()))
            .AlwaysDo([&]() { return buildCountBody(largerFollowerCount); });

          fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
                std::shared_ptr<VPackBuilder>()))
            .AlwaysDo([&]() { return buildCountBody(smallerFollowerCount); });

          ClusterCommResult leaderRes;
          ClusterCommResult follower1Res;
          ClusterCommResult follower2Res;

          bool returnedFirstFollower = false;

          // Mocking the ClusterComm for count calls
          fakeit::When(Method(commMock, asyncRequest))
            .AlwaysDo(
                [&](CoordTransactionID const coordTransactionID,
                  std::string const& destination, rest::RequestType reqtype,
                  std::string const& path,
                  std::shared_ptr<std::string const> body,
                  std::unordered_map<std::string, std::string> const& headerFields,
                  std::shared_ptr<ClusterCommCallback> callback,
                  ClusterCommTimeout timeout, bool singleRequest,
                  ClusterCommTimeout initTimeout) -> OperationID {
                REQUIRE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

                OperationID opId = TRI_NewTickServer();

                ClusterCommResult response;
                response.coordTransactionID = coordTransactionID;
                response.operationID = opId;
                response.answer_code = rest::ResponseCode::OK;
                response.status = CL_COMM_RECEIVED;

                if (destination == "server:" + dbserver1) {
                  response.result = std::shared_ptr<SimpleHttpResult>(
                      &lCount,  [](SimpleHttpResult*) {});
                  leaderRes = response;
                } else if (destination == "server:" + dbserver2) {
                  response.result = std::shared_ptr<SimpleHttpResult>(
                      &f1Count,  [](SimpleHttpResult*) {});
                  follower1Res = response;
                } else if (destination == "server:" + dbserver3) {
                  response.result = std::shared_ptr<SimpleHttpResult>(
                      &f2Count,  [](SimpleHttpResult*) {});
                  follower2Res = response;
                } else {
                  REQUIRE(false);
                }
                return opId;
                });

          fakeit::When(Method(commMock, wait))
            .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                  OperationID const operationID, ShardID const& shardID,
                  ClusterCommTimeout timeout) {
                if (operationID != 0) {
                // Let us sleep 2 seconds here
                std::this_thread::sleep_for(std::chrono::seconds(2));
                return leaderRes;
                }
                if (returnedFirstFollower) {
                return follower2Res;
                } else {
                returnedFirstFollower = true;
                return follower1Res;
                }
                });


          VPackBuilder resultBuilder;
          testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

          VPackSlice result = resultBuilder.slice();
          // Validate first collection is okay (we did not really timeout, just tricked the system...)
          VerifyNumbers(result, colName, s1, leaderCount, smallerFollowerCount);
          // Validate that the second collection uses default values.
          VerifyNumbers(result, secColName, s2, 1, 0);

        }
      }*/
    }
  }
    
  for (auto& f: features) {
    f.first->unprepare();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
