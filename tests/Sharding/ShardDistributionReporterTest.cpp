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

#include "gtest/gtest.h"

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
#include "tests/Mocks/StorageEngineMock.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <queue>
#include <thread>

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::httpclient;

namespace {
void noop(ClusterComm*) {}
}  // namespace

static void VerifyAttributes(VPackSlice result, std::string const& colName,
                             std::string const& sName) {
  ASSERT_TRUE(result.isObject());

  VPackSlice col = result.get(colName);
  ASSERT_TRUE(col.isObject());

  VPackSlice plan = col.get("Plan");
  ASSERT_TRUE(plan.isObject());

  VPackSlice shard = plan.get(sName);
  ASSERT_TRUE(shard.isObject());
}

static void VerifyNumbers(VPackSlice result, std::string const& colName,
                          std::string const& sName, uint64_t testTotal, uint64_t testCurrent) {
  ASSERT_TRUE(result.isObject());

  VPackSlice col = result.get(colName);
  ASSERT_TRUE(col.isObject());

  VPackSlice plan = col.get("Plan");
  ASSERT_TRUE(plan.isObject());

  VPackSlice shard = plan.get(sName);
  ASSERT_TRUE(shard.isObject());

  VPackSlice progress = shard.get("progress");
  ASSERT_TRUE(progress.isObject());

  VPackSlice total = progress.get("total");
  ASSERT_TRUE(total.isNumber());
  ASSERT_TRUE(total.getNumber<uint64_t>() == testTotal);

  VPackSlice current = progress.get("current");
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(current.getNumber<uint64_t>(), testCurrent);
}

static std::shared_ptr<VPackBuilder> buildCountBody(uint64_t count) {
  auto res = std::make_shared<VPackBuilder>();
  res->openObject();
  res->add("count", VPackValue(count));
  res->close();
  return res;
}

class ShardDistributionReporterTest : public ::testing::Test {
 protected:
  arangodb::application_features::ApplicationServer server;
  StorageEngineMock engine;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  fakeit::Mock<ClusterComm> commMock;
  ClusterComm& cc;

  fakeit::Mock<ClusterInfo> infoMock;
  ClusterInfo& ci;

  fakeit::Mock<CollectionInfoCurrent> infoCurrentMock;
  CollectionInfoCurrent& cicInst;

  std::shared_ptr<CollectionInfoCurrent> cic;

  std::string dbname;
  std::string colName;
  std::string cidString;

  std::string s1;
  std::string s2;
  std::string s3;

  std::string dbserver1;
  std::string dbserver2;
  std::string dbserver3;

  std::string dbserver1short;
  std::string dbserver2short;
  std::string dbserver3short;

  std::unique_ptr<TRI_vocbase_t> vocbase;
  std::shared_ptr<VPackBuilder> json;
  std::unique_ptr<arangodb::LogicalCollection> col;

  // Fake the aliases
  std::unordered_map<ServerID, std::string> aliases;

  // Fake the shard map
  std::shared_ptr<ShardMap> shards;
  ShardMap currentShards;

  // Fake the collections
  std::vector<std::shared_ptr<LogicalCollection>> allCollections;

  ShardDistributionReporter testee;

