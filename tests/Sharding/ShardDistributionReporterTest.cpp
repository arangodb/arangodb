////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <queue>
#include <thread>

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardDistributionReporter.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::cluster;
using namespace arangodb::httpclient;

static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();

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
  ASSERT_EQ(total.getNumber<uint64_t>(), testTotal);

  VPackSlice current = progress.get("current");
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(current.getNumber<uint64_t>(), testCurrent);
}

static std::unique_ptr<fuerte::Response> generateCountResponse(uint64_t count) {
  VPackBuffer<uint8_t> responseBuffer;
  VPackBuilder builder(responseBuffer);
  builder.openObject();
  builder.add("count", VPackValue(count));
  builder.close();

  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  std::unique_ptr<fuerte::Response> response(new fuerte::Response(std::move(header)));
  response->setPayload(std::move(responseBuffer), /*offset*/ 0);

  TRI_ASSERT(!response->slices().empty());

  return response;
}

class ShardDistributionReporterTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:
  arangodb::application_features::ApplicationServer server;
  StorageEngineMock engine;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

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

  ShardDistributionReporterTest()
      : server(nullptr, nullptr),
        engine(server),
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
        shards(std::make_shared<ShardMap>()) {
    aliases[dbserver1] = dbserver1short;
    aliases[dbserver2] = dbserver2short;
    aliases[dbserver3] = dbserver3short;

    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(),
                          false);  // required for TRI_vocbase_t::dropCollection(...)
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector, false);
    selector.setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(),
                          false);  // required for TRI_vocbase_t instantiation

    for (auto& f : features) {
      f.first.prepare();
    }

    vocbase = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                              testDBInfo(server));
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
      f.first.unprepare();
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

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& opts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqType == fuerte::RestVerb::Get);
    EXPECT_TRUE(headerFields.empty());
    // This feature has at most 2s to do its job
    // otherwise default values will be returned
    EXPECT_TRUE(opts.timeout.count() <= 2.0);

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    // '/_db/UnitTestDB/_api/collection/' + shard.shard + '/count'
    if (destination == "server:" + dbserver1) {
      // off-sync follows s2,s3
      if (opts.database == "UnitTestDB" && path == "/_api/collection/" + s2 + "/count") {
        response.response = generateCountResponse(shard2LowFollowerCount);
      } else {
        EXPECT_TRUE(opts.database == "UnitTestDB");
        EXPECT_TRUE(path == "/_api/collection/" + s3 + "/count");
        response.response = generateCountResponse(shard3FollowerCount);
      }
    } else if (destination == "server:" + dbserver2) {
      EXPECT_TRUE(opts.database == "UnitTestDB");
      // Leads s2
      EXPECT_TRUE(path == "/_api/collection/" + s2 + "/count");
      response.response = generateCountResponse(shard2LeaderCount);
    } else if (destination == "server:" + dbserver3) {
      // Leads s3
      // off-sync follows s2
      if (opts.database == "UnitTestDB" && path == "/_api/collection/" + s2 + "/count") {
        response.response = generateCountResponse(shard2HighFollowerCount);
      } else {
        EXPECT_TRUE(opts.database == "UnitTestDB");
        EXPECT_TRUE(path == "/_api/collection/" + s3 + "/count");
        response.response = generateCountResponse(shard3LeaderCount);
      }
    } else {
      // Unknown Server!!
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // testing distribution for database
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
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
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
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
            ASSERT_EQ(total.getNumber<uint64_t>(), shard2LeaderCount);

            VPackSlice current = progress.get("current");
            ASSERT_TRUE(current.isNumber());
            ASSERT_EQ(current.getNumber<uint64_t>(), shard2LowFollowerCount);
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

  uint64_t leaderCount = 1337;
  uint64_t smallerFollowerCount = 456;
  uint64_t largerFollowerCount = 1111;

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.response = generateCountResponse(leaderCount);
    } else if (destination == "server:" + dbserver2) {
      response.response = generateCountResponse(largerFollowerCount);
    } else if (destination == "server:" + dbserver3) {
      response.response = generateCountResponse(smallerFollowerCount);
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {    // testing collection distribution for database
    {  // Both followers have a smaller amount of documents
      smallerFollowerCount = 456;
      largerFollowerCount = 1111;

      {  // The minimum should be reported
        VPackBuilder resultBuilder;
        ShardDistributionReporter testee(&ci, sender);
        testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

        VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, smallerFollowerCount);
      }
    }

    {  // Both followers have a larger amount of documents
      smallerFollowerCount = 1987;
      largerFollowerCount = 2345;

      {  // The maximum should be reported
        VPackBuilder resultBuilder;
        ShardDistributionReporter testee(&ci, sender);
        testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);

        VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, largerFollowerCount);
      }
    }

    {  // one follower has more and one has less documents
      smallerFollowerCount = 456;
      largerFollowerCount = 2345;

      {  // The lesser should be reported
        VPackBuilder resultBuilder;
        ShardDistributionReporter testee(&ci, sender);
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

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.error = fuerte::Error::RequestTimeout;
    } else if (destination == "server:" + dbserver2) {
    } else if (destination == "server:" + dbserver3) {
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // It should use the defaults total: 1 current: 0
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VerifyAttributes(resultBuilder.slice(), colName, s1);
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
  uint64_t largerFollowerCount = 1111;

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.response = generateCountResponse(leaderCount);
    } else if (destination == "server:" + dbserver2) {
      response.response = generateCountResponse(largerFollowerCount);
    } else if (destination == "server:" + dbserver3) {
      response.error = fuerte::Error::RequestTimeout;
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // It should use the leader and the other one
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VerifyAttributes(resultBuilder.slice(), colName, s1);
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

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.response = generateCountResponse(leaderCount);
    } else if (destination == "server:" + dbserver2) {
      response.error = fuerte::Error::RequestTimeout;
    } else if (destination == "server:" + dbserver3) {
      response.error = fuerte::Error::RequestTimeout;
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // It should use the leader
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
    testee.getDistributionForDatabase(dbname, resultBuilder);
    VerifyAttributes(resultBuilder.slice(), colName, s1);
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

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.error = fuerte::Error::RequestTimeout;
    } else if (destination == "server:" + dbserver2) {
    } else if (destination == "server:" + dbserver3) {
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // It should use the defaults total: 1 current: 0
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VerifyNumbers(resultBuilder.slice(), colName, s1, 1, 0);
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
  uint64_t largerFollowerCount = 1111;

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.response = generateCountResponse(leaderCount);
    } else if (destination == "server:" + dbserver2) {
      response.response = generateCountResponse(largerFollowerCount);
    } else if (destination == "server:" + dbserver3) {
      response.error = fuerte::Error::RequestTimeout;
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // It should use the leader and the other one
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, largerFollowerCount);
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

  // Mocking sendRequest for count calls
  auto sender = [&, this](network::DestinationId const& destination,
                          arangodb::fuerte::RestVerb reqType, std::string const& path,
                          velocypack::Buffer<uint8_t> body,
                          network::RequestOptions const& reqOpts,
                          network::Headers headerFields) -> network::FutureRes {
    EXPECT_TRUE(reqOpts.database == "UnitTestDB");
    EXPECT_TRUE(path == "/_api/collection/" + s1 + "/count");

    network::Response response;
    response.destination = destination;
    response.error = fuerte::Error::NoError;

    if (destination == "server:" + dbserver1) {
      response.response = generateCountResponse(leaderCount);
    } else if (destination == "server:" + dbserver2) {
      response.error = fuerte::Error::RequestTimeout;
    } else if (destination == "server:" + dbserver3) {
      response.error = fuerte::Error::RequestTimeout;
    } else {
      EXPECT_TRUE(false);
    }

    return futures::makeFuture(std::move(response));
  };

  {  // It should use the leader and the default current of 0
    VPackBuilder resultBuilder;
    ShardDistributionReporter testee(&ci, sender);
    testee.getCollectionDistributionForDatabase(dbname, colName, resultBuilder);
    VerifyNumbers(resultBuilder.slice(), colName, s1, leaderCount, 0);
  }
}