  ShardDistributionReporterTest()
      : server(nullptr, nullptr),
        engine(server),
        cc(commMock.get()),
        ci(infoMock.get()),
        cicInst(infoCurrentMock.get()),
        cic(&cicInst, [](CollectionInfoCurrent*) {}),
        dbname("UnitTestDB"),
        colName("UnitTestCollection"),
        cidString("1337"),
        s1("s1234"),
        s2("s2345"),
        s3("s3456"),
        dbserver1("PRMR_123_456"),
        dbserver2("PRMR_456_123"),
        dbserver3("PRMR_987_654"),
        dbserver1short("DBServer1"),
        dbserver2short("DBServer2"),
        dbserver3short("DBServer3"),
        json(arangodb::velocypack::Parser::fromJson(
            "{ \"cid\" : \"1337\", \"name\": \"UnitTestCollection\" }")),
        aliases{{dbserver1, dbserver1short}, {dbserver2, dbserver2short}, {dbserver3, dbserver3short}},
        shards(std::make_shared<ShardMap>()),
        testee(std::shared_ptr<ClusterComm>(&cc, ::noop), &ci) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    features.emplace_back(new arangodb::DatabaseFeature(server),
                          false);  // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for TRI_vocbase_t instantiation

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    vocbase = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                              1, "testVocbase");
    col = std::make_unique<arangodb::LogicalCollection>(*vocbase, json->slice(), true);

    col->setShardMap(shards);

    // Now we fake the calls
    fakeit::When(Method(infoMock, getCollections)).AlwaysDo([&](DatabaseID const& dbId) {
      EXPECT_TRUE(dbId == dbname);
      return allCollections;
    });
    // Now we fake the single collection call
    fakeit::When(Method(infoMock, getCollection)).AlwaysDo([&](DatabaseID const& dbId, CollectionID const& colId) {
      EXPECT_TRUE(dbId == dbname);
      // EXPECT_TRUE(colId == colName);
      EXPECT_TRUE(allCollections.size() > 0);

      for (auto const& c : allCollections) {
        if (c->name() == colId) {
          return c;
        }
      }

      EXPECT_TRUE(false);
      return std::shared_ptr<LogicalCollection>(nullptr);
    });
    fakeit::When(Method(infoMock, getServerAliases)).AlwaysReturn(aliases);
    fakeit::When(Method(infoMock, getCollectionCurrent).Using(dbname, cidString))
        .AlwaysDo([&](DatabaseID const& dbId, CollectionID const& cId) {
          EXPECT_TRUE(dbId == dbname);
          EXPECT_TRUE(cId == cidString);
          return cic;
        });
  }

  ~ShardDistributionReporterTest() {
    col.reset();
    vocbase.reset();

    for (auto& f : features) {
      f.first->unprepare();
    }
  }
};

TEST_F(ShardDistributionReporterTest,
       a_healthy_instance_a_single_collection_of_three_shards_and_three_replicas) {
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

  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
  shards->emplace(s2, std::vector<ServerID>{dbserver2, dbserver1, dbserver3});
  shards->emplace(s3, std::vector<ServerID>{dbserver3, dbserver1, dbserver2});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});
  currentShards.emplace(s2, std::vector<ServerID>{dbserver2});
  currentShards.emplace(s3, std::vector<ServerID>{dbserver3, dbserver2});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(((sid == s1) || (sid == s2) || (sid == s3)));
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
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(initTimeout == -1.0);  // Default
        EXPECT_TRUE(!singleRequest);       // we want to use keep-alive
        EXPECT_TRUE(callback == nullptr);  // We actively wait
        EXPECT_TRUE(reqtype == rest::RequestType::GET);  // count is only get!
        EXPECT_TRUE(headerFields.empty());               // Nono headers

        // This feature has at most 2s to do its job
        // otherwise default values will be returned
        EXPECT_TRUE(timeout <= 2.0);

        if (!gotFirstRequest) {
          gotFirstRequest = true;
          cordTrxId = coordTransactionID;
        } else {
          // We always use the same id
          EXPECT_EQ(cordTrxId, coordTransactionID);
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
            response.result =
                std::shared_ptr<SimpleHttpResult>(&httpdb1s2Count,
                                                  [](SimpleHttpResult*) {});
          } else {
            EXPECT_TRUE(path ==
                        "/_db/UnitTestDB/_api/collection/" + s3 + "/count");
            response.result =
                std::shared_ptr<SimpleHttpResult>(&httpdb1s3Count,
                                                  [](SimpleHttpResult*) {});
          }
        } else if (destination == "server:" + dbserver2) {
          // Leads s2
          EXPECT_TRUE(path ==
                      "/_db/UnitTestDB/_api/collection/" + s2 + "/count");
          response.result = std::shared_ptr<SimpleHttpResult>(&httpdb2s2Count,
                                                              [](SimpleHttpResult*) {});
          leaderS2Response = response;
          leaderS2Id = opId;
          leaderS2Delivered = false;
          return opId;
        } else if (destination == "server:" + dbserver3) {
          // Leads s3
          // off-sync follows s2
          if (path == "/_db/UnitTestDB/_api/collection/" + s2 + "/count") {
            response.result =
                std::shared_ptr<SimpleHttpResult>(&httpdb3s2Count,
                                                  [](SimpleHttpResult*) {});
          } else {
            EXPECT_TRUE(path ==
                        "/_db/UnitTestDB/_api/collection/" + s3 + "/count");
            response.result =
                std::shared_ptr<SimpleHttpResult>(&httpdb3s3Count,
                                                  [](SimpleHttpResult*) {});
            leaderS3Response = response;
            leaderS3Id = opId;
            leaderS3Delivered = false;
            return opId;
          }
        } else {
          // Unknown Server!!
          EXPECT_TRUE(false);
        }

        responses.push(response);
        return opId;
      });

  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
        EXPECT_EQ(coordTransactionID, cordTrxId);
        EXPECT_TRUE(shardID == "");  // Superfluous
        EXPECT_TRUE(timeout == 0.0);  // Default, the requests has timeout already

        if (operationID == leaderS2Id && !leaderS2Delivered) {
          EXPECT_TRUE(leaderS2Id != 0);
          EXPECT_TRUE(!leaderS2Delivered);
          leaderS2Delivered = true;
          return leaderS2Response;
        }

        if (operationID == leaderS3Id && !leaderS3Delivered) {
          EXPECT_TRUE(leaderS3Id != 0);
          EXPECT_TRUE(!leaderS3Delivered);
          leaderS3Delivered = true;
          return leaderS3Response;
        }

        EXPECT_TRUE(operationID == 0);  // We do not wait for a specific one

        EXPECT_TRUE(!responses.empty());
        ClusterCommResult last = responses.front();
        responses.pop();
        return last;
      });

  {  // testing distribution for database
    gotFirstRequest = false;
    VPackBuilder resultBuilder;
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VPackSlice result = resultBuilder.slice();

    ASSERT_TRUE(result.isObject());

    {
      VPackSlice col = result.get(colName);
      ASSERT_TRUE(col.isObject());
    }

    {  // Checking one of those collections
      result = result.get(colName);
      ASSERT_TRUE(result.isObject());

      {  // validating the plan
        VPackSlice plan = result.get("Plan");
        ASSERT_TRUE(plan.isObject());

        // One entry per shard
        ASSERT_TRUE(plan.length() == shards->size());

        {  // Testing the in-sync shard
          VPackSlice shard = plan.get(s1);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver1short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver2short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver3short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver3short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver2short);
            }
          }

          {  // It should not display progress
            VPackSlice progress = shard.get("progress");
            ASSERT_TRUE(progress.isNone());
          }
        }

        {  // Testing the off-sync shard
          VPackSlice shard = plan.get(s2);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver2short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver1short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver3short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver3short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver1short);
            }
          }

          {  // It should not display the progress
            VPackSlice progress = shard.get("progress");
            ASSERT_TRUE(progress.isNone());
          }
        }

        {  // Testing the partial in-sync shard
          VPackSlice shard = plan.get(s3);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver3short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver1short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver2short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver2short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver1short);
            }
          }

          {  // It should not display the progress
            VPackSlice progress = shard.get("progress");
            ASSERT_TRUE(progress.isNone());
          }
        }
      }

      {  // validating current
        VPackSlice current = result.get("Current");
        ASSERT_TRUE(current.isObject());

        // One entry per shard
        ASSERT_TRUE(current.length() == shards->size());

        {  // Testing the in-sync shard
          VPackSlice shard = current.get(s1);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver1short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver2short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver3short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver3short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver2short);
            }
          }
        }

        {  // Testing the off-sync shard
          VPackSlice shard = current.get(s2);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver2short);
          }

          {  // It should not have any followers
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 0);
          }
        }

        {  // Testing the partial in-sync shard
          VPackSlice shard = current.get(s3);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver3short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 1);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());
            ASSERT_TRUE(firstFollower.copyString() == dbserver2short);
          }
        }
      }
    }
  }

  {  // testing distribution for collection database
    gotFirstRequest = false;
    VPackBuilder resultBuilder;
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VPackSlice result = resultBuilder.slice();

    ASSERT_TRUE(result.isObject());

    {  // It should return one entry for every collection
      VPackSlice col = result.get(colName);
      ASSERT_TRUE(col.isObject());
    }

    {  // Checking one of those collections
      result = result.get(colName);
      ASSERT_TRUE(result.isObject());

      {  // validating the plan
        VPackSlice plan = result.get("Plan");
        ASSERT_TRUE(plan.isObject());

        // One entry per shard
        ASSERT_TRUE(plan.length() == shards->size());

        {  // Testing the in-sync shard
          VPackSlice shard = plan.get(s1);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver1short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver2short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver3short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver3short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver2short);
            }
          }

          {  // It should not display progress
            VPackSlice progress = shard.get("progress");
            ASSERT_TRUE(progress.isNone());
          }
        }

        {  // Testing the off-sync shard
          VPackSlice shard = plan.get(s2);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver2short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver1short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver3short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver3short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver1short);
            }
          }

          {  // It should not display the progress
            VPackSlice progress = shard.get("progress");
            ASSERT_TRUE(progress.isObject());

            VPackSlice total = progress.get("total");
            ASSERT_TRUE(total.isNumber());
            ASSERT_TRUE(total.getNumber<uint64_t>() == shard2LeaderCount);

            VPackSlice current = progress.get("current");
            ASSERT_TRUE(current.isNumber());
            ASSERT_TRUE(current.getNumber<uint64_t>() == shard2LowFollowerCount);
          }
        }

        {  // Testing the partial in-sync shard
          VPackSlice shard = plan.get(s3);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver3short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver1short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver2short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver2short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver1short);
            }
          }

          {  // It should display the progress
            VPackSlice progress = shard.get("progress");
            ASSERT_TRUE(progress.isObject());

            VPackSlice total = progress.get("total");
            ASSERT_TRUE(total.isNumber());
            ASSERT_TRUE(total.getNumber<uint64_t>() == shard3LeaderCount);

            VPackSlice current = progress.get("current");
            ASSERT_TRUE(current.isNumber());
            ASSERT_TRUE(current.getNumber<uint64_t>() == shard3FollowerCount);
          }
        }
      }

      {  // validating current
        VPackSlice current = result.get("Current");
        ASSERT_TRUE(current.isObject());

        // One entry per shard
        ASSERT_TRUE(current.length() == shards->size());

        {  // Testing the in-sync shard
          VPackSlice shard = current.get(s1);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver1short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 2);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());

            VPackSlice secondFollower = followers.at(1);
            ASSERT_TRUE(secondFollower.isString());

            // We do not guarentee any ordering here
            if (arangodb::velocypack::StringRef(firstFollower) == dbserver2short) {
              ASSERT_TRUE(secondFollower.copyString() == dbserver3short);
            } else {
              ASSERT_TRUE(firstFollower.copyString() == dbserver3short);
              ASSERT_TRUE(secondFollower.copyString() == dbserver2short);
            }
          }
        }

        {  // Testing the off-sync shard
          VPackSlice shard = current.get(s2);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver2short);
          }

          {  // It should not have any followers
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 0);
          }
        }

        {  // Testing the partial in-sync shard
          VPackSlice shard = current.get(s3);

          ASSERT_TRUE(shard.isObject());

          {  // It should have the correct leader shortname
            VPackSlice leader = shard.get("leader");

            ASSERT_TRUE(leader.isString());
            ASSERT_TRUE(leader.copyString() == dbserver3short);
          }

          {  // It should have the correct followers shortnames
            VPackSlice followers = shard.get("followers");
            ASSERT_TRUE(followers.isArray());
            ASSERT_TRUE(followers.length() == 1);

            VPackSlice firstFollower = followers.at(0);
            ASSERT_TRUE(firstFollower.isString());
            ASSERT_TRUE(firstFollower.copyString() == dbserver2short);
          }
        }
      }
    }
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_distribution_for_database_a_single_collection_of_three_shards_and_three_replicas) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(sid == s1);
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
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

        OperationID opId = TRI_NewTickServer();

        ClusterCommResult response;
        response.coordTransactionID = coordTransactionID;
        response.operationID = opId;
        response.answer_code = rest::ResponseCode::OK;
        response.status = CL_COMM_RECEIVED;

        if (destination == "server:" + dbserver1) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&lCount, [](SimpleHttpResult*) {});
          leaderRes = response;
        } else if (destination == "server:" + dbserver2) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f1Count, [](SimpleHttpResult*) {});
          follower1Res = response;
        } else if (destination == "server:" + dbserver3) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f2Count, [](SimpleHttpResult*) {});
          follower2Res = response;
        } else {
          EXPECT_TRUE(false);
        }
        return opId;
      });

  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
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

TEST_F(ShardDistributionReporterTest,
       testing_collection_distribution_for_database_a_single_collection_of_three_shards_and_three_replicas) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(sid == s1);
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
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

        OperationID opId = TRI_NewTickServer();

        ClusterCommResult response;
        response.coordTransactionID = coordTransactionID;
        response.operationID = opId;
        response.answer_code = rest::ResponseCode::OK;
        response.status = CL_COMM_RECEIVED;

        if (destination == "server:" + dbserver1) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&lCount, [](SimpleHttpResult*) {});
          leaderRes = response;
        } else if (destination == "server:" + dbserver2) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f1Count, [](SimpleHttpResult*) {});
          follower1Res = response;
        } else if (destination == "server:" + dbserver3) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f2Count, [](SimpleHttpResult*) {});
          follower2Res = response;
        } else {
          EXPECT_TRUE(false);
        }
        return opId;
      });

  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
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

  {    // testing collection distribution for database
    {  // Both followers have a smaller amount of documents
      smallerFollowerCount = 456;
      largerFollowerCount = 1111;

      {  // The minimum should be reported
        VPackBuilder resultBuilder;
        testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

        VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, smallerFollowerCount);
      }
    }

    {  // Both followers have a larger amount of documents
      returnedFirstFollower = false;
      smallerFollowerCount = 1987;
      largerFollowerCount = 2345;

      {  // The maximum should be reported
        VPackBuilder resultBuilder;
        testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

        VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, largerFollowerCount);
      }
    }

    {  // one follower has more and one has less documents
      smallerFollowerCount = 456;
      largerFollowerCount = 2345;

      {  // The lesser should be reported
        VPackBuilder resultBuilder;
        testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

        VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, smallerFollowerCount);
      }
    }
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_distribution_for_database_an_unhealthy_cluster_the_leader_doesnt_respond) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(currentShards.find(sid) != currentShards.end());
    return currentShards[sid];
  });

  ClusterCommResult leaderRes;

  CoordTransactionID coordId = 0;
  // Mocking the ClusterComm for count calls
  fakeit::When(Method(commMock, asyncRequest))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

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
          EXPECT_TRUE(false);
        }
        return opId;
      });

  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
        if (operationID != 0) {
          return leaderRes;
        }
        // If we get here we tried to wait for followers and we cannot use the answer...
        EXPECT_TRUE(false);
        return leaderRes;
      });

  fakeit::When(Method(commMock, drop))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    OperationID const operationID, ShardID const& shardID) {
        // We need to abort this trx
        EXPECT_TRUE(coordTransactionID == coordId);
        // For all operations and shards
        EXPECT_TRUE(operationID == 0);
        EXPECT_TRUE(shardID == "");
      });

  {  // It should use the defaults total: 1 current: 0
    VPackBuilder resultBuilder;
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VerifyAttributes(resultBuilder.slice(), colName, s1);
  }

  {  // It needs to call drop
    fakeit::Verify(Method(commMock, drop)).Exactly(1);
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_distribution_for_database_an_unhealthy_cluster_one_follower_does_not_respond) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(currentShards.find(sid) != currentShards.end());
    return currentShards[sid];
  });

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
      .AlwaysDo([&]() {
        EXPECT_TRUE(false);
        return buildCountBody(smallerFollowerCount);
      });

  ClusterCommResult leaderRes;
  ClusterCommResult follower1Res;
  ClusterCommResult follower2Res;
  bool returnedFirstFollower = false;

  CoordTransactionID coordId = 0;
  // Mocking the ClusterComm for count calls
  fakeit::When(Method(commMock, asyncRequest))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

        OperationID opId = TRI_NewTickServer();
        coordId = coordTransactionID;

        ClusterCommResult response;
        response.coordTransactionID = coordTransactionID;
        response.operationID = opId;
        response.answer_code = rest::ResponseCode::OK;
        response.status = CL_COMM_RECEIVED;

        if (destination == "server:" + dbserver1) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&lCount, [](SimpleHttpResult*) {});
          leaderRes = response;
        } else if (destination == "server:" + dbserver2) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f1Count, [](SimpleHttpResult*) {});
          follower1Res = response;
        } else if (destination == "server:" + dbserver3) {
          response.status = CL_COMM_TIMEOUT;
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f2Count, [](SimpleHttpResult*) {});
          follower2Res = response;
        } else {
          EXPECT_TRUE(false);
        }

        return opId;
      });
  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
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
                    OperationID const operationID, ShardID const& shardID) {
        // We need to abort this trx
        EXPECT_TRUE(coordTransactionID == coordId);
        // For all operations and shards
        EXPECT_TRUE(operationID == 0);
        EXPECT_TRUE(shardID == "");
      });

  {  // It should use the leader and the other one
    VPackBuilder resultBuilder;
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VerifyAttributes(resultBuilder.slice(), colName, s1);
  }

  {  // It should not call drop
    fakeit::Verify(Method(commMock, drop)).Exactly(0);
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_distribution_for_database_an_unhealthy_cluster_no_follower_responds) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(currentShards.find(sid) != currentShards.end());
    return currentShards[sid];
  });

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
      .AlwaysDo([&]() {
        EXPECT_TRUE(false);
        return buildCountBody(largerFollowerCount);
      });

  fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
                                     std::shared_ptr<VPackBuilder>()))
      .AlwaysDo([&]() {
        EXPECT_TRUE(false);
        return buildCountBody(smallerFollowerCount);
      });

  ClusterCommResult leaderRes;
  ClusterCommResult follower1Res;
  ClusterCommResult follower2Res;
  bool returnedFirstFollower = false;

  CoordTransactionID coordId = 0;
  // Mocking the ClusterComm for count calls
  fakeit::When(Method(commMock, asyncRequest))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

        OperationID opId = TRI_NewTickServer();
        coordId = coordTransactionID;

        ClusterCommResult response;
        response.coordTransactionID = coordTransactionID;
        response.operationID = opId;
        response.answer_code = rest::ResponseCode::OK;
        response.status = CL_COMM_RECEIVED;

        if (destination == "server:" + dbserver1) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&lCount, [](SimpleHttpResult*) {});
          leaderRes = response;
        } else if (destination == "server:" + dbserver2) {
          response.status = CL_COMM_TIMEOUT;
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f1Count, [](SimpleHttpResult*) {});
          follower1Res = response;
        } else if (destination == "server:" + dbserver3) {
          response.status = CL_COMM_TIMEOUT;
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f2Count, [](SimpleHttpResult*) {});
          follower2Res = response;
        } else {
          EXPECT_TRUE(false);
        }

        return opId;
      });
  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
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
                    OperationID const operationID, ShardID const& shardID) {
        // We need to abort this trx
        EXPECT_TRUE(coordTransactionID == coordId);
        // For all operations and shards
        EXPECT_TRUE(operationID == 0);
        EXPECT_TRUE(shardID == "");
      });

  {  // It should use the leader
    VPackBuilder resultBuilder;
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VerifyAttributes(resultBuilder.slice(), colName, s1);
  }

  {  // It should not call drop
    fakeit::Verify(Method(commMock, drop)).Exactly(0);
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_collection_distribution_for_database_an_unhealthy_cluster_the_leader_doesnt_respond) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(currentShards.find(sid) != currentShards.end());
    return currentShards[sid];
  });

  ClusterCommResult leaderRes;

  CoordTransactionID coordId = 0;
  // Mocking the ClusterComm for count calls
  fakeit::When(Method(commMock, asyncRequest))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

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
          EXPECT_TRUE(false);
        }
        return opId;
      });

  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
        if (operationID != 0) {
          return leaderRes;
        }
        // If we get here we tried to wait for followers and we cannot use the answer...
        EXPECT_TRUE(false);
        return leaderRes;
      });

  fakeit::When(Method(commMock, drop))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    OperationID const operationID, ShardID const& shardID) {
        // We need to abort this trx
        EXPECT_TRUE(coordTransactionID == coordId);
        // For all operations and shards
        EXPECT_TRUE(operationID == 0);
        EXPECT_TRUE(shardID == "");
      });

  {  // It should use the defaults total: 1 current: 0
    VPackBuilder resultBuilder;
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VerifyNumbers(resultBuilder.slice(), colName, s1, 1, 0);
  }

  {  // It needs to call drop
    fakeit::Verify(Method(commMock, drop)).Exactly(1);
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_collection_distribution_for_database_an_unhealthy_cluster_one_follower_doesnt_respond) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(currentShards.find(sid) != currentShards.end());
    return currentShards[sid];
  });

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
      .AlwaysDo([&]() {
        EXPECT_TRUE(false);
        return buildCountBody(smallerFollowerCount);
      });

  ClusterCommResult leaderRes;
  ClusterCommResult follower1Res;
  ClusterCommResult follower2Res;
  bool returnedFirstFollower = false;

  CoordTransactionID coordId = 0;
  // Mocking the ClusterComm for count calls
  fakeit::When(Method(commMock, asyncRequest))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

        OperationID opId = TRI_NewTickServer();
        coordId = coordTransactionID;

        ClusterCommResult response;
        response.coordTransactionID = coordTransactionID;
        response.operationID = opId;
        response.answer_code = rest::ResponseCode::OK;
        response.status = CL_COMM_RECEIVED;

        if (destination == "server:" + dbserver1) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&lCount, [](SimpleHttpResult*) {});
          leaderRes = response;
        } else if (destination == "server:" + dbserver2) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f1Count, [](SimpleHttpResult*) {});
          follower1Res = response;
        } else if (destination == "server:" + dbserver3) {
          response.status = CL_COMM_TIMEOUT;
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f2Count, [](SimpleHttpResult*) {});
          follower2Res = response;
        } else {
          EXPECT_TRUE(false);
        }

        return opId;
      });
  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
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
                    OperationID const operationID, ShardID const& shardID) {
        // We need to abort this trx
        EXPECT_TRUE(coordTransactionID == coordId);
        // For all operations and shards
        EXPECT_TRUE(operationID == 0);
        EXPECT_TRUE(shardID == "");
      });

  {  // It should use the leader and the other one
    VPackBuilder resultBuilder;
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, largerFollowerCount);
  }

  {  // It should not call drop
    fakeit::Verify(Method(commMock, drop)).Exactly(0);
  }
}

TEST_F(ShardDistributionReporterTest,
       testing_collection_distribution_for_database_an_unhealthy_cluster_no_follower_responds) {
  shards->emplace(s1, std::vector<ServerID>{dbserver1, dbserver2, dbserver3});

  col->setShardMap(shards);

  currentShards.emplace(s1, std::vector<ServerID>{dbserver1});

  allCollections.emplace_back(
      std::shared_ptr<LogicalCollection>(col.get(), [](LogicalCollection*) {}));

  fakeit::When(Method(infoCurrentMock, servers)).AlwaysDo([&](ShardID const& sid) {
    EXPECT_TRUE(currentShards.find(sid) != currentShards.end());
    return currentShards[sid];
  });

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
      .AlwaysDo([&]() {
        EXPECT_TRUE(false);
        return buildCountBody(largerFollowerCount);
      });

  fakeit::When(ConstOverloadedMethod(followerTwoCountMock, getBodyVelocyPack,
                                     std::shared_ptr<VPackBuilder>()))
      .AlwaysDo([&]() {
        EXPECT_TRUE(false);
        return buildCountBody(smallerFollowerCount);
      });

  ClusterCommResult leaderRes;
  ClusterCommResult follower1Res;
  ClusterCommResult follower2Res;
  bool returnedFirstFollower = false;

  CoordTransactionID coordId = 0;
  // Mocking the ClusterComm for count calls
  fakeit::When(Method(commMock, asyncRequest))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID,
                    std::string const& destination, rest::RequestType reqtype,
                    std::string const& path, std::shared_ptr<std::string const> body,
                    std::unordered_map<std::string, std::string> const& headerFields,
                    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
                    bool singleRequest, ClusterCommTimeout initTimeout) -> OperationID {
        EXPECT_TRUE(path == "/_db/UnitTestDB/_api/collection/" + s1 + "/count");

        OperationID opId = TRI_NewTickServer();
        coordId = coordTransactionID;

        ClusterCommResult response;
        response.coordTransactionID = coordTransactionID;
        response.operationID = opId;
        response.answer_code = rest::ResponseCode::OK;
        response.status = CL_COMM_RECEIVED;

        if (destination == "server:" + dbserver1) {
          response.result =
              std::shared_ptr<SimpleHttpResult>(&lCount, [](SimpleHttpResult*) {});
          leaderRes = response;
        } else if (destination == "server:" + dbserver2) {
          response.status = CL_COMM_TIMEOUT;
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f1Count, [](SimpleHttpResult*) {});
          follower1Res = response;
        } else if (destination == "server:" + dbserver3) {
          response.status = CL_COMM_TIMEOUT;
          response.result =
              std::shared_ptr<SimpleHttpResult>(&f2Count, [](SimpleHttpResult*) {});
          follower2Res = response;
        } else {
          EXPECT_TRUE(false);
        }

        return opId;
      });
  fakeit::When(Method(commMock, wait))
      .AlwaysDo([&](CoordTransactionID const coordTransactionID, OperationID const operationID,
                    ShardID const& shardID, ClusterCommTimeout timeout) {
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
                    OperationID const operationID, ShardID const& shardID) {
        // We need to abort this trx
        EXPECT_TRUE(coordTransactionID == coordId);
        // For all operations and shards
        EXPECT_TRUE(operationID == 0);
        EXPECT_TRUE(shardID == "");
      });

  {  // It should use the leader and the default current of 0
    VPackBuilder resultBuilder;
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, 0);
  }

  {  // It should not call drop
    fakeit::Verify(Method(commMock, drop)).Exactly(0);
  }
}
